#include "jolt_world.h"

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

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

JPH::BodyID CreateSeaFloorBody(JPH::BodyInterface& bodyInterface, float centerX, float centerZ, float halfExtentX,
                                float halfExtentZ, float depth) {
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(halfExtentX, 10.0f, halfExtentZ));
    JPH::ShapeRefC shape = shapeSettings.Create().Get();
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(centerX, depth, centerZ), JPH::Quat::sIdentity(),
                                        JPH::EMotionType::Static, JoltLayers::kNonMoving);
    return bodyInterface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
}

}  // namespace archipelago
