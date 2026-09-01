#include "job_system.h"
#include "nav_grid.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct Query {
    sim::Vec2 Start;
    sim::Vec2 Goal;
};

struct BatchResult {
    double Millis = 0.0;
    size_t Successes = 0;
    uint64_t ExpandedNodes = 0;
};

std::vector<sim::WallBounds> default_walls() {
    // Keep the benchmark's topology aligned with data/maps/default.json so
    // its path mix reflects the gameplay map rather than a toy maze.
    return {
        {{-15.07f, -41.51f}, {5.93f, -30.51f}},
        {{-32.51f, -6.31f}, {-27.51f, 9.69f}},
        {{-42.39f, -21.95f}, {28.61f, -19.95f}},
        {{-44.36f, 25.8f}, {-0.36f, 27.8f}},
        {{-42.84f, -49.85f}, {-40.84f, -10.85f}},
        {{-30.93f, -41.68f}, {-24.93f, -35.68f}},
        {{-13.5f, -21.72f}, {-11.5f, 26.28f}},
        {{-12.9f, 6.08f}, {45.1f, 8.08f}},
        {{39.69f, -47.41f}, {41.69f, -6.41f}},
        {{33.0f, -21.93f}, {45.0f, -19.93f}},
        {{6.53f, -11.39f}, {14.53f, -2.39f}},
        {{19.24f, 7.8f}, {21.24f, 48.8f}},
        {{-38.55f, 40.62f}, {19.45f, 42.62f}},
        {{4.22f, 18.48f}, {48.22f, 20.48f}},
        {{30.67f, 31.21f}, {38.67f, 40.21f}},
        {{28.42f, -41.84f}, {33.42f, -25.84f}},
    };
}

std::vector<Query> make_queries(size_t count) {
    std::vector<Query> queries;
    queries.reserve(count);
    std::mt19937 rng(1337);
    std::uniform_real_distribution<float> position(-44.0f, 44.0f);
    for (size_t i = 0; i < count; ++i) {
        queries.push_back({
            sim::Vec2{position(rng), position(rng)},
            sim::Vec2{position(rng), position(rng)},
        });
    }
    return queries;
}

std::vector<Query> make_long_queries() {
    return {
        {{-45.0f, -45.0f}, {45.0f, 45.0f}},
        {{-45.0f, 45.0f}, {45.0f, -45.0f}},
        {{-42.0f, -38.0f}, {40.0f, 42.0f}},
        {{-40.0f, 42.0f}, {42.0f, -40.0f}},
    };
}

sim::NavGrid build_grid(const std::vector<sim::WallBounds> &walls) {
    sim::NavGrid grid;
    grid.build(50.0f, walls, 0.5f, 0.5f);
    return grid;
}

BatchResult run_serial(const sim::NavGrid &grid, const std::vector<Query> &queries) {
    sim::PathScratch scratch;
    size_t successes = 0;
    uint64_t expanded = 0;
    const auto begin = Clock::now();
    for (const auto &query : queries) {
        uint32_t nodes = 0;
        const auto path = grid.find_path(query.Start, query.Goal, scratch, &nodes);
        if (!path.empty())
            ++successes;
        expanded += nodes;
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
        Clock::now() - begin
    );
    return {elapsed.count(), successes, expanded};
}

BatchResult run_parallel(
    const sim::NavGrid &grid,
    const std::vector<Query> &queries,
    size_t worker_count
) {
    sim::JobSystem jobs(worker_count);
    std::vector<sim::PathScratch> scratch(jobs.worker_count());
    const size_t cell_count = static_cast<size_t>(grid.Width) * grid.Height;
    for (auto &item : scratch)
        item.resize(cell_count);

    std::vector<uint8_t> success(queries.size(), 0);
    std::vector<uint32_t> expanded(queries.size(), 0);
    const auto begin = Clock::now();
    jobs.parallel_for(
        queries.size(),
        1,
        sim::JobPriority::Normal,
        [&](size_t index, size_t worker_index) {
            uint32_t nodes = 0;
            auto &item = scratch[worker_index % scratch.size()];
            const auto path = grid.find_path(
                queries[index].Start,
                queries[index].Goal,
                item,
                &nodes
            );
            success[index] = path.empty() ? 0 : 1;
            expanded[index] = nodes;
        }
    );
    jobs.wait_idle();
    const auto elapsed = std::chrono::duration<double, std::milli>(
        Clock::now() - begin
    );
    return {
        elapsed.count(),
        std::accumulate(success.begin(), success.end(), size_t{0}),
        std::accumulate(expanded.begin(), expanded.end(), uint64_t{0}),
    };
}

void print_batch(
    const char *name,
    const BatchResult &serial,
    const BatchResult &parallel,
    size_t workers,
    size_t request_count
) {
    const double speedup = parallel.Millis > 0.0
                               ? serial.Millis / parallel.Millis
                               : 0.0;
    std::printf(
        "[path_jobs] case=%s workers=%zu requests=%zu serial_ms=%.3f "
        "parallel_ms=%.3f speedup=%.2f serial_success=%zu "
        "parallel_success=%zu expanded=%llu\n",
        name,
        workers,
        request_count,
        serial.Millis,
        parallel.Millis,
        speedup,
        serial.Successes,
        parallel.Successes,
        static_cast<unsigned long long>(parallel.ExpandedNodes)
    );
}

} // namespace

int main(int argc, char **argv) {
    size_t worker_count = 4;
    size_t request_count = 64;
    if (argc > 1)
        worker_count = std::max<size_t>(1, std::strtoul(argv[1], nullptr, 10));
    if (argc > 2)
        request_count = std::max<size_t>(1, std::strtoul(argv[2], nullptr, 10));

    const auto walls = default_walls();
    const auto grid = build_grid(walls);
    const auto queries = make_queries(request_count);
    const auto long_queries = make_long_queries();

    // Warm both code paths so the measurements represent steady-state query
    // work rather than first-use allocation and instruction-cache effects.
    sim::PathScratch warm_scratch;
    grid.find_path(queries.front().Start, queries.front().Goal, warm_scratch);
    {
        sim::JobSystem warm_jobs(worker_count);
        warm_jobs.enqueue(sim::JobPriority::High, [&](size_t) {
            sim::PathScratch scratch;
            grid.find_path(queries.front().Start, queries.front().Goal, scratch);
        });
        warm_jobs.wait_idle();
    }

    const auto serial = run_serial(grid, queries);
    const auto parallel = run_parallel(grid, queries, worker_count);
    print_batch("default_64", serial, parallel, worker_count, queries.size());

    const auto long_serial = run_serial(grid, long_queries);
    const auto long_parallel = run_parallel(grid, long_queries, worker_count);
    print_batch(
        "long_distance",
        long_serial,
        long_parallel,
        worker_count,
        long_queries.size()
    );

    auto blocked_walls = walls;
    blocked_walls.push_back({sim::Vec2{-1.0f, -49.0f}, sim::Vec2{1.0f, 49.0f}});
    const auto blocked_grid = build_grid(blocked_walls);
    const std::vector<Query> unreachable = {
        {{-40.0f, 0.0f}, {40.0f, 0.0f}},
        {{-35.0f, 20.0f}, {35.0f, -20.0f}},
    };
    const auto blocked_serial = run_serial(blocked_grid, unreachable);
    const auto blocked_parallel =
        run_parallel(blocked_grid, unreachable, worker_count);
    print_batch(
        "unreachable",
        blocked_serial,
        blocked_parallel,
        worker_count,
        unreachable.size()
    );

    std::printf(
        "[sim_perf] grid=%dx%d cells=%d workers=%zu requests=%zu\n",
        grid.Width,
        grid.Height,
        grid.Width * grid.Height,
        worker_count,
        request_count
    );
    return 0;
}
