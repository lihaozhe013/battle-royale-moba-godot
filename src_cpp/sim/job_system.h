#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace sim {

enum class JobPriority : uint8_t {
    High = 0,
    Normal = 1,
    Low = 2,
};

// A small, reusable simulation-side worker pool. Jobs receive the stable
// worker index so systems can keep per-worker scratch memory without sharing
// mutable state between tasks.
class JobSystem {
  public:
    using Job = std::function<void(size_t worker_index)>;

    explicit JobSystem(size_t worker_count = 0, size_t max_queued_jobs = 4096);
    ~JobSystem();

    JobSystem(const JobSystem &) = delete;
    JobSystem &operator=(const JobSystem &) = delete;

    bool enqueue(JobPriority priority, Job job);

    template <typename Function>
    size_t parallel_for(
        size_t count,
        size_t grain_size,
        JobPriority priority,
        Function function
    ) {
        if (count == 0 || grain_size == 0)
            return 0;

        size_t submitted = 0;
        for (size_t begin = 0; begin < count; begin += grain_size) {
            const size_t end = (begin + grain_size < count)
                                   ? begin + grain_size
                                   : count;
            Job job = [function, begin, end](size_t worker_index) {
                for (size_t index = begin; index < end; ++index)
                    function(index, worker_index);
            };
            if (enqueue(priority, std::move(job)))
                ++submitted;
        }
        return submitted;
    }

    void wait_idle();
    void reconfigure(size_t worker_count);
    void shutdown();

    size_t worker_count() const { return _workers.size(); }
    size_t queued_jobs() const;
    size_t peak_queued_jobs() const;
    size_t active_jobs() const;

  private:
    struct QueuedJob {
        JobPriority priority;
        Job function;
    };

    void worker_loop(size_t worker_index);
    void start_workers(size_t worker_count);
    bool has_jobs_locked() const;
    size_t queued_jobs_locked() const;
    QueuedJob pop_job_locked();

    const size_t _max_queued_jobs;
    mutable std::mutex _mutex;
    std::condition_variable _wake;
    std::condition_variable _idle;
    std::array<std::deque<QueuedJob>, 3> _queues;
    std::vector<std::jthread> _workers;
    size_t _active_jobs = 0;
    size_t _peak_queued_jobs = 0;
    size_t _priority_cursor = 0;
    bool _stopping = false;
};

} // namespace sim
