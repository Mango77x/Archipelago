#include "jolt_world.h"

#include <vector>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include "terrain.h"

namespace archipelago {

JoltBroadPhaseLayerInterface::JoltBroadPhaseLayerInterface() {
    objectToBroadPhase_[JoltLayers::kNonMoving] = JoltBroadPhaseLayers::kNonMoving;
    objectToBroadPhase_[JoltLayers::kMoving] = JoltBroadPhaseLayers::kMoving;
}

bool JoltObjectVsBroadPhaseLayerFilter::ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const {
    if (layer1 == JoltLayers::kNonMoving) return layer2 == JoltBroadPhaseLayers::kMoving;
    return true;
}

bool JoltObjectLayerPairFilter::ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const {
    if (object1 == JoltLayers::kNonMoving) return object2 == JoltLayers::kMoving;
    return true;
}

JPH::BodyID CreateSeaFloorHeightFieldBody(JPH::BodyInterface& bodyInterface, float centerX, float centerZ,
                                           float halfExtentX, float halfExtentZ, int sampleCount, uint32_t seed,
                                           float offsetX, float offsetZ) {
    float minX = centerX - halfExtentX;
    float minZ = centerZ - halfExtentZ;
    float stepX = (2.0f * halfExtentX) / static_cast<float>(sampleCount - 1);
    float stepZ = (2.0f * halfExtentZ) / static_cast<float>(sampleCount - 1);

    std::vector<float> samples(static_cast<size_t>(sampleCount) * static_cast<size_t>(sampleCount));
    for (int row = 0; row < sampleCount; ++row) {
        float z = minZ + stepZ * static_cast<float>(row);
        for (int col = 0; col < sampleCount; ++col) {
            float x = minX + stepX * static_cast<float>(col);
            samples[static_cast<size_t>(row) * static_cast<size_t>(sampleCount) + static_cast<size_t>(col)] =
                Terrain::Height(x - offsetX, z - offsetZ, seed);
        }
    }

    // inOffset places sample (0,0) at world (minX, 0, minZ); inScale.y=1
    // since samples[] already stores real world-space Y heights directly,
    // no extra vertical scaling needed.
    JPH::HeightFieldShapeSettings shapeSettings(samples.data(), JPH::Vec3(minX, 0.0f, minZ),
                                                 JPH::Vec3(stepX, 1.0f, stepZ),
                                                 static_cast<JPH::uint32>(sampleCount));
    JPH::ShapeRefC shape = shapeSettings.Create().Get();
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(0.0, 0.0, 0.0), JPH::Quat::sIdentity(),
                                        JPH::EMotionType::Static, JoltLayers::kNonMoving);
    return bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

}  // namespace archipelago
