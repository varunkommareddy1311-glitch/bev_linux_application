#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace bev {

/// Coarse-grained lifecycle state, exposed for logging, systemd
/// notification (sd_notify) integration, and future watchdog integration.
enum class HealthState : std::uint8_t {
    Init,
    Running,
    Error,
    Recovering,
    Shutdown
};

/// Lightweight, allocation-free health/heartbeat tracker for one processing
/// pipeline (one camera, or the overall app). Deliberately simple per the
/// architecture requirements: counters + a state flag, no threads, no
/// external dependencies. Safe to update from a single processing thread;
/// reads (e.g. from a future status-reporting thread) use atomics.
class HealthMonitor {
public:
    HealthMonitor() = default;

    void setState(HealthState state) noexcept { state_.store(state, std::memory_order_relaxed); }
    [[nodiscard]] HealthState state() const noexcept { return state_.load(std::memory_order_relaxed); }

    /// Call once per frame that successfully arrives at the pipeline entry.
    void onFrameArrived() noexcept {
        framesArrived_.fetch_add(1, std::memory_order_relaxed);
        lastHeartbeatUs_.store(nowUs(), std::memory_order_relaxed);
    }

    /// Call when a frame is dropped/rejected (e.g. failed validation).
    void onFrameDropped() noexcept { framesDropped_.fetch_add(1, std::memory_order_relaxed); }

    /// Call after a frame finishes processing, with the elapsed wall time,
    /// to maintain a simple running latency sample (last value, not an
    /// average - keep it allocation-free and cheap to read).
    void onFrameProcessed(std::chrono::microseconds latency) noexcept {
        framesProcessed_.fetch_add(1, std::memory_order_relaxed);
        lastLatencyUs_.store(static_cast<std::int64_t>(latency.count()), std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint64_t framesArrived() const noexcept { return framesArrived_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t framesDropped() const noexcept { return framesDropped_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t framesProcessed() const noexcept { return framesProcessed_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::int64_t lastLatencyUs() const noexcept { return lastLatencyUs_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::int64_t lastHeartbeatUs() const noexcept { return lastHeartbeatUs_.load(std::memory_order_relaxed); }

    /// True if no heartbeat has been recorded within `timeoutUs`. Intended
    /// to be polled by a future watchdog/systemd-notify integration - this
    /// class intentionally does not spawn its own timer thread.
    [[nodiscard]] bool isStale(std::int64_t timeoutUs) const noexcept {
        return (nowUs() - lastHeartbeatUs()) > timeoutUs;
    }

private:
    static std::int64_t nowUs() noexcept {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }

    std::atomic<HealthState> state_{HealthState::Init};
    std::atomic<std::uint64_t> framesArrived_{0};
    std::atomic<std::uint64_t> framesDropped_{0};
    std::atomic<std::uint64_t> framesProcessed_{0};
    std::atomic<std::int64_t> lastLatencyUs_{0};
    std::atomic<std::int64_t> lastHeartbeatUs_{0};
};

} // namespace bev
