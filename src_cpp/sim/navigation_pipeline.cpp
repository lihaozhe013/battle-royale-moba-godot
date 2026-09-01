#include "navigation_pipeline.h"

#include <algorithm>
#include <chrono>

namespace sim {

NavigationPipeline::NavigationPipeline(JobSystem &jobs) : _jobs(jobs) {
    _scratch.resize(_jobs.worker_count());
}

NavigationPipeline::~NavigationPipeline() {
    clear();
}

void NavigationPipeline::reset_metrics() {
    _next_request_id = 1;
    _submitted.store(0, std::memory_order_relaxed);
    _rejected.store(0, std::memory_order_relaxed);
    _completed.store(0, std::memory_order_relaxed);
    _no_path.store(0, std::memory_order_relaxed);
    _merged.store(0, std::memory_order_relaxed);
    _stale.store(0, std::memory_order_relaxed);
    _late.store(0, std::memory_order_relaxed);
    _result_age_ticks.store(0, std::memory_order_relaxed);
    _result_age_max_ticks.store(0, std::memory_order_relaxed);
    _expanded_nodes.store(0, std::memory_order_relaxed);
    _worker_micros.store(0, std::memory_order_relaxed);
}

void NavigationPipeline::set_nav_grid(std::shared_ptr<const NavGrid> nav_grid) {
    clear();
    _nav_grid = std::move(nav_grid);
    reset_worker_scratch();
}

void NavigationPipeline::reset_worker_scratch() {
    _jobs.wait_idle();
    _scratch.clear();
    _scratch.resize(_jobs.worker_count());
    for (auto &scratch : _scratch)
        scratch = PathScratch{};
    if (!_nav_grid)
        return;
    for (auto &scratch : _scratch)
        scratch.resize(static_cast<size_t>(_nav_grid->Width) * _nav_grid->Height);
}

void NavigationPipeline::clear() {
    _jobs.wait_idle();
    _pending.clear();
}

JobPriority
NavigationPipeline::to_job_priority(PathQueryPriority priority) {
    switch (priority) {
    case PathQueryPriority::High:
        return JobPriority::High;
    case PathQueryPriority::Low:
        return JobPriority::Low;
    case PathQueryPriority::Normal:
    default:
        return JobPriority::Normal;
    }
}

bool NavigationPipeline::submit(
    NavigationRequest request, uint64_t &request_id
) {
    request_id = _next_request_id++;
    request.RequestId = request_id;
    if (!_nav_grid || _nav_grid->Width <= 0 || _nav_grid->Height <= 0) {
        _rejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (request.NavRevision == 0)
        request.NavRevision = _nav_grid->Revision;

    auto slot = std::make_shared<ResultSlot>();
    slot->request = request;
    auto nav_grid = _nav_grid;
    const bool accepted = _jobs.enqueue(
        to_job_priority(request.Priority),
        [this, slot, nav_grid](size_t worker_index) noexcept {
            const auto begin = std::chrono::steady_clock::now();
            PathScratch &scratch =
                _scratch[worker_index % std::max<size_t>(1, _scratch.size())];
            uint32_t expanded_nodes = 0;
            auto raw = nav_grid->find_path(
                slot->request.Start,
                slot->request.Goal,
                scratch,
                &expanded_nodes
            );

            slot->status = raw.empty() ? NavigationResultStatus::NoPath
                                       : NavigationResultStatus::Success;
            if (!raw.empty()) {
                // Consumers start from the current position at apply time;
                // do not feed the captured start point back into movement.
                if (raw.size() > 1)
                    raw.erase(raw.begin());
            }
            slot->waypoints = std::move(raw);
            slot->expanded_nodes = expanded_nodes;
            slot->search_micros = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - begin
                )
                    .count()
            );
            _expanded_nodes.fetch_add(expanded_nodes, std::memory_order_relaxed);
            _worker_micros.fetch_add(
                slot->search_micros, std::memory_order_relaxed
            );
            slot->ready.store(true, std::memory_order_release);
        }
    );
    if (!accepted) {
        _rejected.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    _pending.push_back(std::move(slot));
    _submitted.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void NavigationPipeline::poll_completed(
    int current_tick, std::vector<NavigationResult> &out
) {
    out.clear();
    for (auto it = _pending.begin(); it != _pending.end();) {
        const auto &slot = *it;
        if (!slot->ready.load(std::memory_order_acquire) ||
            current_tick < slot->request.SubmitTick + 1) {
            ++it;
            continue;
        }

        NavigationResult result;
        result.Request = slot->request;
        result.Status = slot->status;
        result.Waypoints = std::move(slot->waypoints);
        result.ExpandedNodes = slot->expanded_nodes;
        result.SearchMicros = slot->search_micros;
        out.push_back(std::move(result));
        const uint64_t age = static_cast<uint64_t>(std::max(
            0, current_tick - slot->request.SubmitTick
        ));
        _result_age_ticks.fetch_add(age, std::memory_order_relaxed);
        uint64_t previous_max = _result_age_max_ticks.load(
            std::memory_order_relaxed
        );
        while (age > previous_max &&
               !_result_age_max_ticks.compare_exchange_weak(
                   previous_max,
                   age,
                   std::memory_order_relaxed,
                   std::memory_order_relaxed
               )) {
        }
        if (age > 1)
            _late.fetch_add(1, std::memory_order_relaxed);
        if (slot->status == NavigationResultStatus::NoPath)
            _no_path.fetch_add(1, std::memory_order_relaxed);
        it = _pending.erase(it);
        _completed.fetch_add(1, std::memory_order_relaxed);
    }
}

NavigationMetrics NavigationPipeline::metrics() const {
    return {
        _submitted.load(std::memory_order_relaxed),
        _rejected.load(std::memory_order_relaxed),
        _completed.load(std::memory_order_relaxed),
        _no_path.load(std::memory_order_relaxed),
        _merged.load(std::memory_order_relaxed),
        _stale.load(std::memory_order_relaxed),
        _late.load(std::memory_order_relaxed),
        _result_age_ticks.load(std::memory_order_relaxed),
        _result_age_max_ticks.load(std::memory_order_relaxed),
        _expanded_nodes.load(std::memory_order_relaxed),
        _worker_micros.load(std::memory_order_relaxed),
        _pending.size(),
    };
}

} // namespace sim
