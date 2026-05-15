#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// Error aggregation buffer (todo.md #250).
//
// Sentry-style "collect errors and ship to a central server" is the goal,
// but picking a vendor (self-host vs SaaS) and wiring an HTTP egress
// path are deployment-level decisions that don't belong in the engine.
// What DOES belong here is an in-process aggregator: every error site
// (the unified #70/#125 hook, the #105 crash handler, MessageLimiter
// rejections, #236 illegal-packet counter trips) gets a one-line API
// to record an event; downstream shippers — Prometheus exporter, a
// future HTTP push, a SIGUSR2 dump-to-file — pull from a single queue.
//
// This decouples "where do errors get recorded?" (everywhere — easy)
// from "where do they get shipped?" (one place — under deployment
// control). The same buffer also serves as the source of truth for
// "what happened in the last N minutes?" when an on-call grabs a node
// to triage.
//
// Threading: the buffer is shared across muduo's EventLoop threads, the
// Kafka consumer pool, and the proto-gen worker. A small mutex protects
// the deque; the contention point is one push per error (rare by design)
// vs the legal-message throughput path, so the lock is well outside any
// hot path.
//
// Capacity: bounded ring (default 1024 events). When full, the OLDEST
// event is dropped — recent errors are usually more interesting for
// triage than old ones, and dropping silently keeps the recorder
// always non-blocking. A monotonic dropped-count exposes the
// drop-pressure to monitoring.

namespace error_reporter {

struct Event {
    int64_t wall_ms{0};        // std::chrono::system_clock millis since epoch
    uint32_t error_code{0};     // domain-specific; 0 = "no code, just text"
    std::string tag;            // short label, e.g. "AssignGate", "Logout"
    std::string message;        // human-readable detail
};

class Buffer {
public:
    explicit Buffer(size_t capacity = 1024) : capacity_(capacity) {}

    // Record one event. Allocates only inside the lock (mutex + deque
    // push); call sites are expected to be on the rare error path, so
    // the cost is amortized below noise.
    void Record(uint32_t error_code, std::string_view tag, std::string_view message)
    {
        Event e;
        e.wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        e.error_code = error_code;
        e.tag.assign(tag);
        e.message.assign(message);

        std::lock_guard<std::mutex> lock(mutex_);
        if (buffer_.size() >= capacity_) {
            buffer_.pop_front();
            ++dropped_count_;
        }
        buffer_.push_back(std::move(e));
        ++total_count_;
    }

    // Move out all currently buffered events. Atomically clears the
    // internal buffer; subsequent Record() calls start from empty.
    // Intended for periodic shippers (metric exporter, HTTP push loop).
    std::vector<Event> Drain()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Event> out;
        out.reserve(buffer_.size());
        for (auto& e : buffer_) {
            out.push_back(std::move(e));
        }
        buffer_.clear();
        return out;
    }

    // Copy out without clearing — for SIGUSR2 dump-to-file or a "what
    // happened?" diagnostic RPC.
    std::vector<Event> Snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::vector<Event>(buffer_.begin(), buffer_.end());
    }

    // Counters for Prometheus-style observability. Both are monotonic
    // since process start.
    uint64_t TotalCount() const { return total_count_.load(std::memory_order_relaxed); }
    uint64_t DroppedCount() const { return dropped_count_.load(std::memory_order_relaxed); }
    size_t CurrentSize() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return buffer_.size();
    }

private:
    mutable std::mutex mutex_;
    std::deque<Event> buffer_;
    const size_t capacity_;
    std::atomic<uint64_t> total_count_{0};
    std::atomic<uint64_t> dropped_count_{0};
};

// Process-wide singleton. Acquire-then-Record from any thread. The
// instance is leaked at process exit (Meyers-singleton pattern keeps
// it alive until program termination, avoiding "report an error during
// shutdown teardown" lifetime races).
inline Buffer& Instance()
{
    static Buffer buffer;
    return buffer;
}

// One-liner for the call-site convenience. Equivalent to
// `Instance().Record(...)`.
inline void Record(uint32_t error_code, std::string_view tag, std::string_view message)
{
    Instance().Record(error_code, tag, message);
}

} // namespace error_reporter
