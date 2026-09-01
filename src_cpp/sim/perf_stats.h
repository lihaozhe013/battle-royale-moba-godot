#pragma once

#include "navigation_pipeline.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <vector>

namespace sim {

enum class PerfPhase : uint8_t {
    InputAi = 0,
    Actions = 1,
    Navigation = 2,
    MovementCollision = 3,
    Effects = 4,
    Snapshot = 5,
    Flush = 6,
    Count = 7,
};

constexpr size_t perf_phase_count = static_cast<size_t>(PerfPhase::Count);

struct PerfTimingStats {
    uint64_t SampleTicks = 0;
    double TickAverageMicros = 0.0;
    double TickP95Micros = 0.0;
    double TickP99Micros = 0.0;
    uint64_t TickMaxMicros = 0;
    std::array<double, perf_phase_count> PhaseAverageMicros{};
    std::array<double, perf_phase_count> PhaseP95Micros{};
    std::array<double, perf_phase_count> PhaseP99Micros{};
    std::array<uint64_t, perf_phase_count> PhaseMaxMicros{};
};

struct RuntimePerfStats {
    PerfTimingStats Timing;
    NavigationMetrics Navigation;
    size_t JobQueued = 0;
    size_t JobPeakQueued = 0;
    size_t JobActive = 0;
};

class PerfCollector {
  public:
    static constexpr size_t sample_window = 512;

    void reset() {
        _sample_index = 0;
        _sample_count = 0;
        _active_phase = -1;
        _current_phase_micros.fill(0);
        _tick_samples.fill(0);
        for (auto &samples : _phase_samples)
            samples.fill(0);
    }

    void begin_tick() {
        _tick_start = Clock::now();
        _current_phase_micros.fill(0);
        _active_phase = -1;
    }

    void begin_phase(PerfPhase phase) {
        end_phase();
        _active_phase = static_cast<int>(phase);
        _phase_start = Clock::now();
    }

    void end_phase() {
        if (_active_phase < 0)
            return;
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::microseconds>(Clock::now() - _phase_start);
        _current_phase_micros[static_cast<size_t>(_active_phase)] +=
            static_cast<uint64_t>(std::max<int64_t>(0, elapsed.count()));
        _active_phase = -1;
    }

    void end_tick() {
        end_phase();
        const auto elapsed = std::chrono::duration_cast<
            std::chrono::microseconds>(Clock::now() - _tick_start);
        _tick_samples[_sample_index] =
            static_cast<uint64_t>(std::max<int64_t>(0, elapsed.count()));
        for (size_t phase = 0; phase < perf_phase_count; ++phase)
            _phase_samples[phase][_sample_index] = _current_phase_micros[phase];
        _sample_index = (_sample_index + 1) % sample_window;
        _sample_count = std::min(sample_window, _sample_count + 1);
    }

    PerfTimingStats snapshot() const {
        PerfTimingStats out;
        out.SampleTicks = _sample_count;
        if (_sample_count == 0)
            return out;

        std::vector<uint64_t> values;
        values.reserve(_sample_count);
        for (size_t i = 0; i < _sample_count; ++i)
            values.push_back(_tick_samples[i]);
        out.TickAverageMicros = average(values);
        out.TickP95Micros = percentile(values, 0.95);
        out.TickP99Micros = percentile(values, 0.99);
        out.TickMaxMicros = *std::max_element(values.begin(), values.end());

        for (size_t phase = 0; phase < perf_phase_count; ++phase) {
            values.clear();
            for (size_t i = 0; i < _sample_count; ++i)
                values.push_back(_phase_samples[phase][i]);
            out.PhaseAverageMicros[phase] = average(values);
            out.PhaseP95Micros[phase] = percentile(values, 0.95);
            out.PhaseP99Micros[phase] = percentile(values, 0.99);
            out.PhaseMaxMicros[phase] =
                *std::max_element(values.begin(), values.end());
        }
        return out;
    }

  private:
    using Clock = std::chrono::steady_clock;

    static double average(const std::vector<uint64_t> &values) {
        uint64_t sum = 0;
        for (uint64_t value : values)
            sum += value;
        return static_cast<double>(sum) / values.size();
    }

    static double percentile(
        std::vector<uint64_t> values, double fraction
    ) {
        if (values.empty())
            return 0.0;
        std::sort(values.begin(), values.end());
        const size_t index = std::min(
            values.size() - 1,
            static_cast<size_t>(fraction * static_cast<double>(values.size() - 1))
        );
        return static_cast<double>(values[index]);
    }

    size_t _sample_index = 0;
    size_t _sample_count = 0;
    int _active_phase = -1;
    Clock::time_point _tick_start{};
    Clock::time_point _phase_start{};
    std::array<uint64_t, perf_phase_count> _current_phase_micros{};
    std::array<uint64_t, sample_window> _tick_samples{};
    std::array<
        std::array<uint64_t, sample_window>,
        perf_phase_count
    > _phase_samples{};
};

} // namespace sim
