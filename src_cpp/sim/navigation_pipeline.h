#pragma once

#include "components.h"
#include "job_system.h"
#include "nav_grid.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace sim {

enum class NavigationResultStatus : uint8_t {
    Success = 0,
    NoPath = 1,
};

struct NavigationRequest {
    uint64_t RequestId = 0;
    entt::entity Requester = entt::null;
    PathQueryChannel Channel = PathQueryChannel::Movement;
    PathQueryPriority Priority = PathQueryPriority::Normal;
    PathQueryDestination Destination = PathQueryDestination::Ground;
    entt::entity TargetEntity = entt::null;
    uint64_t IntentVersion = 0;
    Vec2 Start{0.0f};
    Vec2 Goal{0.0f};
    uint32_t NavRevision = 0;
    int SubmitTick = 0;
};

struct NavigationResult {
    NavigationRequest Request;
    NavigationResultStatus Status = NavigationResultStatus::NoPath;
    std::vector<Vec2> Waypoints;
    uint32_t ExpandedNodes = 0;
    uint64_t SearchMicros = 0;
};

struct NavigationMetrics {
    uint64_t Submitted = 0;
    uint64_t Rejected = 0;
    uint64_t Completed = 0;
    uint64_t NoPath = 0;
    uint64_t Merged = 0;
    uint64_t Stale = 0;
    uint64_t Late = 0;
    uint64_t ResultAgeTicks = 0;
    uint64_t ResultAgeMaxTicks = 0;
    uint64_t ExpandedNodes = 0;
    uint64_t WorkerMicros = 0;
    size_t Pending = 0;
};

// Owns the async boundary around navigation. Requests and results are value
// types; the worker side never receives an EnTT registry or a gameplay
// callback. Completed results are polled by the main thread and committed by
// the consuming system after validating the request version.
class NavigationPipeline {
  public:
    explicit NavigationPipeline(JobSystem &jobs);
    ~NavigationPipeline();

    NavigationPipeline(const NavigationPipeline &) = delete;
    NavigationPipeline &operator=(const NavigationPipeline &) = delete;

    void set_nav_grid(std::shared_ptr<const NavGrid> nav_grid);
    void reset_worker_scratch();
    void reset_metrics();
    void clear();

    bool submit(NavigationRequest request, uint64_t &request_id);
    void poll_completed(int current_tick, std::vector<NavigationResult> &out);
    void note_merged_request() {
        _merged.fetch_add(1, std::memory_order_relaxed);
    }
    void note_stale_result() {
        _stale.fetch_add(1, std::memory_order_relaxed);
    }

    const std::shared_ptr<const NavGrid> &nav_grid() const { return _nav_grid; }
    NavigationMetrics metrics() const;

  private:
    struct ResultSlot {
        NavigationRequest request;
        NavigationResultStatus status = NavigationResultStatus::NoPath;
        std::vector<Vec2> waypoints;
        uint32_t expanded_nodes = 0;
        uint64_t search_micros = 0;
        std::atomic<bool> ready = false;
    };

    static JobPriority to_job_priority(PathQueryPriority priority);

    JobSystem &_jobs;
    std::shared_ptr<const NavGrid> _nav_grid;
    std::vector<PathScratch> _scratch;
    std::vector<std::shared_ptr<ResultSlot>> _pending;
    uint64_t _next_request_id = 1;

    std::atomic<uint64_t> _submitted = 0;
    std::atomic<uint64_t> _rejected = 0;
    std::atomic<uint64_t> _completed = 0;
    std::atomic<uint64_t> _no_path = 0;
    std::atomic<uint64_t> _merged = 0;
    std::atomic<uint64_t> _stale = 0;
    std::atomic<uint64_t> _late = 0;
    std::atomic<uint64_t> _result_age_ticks = 0;
    std::atomic<uint64_t> _result_age_max_ticks = 0;
    std::atomic<uint64_t> _expanded_nodes = 0;
    std::atomic<uint64_t> _worker_micros = 0;
};

} // namespace sim
