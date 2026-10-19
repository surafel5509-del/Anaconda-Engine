// ===========================================================================
//  anaconda/physics/Broadphase.hpp — spatial hash grid (Month 3).
//
//  O(n) average broadphase for up to 10k colliders. Cell size = 2 world units
//  (tunable). Each entity is inserted into 1-4 cells based on its AABB. Pairs
//  are generated per-cell, deduplicated via sorted pair set, then handed to
//  narrowphase. Host-testable, no GL, no allocations in hot loop beyond
//  per-frame hash map clear.
// ===========================================================================
#pragma once

#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/physics/PhysicsSystem.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace anaconda {

struct BroadphaseCell {
    int x = 0, y = 0;
    bool operator==(const BroadphaseCell& o) const { return x == o.x && y == o.y; }
};

struct BroadphaseCellHash {
    size_t operator()(const BroadphaseCell& c) const noexcept {
        return (static_cast<size_t>(c.x) * 73856093ULL) ^ (static_cast<size_t>(c.y) * 19349663ULL);
    }
};

class BroadphaseGrid {
public:
    explicit BroadphaseGrid(float cellSize = 2.0f) : cellSize_(cellSize) {}

    void setCellSize(float s) { cellSize_ = s > 0.1f ? s : 2.0f; }
    float cellSize() const { return cellSize_; }

    void clear() { cells_.clear(); }

    // Insert an entity's AABB into the grid.
    void insert(ecs::Entity e, const Aabb& box) {
        int minX = static_cast<int>(std::floor(box.min.x / cellSize_));
        int maxX = static_cast<int>(std::floor(box.max.x / cellSize_));
        int minY = static_cast<int>(std::floor(box.min.y / cellSize_));
        int maxY = static_cast<int>(std::floor(box.max.y / cellSize_));
        for (int x = minX; x <= maxX; ++x)
            for (int y = minY; y <= maxY; ++y)
                cells_[{x, y}].push_back(e);
    }

    // Generate candidate pairs (entity indices). Deduplicated.
    std::vector<std::pair<ecs::Entity, ecs::Entity>> generatePairs() const {
        std::unordered_set<uint64_t> seen;
        std::vector<std::pair<ecs::Entity, ecs::Entity>> out;
        for (auto& kv : cells_) {
            auto& list = kv.second;
            for (size_t i = 0; i < list.size(); ++i)
                for (size_t j = i + 1; j < list.size(); ++j) {
                    uint64_t a = static_cast<uint64_t>(list[i]);
                    uint64_t b = static_cast<uint64_t>(list[j]);
                    if (a > b) std::swap(a, b);
                    uint64_t key = (a << 32) | b;
                    if (seen.insert(key).second) out.emplace_back(static_cast<ecs::Entity>(a), static_cast<ecs::Entity>(b));
                }
        }
        return out;
    }

    size_t cellCount() const { return cells_.size(); }

private:
    float cellSize_;
    std::unordered_map<BroadphaseCell, std::vector<ecs::Entity>, BroadphaseCellHash> cells_;
};

}  // namespace anaconda
