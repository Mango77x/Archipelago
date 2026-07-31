#pragma once

// --- Fase 7.0: Jolt Physics world setup. Layer/filter boilerplate follows
// Jolt's own "HelloWorld" sample structure — validated separately (a static
// floor + a box falling under gravity and settling on it) before wiring
// CargoShip to a real rigid body. ---

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include "common.h"

namespace archipelago {

namespace JoltLayers {
constexpr JPH::ObjectLayer kNonMoving = 0;
constexpr JPH::ObjectLayer kMoving = 1;
constexpr JPH::ObjectLayer kNumLayers = 2;
}  // namespace JoltLayers

namespace JoltBroadPhaseLayers {
constexpr JPH::BroadPhaseLayer kNonMoving(0);
constexpr JPH::BroadPhaseLayer kMoving(1);
constexpr JPH::uint kNumLayers = 2;
}  // namespace JoltBroadPhaseLayers

class JoltBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface {
public:
    JoltBroadPhaseLayerInterface();
    JPH::uint GetNumBroadPhaseLayers() const override { return JoltBroadPhaseLayers::kNumLayers; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return objectToBroadPhase_[layer];
    }

private:
    JPH::BroadPhaseLayer objectToBroadPhase_[JoltLayers::kNumLayers];
};

class JoltObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override;
};

class JoltObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override;
};

// Fase 7.1: a simple flat seafloor — static collision body, deep enough to
// stay out of the way of normal buoyancy (nothing should ever ride this low
// under current gameplay). No deformation/terrain yet, on purpose: the user
// asked for "just a floor for now," with actual terrain shape deferred to
// whenever procedural world generation (Fase 8) settles what that even
// means. Exists mainly so the sea has *something* down there for whatever
// eventually needs it — grounding, anchoring, submarines.
JPH::BodyID CreateSeaFloorBody(JPH::BodyInterface& bodyInterface, float centerX, float centerZ, float halfExtentX,
                                float halfExtentZ, float depth);

}  // namespace archipelago
