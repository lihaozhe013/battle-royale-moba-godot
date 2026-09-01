#include "job_system.h"

#include <algorithm>

namespace sim {

JobSystem::JobSystem(size_t worker_count, size_t max_queued_jobs)
    : _max_queued_jobs(std::max<size_t>(1, max_queued_jobs)) {
    start_workers(worker_count);
}

void JobSystem::start_workers(size_t worker_count) {
    if (worker_count == 0) {
        const unsigned int hardware = std::thread::hardware_concurrency();
        worker_count = hardware > 1 ? static_cast<size_t>(hardware - 1) : 1;
        worker_count = std::clamp<size_t>(worker_count, 1, 8);
    }
    _workers.reserve(worker_count);
    for (size_t index = 0; index < worker_count; ++index) {
        _workers.emplace_back(
            [this, index](std::stop_token) { worker_loop(index); }
        );
    }
}

JobSystem::~JobSystem() {
    shutdown();
}

bool JobSystem::enqueue(JobPriority priority, Job job) {
    if (!job)
        return false;

    const size_t queue_index = static_cast<size_t>(priority);
    if (queue_index >= _queues.size())
        return false;

    {
        std::lock_guard lock(_mutex);
        if (_stopping || queued_jobs_locked() >= _max_queued_jobs)
            return false;
        _queues[queue_index].push_back({priority, std::move(job)});
        _peak_queued_jobs = std::max(_peak_queued_jobs, queued_jobs_locked());
    }
    _wake.notify_one();
    return true;
}

void JobSystem::wait_idle() {
    std::unique_lock lock(_mutex);
    _idle.wait(lock, [this] {
        return _active_jobs == 0 && !has_jobs_locked();
    });
}

void JobSystem::reconfigure(size_t worker_count) {
    shutdown();
    {
        std::lock_guard lock(_mutex);
        _stopping = false;
        _priority_cursor = 0;
        _peak_queued_jobs = 0;
    }
    start_workers(worker_count);
}

void JobSystem::shutdown() {
    {
        std::unique_lock lock(_mutex);
        if (_stopping && _workers.empty())
            return;
        _idle.wait(lock, [this] {
            return _active_jobs == 0 && !has_jobs_locked();
        });
        _stopping = true;
    }
    _wake.notify_all();
    for (auto &worker : _workers)
        worker.request_stop();
    _workers.clear();
}

size_t JobSystem::queued_jobs() const {
    std::lock_guard lock(_mutex);
    return queued_jobs_locked();
}

size_t JobSystem::peak_queued_jobs() const {
    std::lock_guard lock(_mutex);
    return _peak_queued_jobs;
}

size_t JobSystem::queued_jobs_locked() const {
    size_t total = 0;
    for (const auto &queue : _queues)
        total += queue.size();
    return total;
}

size_t JobSystem::active_jobs() const {
    std::lock_guard lock(_mutex);
    return _active_jobs;
}

bool JobSystem::has_jobs_locked() const {
    for (const auto &queue : _queues) {
        if (!queue.empty())
            return true;
    }
    return false;
}

JobSystem::QueuedJob JobSystem::pop_job_locked() {
    // Weighted 4:2:1 scheduling. If the preferred lane is empty, fall back
    // to the highest available priority so idle workers never wait on it.
    constexpr std::array<size_t, 7> schedule = {0, 0, 0, 0, 1, 1, 2};
    for (size_t attempt = 0; attempt < schedule.size(); ++attempt) {
        const size_t index = schedule[_priority_cursor++ % schedule.size()];
        if (!_queues[index].empty()) {
            QueuedJob job = std::move(_queues[index].front());
            _queues[index].pop_front();
            return job;
        }
    }

    for (size_t index = 0; index < _queues.size(); ++index) {
        if (!_queues[index].empty()) {
            QueuedJob job = std::move(_queues[index].front());
            _queues[index].pop_front();
            return job;
        }
    }
    return {};
}

void JobSystem::worker_loop(size_t worker_index) {
    for (;;) {
        QueuedJob queued;
        {
            std::unique_lock lock(_mutex);
            _wake.wait(lock, [this] { return _stopping || has_jobs_locked(); });
            if (_stopping && !has_jobs_locked())
                return;
            queued = pop_job_locked();
            ++_active_jobs;
        }

        // Job callbacks are required to be noexcept by convention. Keeping
        // the worker loop free of exception handling also supports the
        // project's -fno-exceptions native build.
        queued.function(worker_index);

        {
            std::lock_guard lock(_mutex);
            --_active_jobs;
            if (_active_jobs == 0 && !has_jobs_locked())
                _idle.notify_all();
        }
    }
}

} // namespace sim
