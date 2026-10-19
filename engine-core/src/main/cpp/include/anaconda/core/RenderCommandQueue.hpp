// ============================================================================
//  anaconda/core/RenderCommandQueue.hpp
//
//  Deferred work that MUST run on the GL thread — the Phase-2 extension of
//  the threading model (docs/ARCHITECTURE.md §3).
//
//  The asset pipeline decodes images on IO threads; GLES calls are only legal
//  on the GL thread with a live context. Producers push closures here; the
//  Engine drains the queue at the TOP of every frame, before touching the
//  world (see Engine::renderFrame). Draining swaps the whole batch under the
//  queue mutex, so producers never block behind a frame, and commands execute
//  while the world mutex is already held — no nested-lock ordering hazards.
//
//  Header-only, no GL dependency → host-testable.
// ============================================================================
#pragma once

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace anaconda {

class RenderCommandQueue {
public:
    using Command = std::function<void()>;

    /// Any thread. Ownership of captured data (e.g. decoded pixels) moves in.
    void push(Command command) {
        if (!command) return;
        std::lock_guard<std::mutex> lock(mutex_);
        pending_.push_back(std::move(command));
    }

    /// GL thread, once per frame. Returns the batch; the caller executes it.
    /// Swap-drain keeps the queue mutex hold time constant regardless of
    /// consumer work.
    [[nodiscard]] std::vector<Command> drain() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Command> batch;
        batch.swap(pending_);
        return batch;
    }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return pending_.size();
    }

    [[nodiscard]] bool empty() const { return size() == 0; }

private:
    mutable std::mutex mutex_;
    std::vector<Command> pending_;
};

}  // namespace anaconda
