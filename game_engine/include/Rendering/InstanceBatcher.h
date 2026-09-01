#pragma once

#include "Rendering/RenderTypes.h"
#include <glm/glm.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace GameEngine {

// Per-instance vertex data, shared by every backend
// glm matrices are column-major, so a mat4 is four consecutive vec4 columns 
// and the shader's mat4(c0, c1, c2, c3) is a straight reinterpret of these bytes. 
struct InstanceData {
    glm::mat4 modelMatrix;
    glm::vec4 normalMatrix[3];
};

// Decides whether a backend can instance a given command at all
using InstanceEligibleFn = bool (*)(const RenderCommand&);

class InstanceBatcher {
public:
    struct Batch {
        uint32_t queueIndex = 0; // representative command: mesh, material, state
        uint32_t firstInstance = 0; // offset into instanceData()
        uint32_t instanceCount = 0;
    };

    void build(const std::vector<RenderCommand>& queue, const std::vector<uint8_t>& visible, InstanceEligibleFn eligible, uint32_t minBatchSize = 2);

    void clear();

    const std::vector<Batch>& batches() const { return batches_; }
    const std::vector<InstanceData>& instanceData() const { return instanceData_; }
    bool empty() const { return batches_.empty(); }

    bool isBatched(size_t queueIndex) const {
        return queueIndex < batched_.size() && batched_[queueIndex] != 0;
    }

    uint32_t batchedCommandCount() const { return batchedCommandCount_; }

private:
    struct BatchKey {
        const Mesh* mesh;
        const Material* material;
        uint8_t flags;

        BatchKey(const Mesh* meshKey, const Material* materialKey, uint8_t stateKey) : mesh(meshKey), material(materialKey), flags(stateKey) {}

        bool operator==(const BatchKey& other) const {
            return mesh == other.mesh && material == other.material && flags == other.flags;
        }
    };

    struct BatchKeyHash {
        size_t operator()(const BatchKey& key) const;
    };

    // Per-draw state that does not live in the material, so it cannot be folded into a single instanced draw
    static uint8_t stateFlags(const RenderCommand& command);

    // eligibility test.
    static bool isInstanceable(const RenderCommand& command);

    std::vector<Batch> batches_;
    std::vector<InstanceData> instanceData_;
    std::vector<uint8_t> batched_;
    uint32_t batchedCommandCount_ = 0;
};

}
