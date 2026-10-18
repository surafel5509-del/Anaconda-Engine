// ============================================================================
//  anaconda/ecs/Ecs.hpp
//
//  A compact, high-performance Entity Component System (sparse-set flavor).
//
//  Design goals:
//    * Header-only, zero external dependencies (vendored forever).
//    * O(1) add/remove of components, cache-friendly dense iteration.
//    * Stale-handle safety: entities carry a generation counter, so a handle
//      held after destroy() fails validation instead of aliasing a recycled
//      slot.
//    * View<Ts...> iterates the SMALLEST matching pool and filters the rest —
//      never a full world scan.
//
//  Entity layout (32 bits):
//      [ 12 bits generation ][ 20 bits index ]
//      → up to 1,048,575 simultaneously alive entities, 4096 recycles per
//        slot before generation wrap (wrap collides only with the reserved
//        null handle after ~4 billion operations — treated as unreachable).
//
//  Usage:
//      anaconda::ecs::Registry world;
//      Entity e  = world.create();
//      auto&  t  = world.add(e, TransformComponent{});
//      world.view<TransformComponent, SpriteComponent>([&](Entity id, auto& tr, auto& sp) {
//          // hot loop over entities that have BOTH components
//      });
// ============================================================================
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "anaconda/core/Log.hpp"

namespace anaconda::ecs {

// ---------------------------------------------------------------------------
// Entity handle
// ---------------------------------------------------------------------------
using Entity = std::uint32_t;

inline constexpr std::uint32_t kEntityIndexBits = 20;
inline constexpr std::uint32_t kEntityIndexMask = (1u << kEntityIndexBits) - 1u;  // 0xFFFFF
inline constexpr std::uint32_t kEntityGenBits = 12;
inline constexpr std::uint32_t kEntityGenMask = (1u << kEntityGenBits) - 1u;      // 0xFFF
inline constexpr std::uint32_t kEntityGenShift = kEntityIndexBits;

/// Reserved "no entity" handle (max index + max generation — never allocated).
inline constexpr Entity kNullEntity = 0xFFFFFFFFu;

constexpr Entity makeEntity(std::uint32_t index, std::uint32_t generation) noexcept {
    return (generation << kEntityGenShift) | (index & kEntityIndexMask);
}
constexpr std::uint32_t entityIndex(Entity e) noexcept { return e & kEntityIndexMask; }
constexpr std::uint32_t entityGeneration(Entity e) noexcept {
    return (e >> kEntityGenShift) & kEntityGenMask;
}

// ---------------------------------------------------------------------------
// IComponentPool — type-erased surface the Registry and View need.
// ---------------------------------------------------------------------------
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual bool has(Entity e) const = 0;
    virtual void remove(Entity e) = 0;
    virtual std::size_t denseSize() const = 0;
    virtual Entity denseEntity(std::size_t denseIndex) const = 0;
};

// ---------------------------------------------------------------------------
// ComponentPool<T> — sparse set.
//
//   sparse_[entityIndex]  → dense slot + 1   (0 = "not present")
//   denseEntities_[i]     → Entity owning dense slot i
//   denseData_[i]         → component payload (struct-of-arrays friendly)
//
// Removal is swap-and-pop: O(1), keeps the dense arrays contiguous.
// ---------------------------------------------------------------------------
template <typename T>
class ComponentPool final : public IComponentPool {
public:
    bool has(Entity e) const override {
        const std::uint32_t idx = entityIndex(e);
        if (idx >= sparse_.size()) return false;
        const std::uint32_t dense = sparse_[idx];
        return dense != 0 && denseEntities_[dense - 1] == e;
    }

    /// Insert or overwrite. Returns the stored reference.
    T& insert(Entity e, T value) {
        const std::uint32_t idx = entityIndex(e);
        if (idx >= sparse_.size()) sparse_.resize(idx + 1, 0);

        if (sparse_[idx] != 0 && denseEntities_[sparse_[idx] - 1] == e) {
            T& slot = denseData_[sparse_[idx] - 1];
            slot = std::move(value);
            return slot;
        }
        denseEntities_.push_back(e);
        denseData_.push_back(std::move(value));
        sparse_[idx] = static_cast<std::uint32_t>(denseEntities_.size());  // dense+1
        return denseData_.back();
    }

    /// Precondition: has(e). Checked by ANA_ASSERT in debug builds.
    T& get(Entity e) {
        ANA_ASSERT(has(e), "ComponentPool::get on entity without component");
        return denseData_[sparse_[entityIndex(e)] - 1];
    }
    const T& get(Entity e) const {
        ANA_ASSERT(has(e), "ComponentPool::get on entity without component");
        return denseData_[sparse_[entityIndex(e)] - 1];
    }

    void remove(Entity e) override {
        if (!has(e)) return;
        const std::uint32_t idx = entityIndex(e);
        const std::uint32_t dense = sparse_[idx] - 1;
        const auto lastDense = static_cast<std::uint32_t>(denseData_.size() - 1);

        if (dense != lastDense) {
            // Swap the removed slot with the last one and fix its sparse entry.
            denseData_[dense] = std::move(denseData_[lastDense]);
            denseEntities_[dense] = denseEntities_[lastDense];
            sparse_[entityIndex(denseEntities_[dense])] = dense + 1;
        }
        denseData_.pop_back();
        denseEntities_.pop_back();
        sparse_[idx] = 0;
    }

    [[nodiscard]] std::size_t denseSize() const override { return denseData_.size(); }
    [[nodiscard]] Entity denseEntity(std::size_t i) const override { return denseEntities_[i]; }

    /// Direct slot access for systems that already hold a dense index.
    [[nodiscard]] T* tryGetDense(std::size_t i) {
        return i < denseData_.size() ? &denseData_[i] : nullptr;
    }

private:
    std::vector<std::uint32_t> sparse_;      // entity index → dense slot + 1
    std::vector<Entity> denseEntities_;      // dense slot  → entity
    std::vector<T> denseData_;               // dense slot  → component
};

// ---------------------------------------------------------------------------
// Registry — owns entities + every component pool.
// ---------------------------------------------------------------------------
class Registry {
public:
    // ---- entity lifecycle --------------------------------------------------
    Entity create() {
        std::uint32_t idx = 0;
        if (!freeList_.empty()) {
            idx = freeList_.back();
            freeList_.pop_back();
            ANA_ASSERT(!alive_[idx], "free list contained a live entity");
        } else {
            idx = static_cast<std::uint32_t>(generations_.size());
            generations_.push_back(0);
            alive_.push_back(false);
        }
        alive_[idx] = true;
        ++aliveCount_;
        return makeEntity(idx, generations_[idx]);
    }

    void destroy(Entity e) {
        if (!valid(e)) return;
        const std::uint32_t idx = entityIndex(e);
        for (auto& [_, pool] : pools_) pool->remove(e);   // strip all components
        alive_[idx] = false;
        generations_[idx] = (generations_[idx] + 1) & kEntityGenMask;
        freeList_.push_back(idx);
        --aliveCount_;
    }

    [[nodiscard]] bool valid(Entity e) const noexcept {
        if (e == kNullEntity) return false;
        const std::uint32_t idx = entityIndex(e);
        return idx < generations_.size() && alive_[idx] &&
               generations_[idx] == entityGeneration(e);
    }

    [[nodiscard]] std::size_t aliveCount() const noexcept { return aliveCount_; }

    /// All currently valid entities (stable order: index order).
    [[nodiscard]] std::vector<Entity> aliveEntities() const {
        std::vector<Entity> out;
        out.reserve(aliveCount_);
        for (std::size_t i = 0; i < alive_.size(); ++i) {
            if (alive_[i]) out.push_back(makeEntity(static_cast<std::uint32_t>(i),
                                                    generations_[i]));
        }
        return out;
    }

    // ---- component API -----------------------------------------------------
    template <typename T>
    T& add(Entity e, T value = T{}) {
        ANA_ASSERT(valid(e), "add<T> on invalid entity");
        return poolFor<T>().insert(e, std::move(value));
    }

    template <typename T>
    [[nodiscard]] bool has(Entity e) const {
        if (!valid(e)) return false;
        const auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) return false;
        return it->second->has(e);
    }

    template <typename T>
    T& get(Entity e) {
        return poolFor<T>().get(e);   // pool get asserts presence
    }
    template <typename T>
    const T& get(Entity e) const {
        auto it = pools_.find(std::type_index(typeid(T)));
        ANA_ASSERT(it != pools_.end() && it->second->has(e),
                   "get<T> on entity without component");
        return static_cast<const ComponentPool<T>*>(it->second.get())->get(e);
    }

    /// Pointer variant for optional components (nullptr when absent).
    template <typename T>
    T* tryGet(Entity e) {
        if (!valid(e)) return nullptr;
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) return nullptr;
        auto* pool = static_cast<ComponentPool<T>*>(it->second.get());
        return pool->has(e) ? &pool->get(e) : nullptr;
    }
    template <typename T>
    const T* tryGet(Entity e) const {
        if (!valid(e)) return nullptr;
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) return nullptr;
        auto* pool = static_cast<ComponentPool<T>*>(it->second.get());
        return pool->has(e) ? &pool->get(e) : nullptr;
    }

    template <typename T>
    void remove(Entity e) {
        if (!valid(e)) return;
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it != pools_.end()) it->second->remove(e);
    }

    /// Get-or-create pool access for systems/Views.
    template <typename T>
    ComponentPool<T>& poolFor() {
        const std::type_index key(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            auto pool = std::make_unique<ComponentPool<T>>();
            auto* raw = pool.get();
            pools_.emplace(key, std::move(pool));
            return *raw;
        }
        return *static_cast<ComponentPool<T>*>(it->second.get());
    }

private:
    std::vector<std::uint32_t> generations_;    // slot → generation
    std::vector<bool> alive_;                   // slot → alive flag
    std::vector<std::uint32_t> freeList_;       // recycled slots (LIFO)
    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
    std::size_t aliveCount_ = 0;
};

// ---------------------------------------------------------------------------
// View<Ts...> — iterate entities owning ALL of Ts... efficiently.
// ---------------------------------------------------------------------------
template <typename... Ts>
class View {
public:
    static_assert(sizeof...(Ts) > 0, "View requires at least one component type");

    explicit View(Registry& registry) noexcept : reg_(&registry) {}

    /// NOTE: do not add/remove components for the iterated types inside fn
    /// (structural change invalidates dense slots mid-iteration).
    template <typename Fn>
    void each(Fn&& fn) {
        std::array<IComponentPool*, sizeof...(Ts)> pools{
            &reg_->template poolFor<Ts>()...};

        const IComponentPool* smallest = pools[0];
        for (const IComponentPool* p : pools)
            if (p->denseSize() < smallest->denseSize()) smallest = p;

        const std::size_t count = smallest->denseSize();
        for (std::size_t i = 0; i < count; ++i) {
            const Entity e = smallest->denseEntity(i);
            if ((reg_->template has<Ts>(e) && ...)) {
                fn(e, reg_->template get<Ts>(e)...);
            }
        }
    }

private:
    Registry* reg_;
};

/// Convenience factory: world.view<A, B>([](Entity, A&, B&){ ... });
template <typename... Ts, typename Fn>
void view(Registry& registry, Fn&& fn) {
    View<Ts...>(registry).each(fn);
}

}  // namespace anaconda::ecs
