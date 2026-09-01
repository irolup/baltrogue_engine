#include "Rendering/InstanceBatcher.h"
#include "Rendering/Material.h"
#include "Rendering/Mesh.h"

#include <functional>
#include <unordered_map>

namespace GameEngine {

size_t InstanceBatcher::BatchKeyHash::operator()(const BatchKey& key) const {
    size_t h = std::hash<const void*>{}(key.mesh);
    h ^= std::hash<const void*>{}(key.material) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= static_cast<size_t>(key.flags) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

uint8_t InstanceBatcher::stateFlags(const RenderCommand& command) {
    uint8_t flags = 0;
    if (command.disableCulling) flags |= 1u << 0;
    if (command.receiveShadows) flags |= 1u << 1;
    return flags;
}

bool InstanceBatcher::isInstanceable(const RenderCommand& command) {
    if (!command.mesh || !command.material) {
        return false;
    }
    // Beams derive their geometry from per-draw endpoints, not from a transform
    if (command.isBeam) {
        return false;
    }
    // Skinned draws carry their own bone matrix array per command
    if (command.boneTransforms && !command.boneTransforms->empty()) {
        return false;
    }
    if (command.material->getBlendMode() != BlendMode::Opaque) {
        return false;
    }
    return true;
}

void InstanceBatcher::clear() {
    batches_.clear();
    instanceData_.clear();
    batched_.clear();
    batchedCommandCount_ = 0;
}

void InstanceBatcher::build(const std::vector<RenderCommand>& queue, const std::vector<uint8_t>& visible, InstanceEligibleFn eligible, uint32_t minBatchSize) {
    clear();
    batched_.assign(queue.size(), 0);

    if (minBatchSize < 2 || queue.size() < minBatchSize) {
        return;
    }

    std::vector<std::vector<uint32_t>> groups;
    std::unordered_map<BatchKey, size_t, BatchKeyHash> groupOfKey;
    groups.reserve(queue.size() / 2 + 1);

    for (size_t i = 0; i < queue.size(); ++i) {
        const RenderCommand& command = queue[i];

        const bool isVisible = visible.empty() || (i < visible.size() && visible[i] != 0);
        if (!isVisible) {
            continue;
        }
        if (!isInstanceable(command)) {
            continue;
        }
        if (eligible && !eligible(command)) {
            continue;
        }

        const BatchKey key{command.mesh.get(), command.material.get(), stateFlags(command)};
        auto found = groupOfKey.find(key);
        if (found == groupOfKey.end()) {
            groupOfKey.emplace(key, groups.size());
            groups.emplace_back();
            groups.back().push_back(static_cast<uint32_t>(i));
        } else {
            groups[found->second].push_back(static_cast<uint32_t>(i));
        }
    }

    for (const auto& group : groups) {
        if (group.size() < minBatchSize) {
            continue;
        }

        Batch batch;
        batch.queueIndex = group.front();
        batch.firstInstance = static_cast<uint32_t>(instanceData_.size());
        batch.instanceCount = static_cast<uint32_t>(group.size());

        for (uint32_t queueIndex : group) {
            const RenderCommand& command = queue[queueIndex];

            InstanceData data;
            data.modelMatrix = command.modelMatrix;
            data.normalMatrix[0] = glm::vec4(command.normalMatrix[0], 0.0f);
            data.normalMatrix[1] = glm::vec4(command.normalMatrix[1], 0.0f);
            data.normalMatrix[2] = glm::vec4(command.normalMatrix[2], 0.0f);
            instanceData_.push_back(data);

            batched_[queueIndex] = 1;
        }

        batchedCommandCount_ += batch.instanceCount;
        batches_.push_back(batch);
    }
}

}
