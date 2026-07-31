#include "ship.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include "jolt_world.h"
#include "waves.h"

namespace archipelago {

void CargoShip::ApplyInput(bool thrustForward, bool thrustBackward, bool turnLeft, bool turnRight) {
    float heading = CurrentHeading();
    JPH::Vec3 forward(std::cos(heading), 0.0f, std::sin(heading));
    if (thrustForward) bodyInterface_->AddForce(bodyId_, forward * kThrustForce);
    if (thrustBackward) bodyInterface_->AddForce(bodyId_, forward * -kThrustForce);
    if (turnLeft) bodyInterface_->AddTorque(bodyId_, JPH::Vec3(0.0f, kTurnTorque, 0.0f));
    if (turnRight) bodyInterface_->AddTorque(bodyId_, JPH::Vec3(0.0f, -kTurnTorque, 0.0f));
}

void CargoShip::AutoPilot(Vec3 island1Dock, Vec3 island2Dock, Vec3 island3Dock) {
    Vec3 origin = (routeKind_ == RouteKind::IronRoute) ? island1Dock : island2Dock;
    Vec3 destination = (routeKind_ == RouteKind::IronRoute) ? island2Dock : island3Dock;
    Vec3 target = (cargo_ <= 0) ? origin : destination;

    Vec3 pos = position();
    float dx = target.x - pos.x;
    float dz = target.z - pos.z;
    float distance = std::sqrt(dx * dx + dz * dz);
    if (distance < 1.0f) return;

    float heading = CurrentHeading();
    float desiredHeading = std::atan2(dz, dx);
    float angleDiff = NormalizeAngle(desiredHeading - heading);

    if (angleDiff > kAngleThreshold) {
        bodyInterface_->AddTorque(bodyId_, JPH::Vec3(0.0f, -kTurnTorque, 0.0f));
    } else if (angleDiff < -kAngleThreshold) {
        bodyInterface_->AddTorque(bodyId_, JPH::Vec3(0.0f, kTurnTorque, 0.0f));
    }

    Vec3 vel = velocity();
    float speed = std::sqrt(vel.x * vel.x + vel.z * vel.z);
    float desiredSpeed = kMaxSpeed * std::min(1.0f, distance / kBrakingDistance);

    if (speed > desiredSpeed && speed > 0.01f) {
        JPH::Vec3 brakeDir(-vel.x / speed, 0.0f, -vel.z / speed);
        bodyInterface_->AddForce(bodyId_, brakeDir * kThrustForce);
    } else if (std::abs(angleDiff) < kThrustAngleLimit) {
        JPH::Vec3 forward(std::cos(heading), 0.0f, std::sin(heading));
        bodyInterface_->AddForce(bodyId_, forward * kThrustForce);
    }
}

void CargoShip::ApplyBuoyancy(float waveTime) {
    JPH::RVec3 centerOfMass = bodyInterface_->GetCenterOfMassPosition(bodyId_);
    JPH::Quat rotation = bodyInterface_->GetRotation(bodyId_);
    // Bottom-face corners in local (body) space — half-extents match the
    // box shape in CreateShipBody (24, 8, 12).
    constexpr float kHalfLength = 24.0f;
    constexpr float kHalfHeight = 8.0f;
    constexpr float kHalfWidth = 12.0f;
    const JPH::Vec3 localCorners[4] = {
        JPH::Vec3(kHalfLength, -kHalfHeight, kHalfWidth),
        JPH::Vec3(kHalfLength, -kHalfHeight, -kHalfWidth),
        JPH::Vec3(-kHalfLength, -kHalfHeight, kHalfWidth),
        JPH::Vec3(-kHalfLength, -kHalfHeight, -kHalfWidth),
    };
    for (const JPH::Vec3& localCorner : localCorners) {
        JPH::RVec3 cornerPos = centerOfMass + (rotation * localCorner);
        float waterHeight = Waves::Height(static_cast<float>(cornerPos.GetX()), static_cast<float>(cornerPos.GetZ()),
                                           waveTime);
        float submersion = waterHeight - static_cast<float>(cornerPos.GetY());
        if (submersion <= 0.0f) continue;
        // Spring-only buoyancy (force proportional to depth alone) is an
        // undamped oscillator — first test had the hull launching clear
        // out of the water and bobbing indefinitely. kBuoyancyDamping adds
        // a dashpot term (opposing that corner's actual vertical velocity,
        // via GetPointVelocity so pitch/roll contribute too) tuned near
        // critical damping for the stiffness below, so it settles instead
        // of bouncing.
        float cornerVerticalVelocity = bodyInterface_->GetPointVelocity(bodyId_, cornerPos).GetY();
        float force = kBuoyancyStiffness * submersion - kBuoyancyDamping * cornerVerticalVelocity;
        bodyInterface_->AddForce(bodyId_, JPH::Vec3(0.0f, force, 0.0f), cornerPos);
    }
}

void CargoShip::ClampSpeed() {
    JPH::Vec3 v = bodyInterface_->GetLinearVelocity(bodyId_);
    float speed = std::sqrt(v.GetX() * v.GetX() + v.GetZ() * v.GetZ());
    if (speed > kMaxSpeed) {
        float scale = kMaxSpeed / speed;
        bodyInterface_->SetLinearVelocity(bodyId_, JPH::Vec3(v.GetX() * scale, v.GetY(), v.GetZ() * scale));
    }
}

void CargoShip::HandleDocking(Warehouse& island1Warehouse, Warehouse& island2Warehouse, Market& market,
                               Economy& economy, Port& port, Vec3 island1Dock, Vec3 island2Dock, Vec3 island3Dock,
                               bool isBot) {
    Vec3 pos = position();
    bool canHandleIron = !isBot || routeKind_ == RouteKind::IronRoute;
    bool canHandleSteel = !isBot || routeKind_ == RouteKind::SteelRoute;

    // Isla 1 (Mina): pick up Iron if the hold is empty.
    if (canHandleIron && cargo_ <= 0 && HorizontalDistance(pos, island1Dock) <= kDockRadius) {
        int available = island1Warehouse.Get(Resource::Iron);
        int amount = std::min(available, capacity_);
        if (amount > 0) {
            island1Warehouse.Withdraw(Resource::Iron, amount);
            cargo_ = amount;
            cargoResource_ = Resource::Iron;
            std::cout << "Cargo Ship loaded " << amount << " Iron at Isla 1\n";
        }
    }

    // Isla 2 (Aceria): deliver Iron if carrying it; otherwise pick up Steel if empty.
    if (HorizontalDistance(pos, island2Dock) <= kDockRadius) {
        if (cargo_ > 0 && cargoResource_ == Resource::Iron) {
            island2Warehouse.Deposit(Resource::Iron, cargo_);
            std::cout << "Cargo Ship delivered " << cargo_ << " Iron at Isla 2\n";
            cargo_ = 0;
        } else if (canHandleSteel && cargo_ <= 0) {
            int available = island2Warehouse.Get(Resource::Steel);
            int amount = std::min(available, capacity_);
            if (amount > 0) {
                island2Warehouse.Withdraw(Resource::Steel, amount);
                cargo_ = amount;
                cargoResource_ = Resource::Steel;
                std::cout << "Cargo Ship loaded " << amount << " Steel at Isla 2\n";
            }
        }
    }

    // Isla 3 (Puerto): sell Steel.
    if (cargo_ > 0 && cargoResource_ == Resource::Steel && HorizontalDistance(pos, island3Dock) <= kDockRadius) {
        double revenue = market.Sell(cargo_);
        economy.AddRevenue(revenue);
        port.Export(cargo_);
        std::cout << "Cargo Ship sold " << cargo_ << " Steel for $" << revenue << "\n";
        cargo_ = 0;
    }
}

Vec3 CargoShip::position() const {
    JPH::RVec3 p = bodyInterface_->GetCenterOfMassPosition(bodyId_);
    return Vec3(static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ()));
}

Vec3 CargoShip::velocity() const {
    JPH::Vec3 v = bodyInterface_->GetLinearVelocity(bodyId_);
    return Vec3(v.GetX(), v.GetY(), v.GetZ());
}

float CargoShip::heading() const { return CurrentHeading(); }

glm::quat CargoShip::rotation() const {
    JPH::Quat q = bodyInterface_->GetRotation(bodyId_);
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

float CargoShip::angularVelocity() const { return bodyInterface_->GetAngularVelocity(bodyId_).GetY(); }

void CargoShip::SetState(Vec3 position, Vec3 velocity, float heading, float angularVelocityY, int cargo,
                          Resource cargoResource) {
    JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), heading);
    bodyInterface_->SetPositionAndRotation(bodyId_, JPH::RVec3(position.x, position.y, position.z), rotation,
                                            JPH::EActivation::Activate);
    bodyInterface_->SetLinearVelocity(bodyId_, JPH::Vec3(velocity.x, velocity.y, velocity.z));
    bodyInterface_->SetAngularVelocity(bodyId_, JPH::Vec3(0.0f, angularVelocityY, 0.0f));
    cargo_ = cargo;
    cargoResource_ = cargoResource;
}

float CargoShip::CurrentHeading() const {
    JPH::Vec3 forward = bodyInterface_->GetRotation(bodyId_) * JPH::Vec3::sAxisX();
    return std::atan2(forward.GetZ(), forward.GetX());
}

JPH::BodyID CreateShipBody(JPH::BodyInterface& bodyInterface, Vec3 startPos) {
    // Spawning exactly at the nominal water level (Y=0, submersion=0) means
    // buoyancy starts at zero and gravity gets a free first few frames before
    // any restoring force exists — a needless splash-down transient. Starting
    // a bit above 0 instead pre-positions the hull near its expected resting
    // waterline: weight (mass*gravity=5000*9.81≈49050) / (4 corners *
    // CargoShip::kBuoyancyStiffness=2000) ≈ 6.1 units of submersion at the
    // bottom corners (local Y=-8 relative to center of mass) at equilibrium,
    // so center of mass ≈ -8 + 6.1 ≈ +1.9 above the surface it settles into.
    // Keep this in sync if kBuoyancyStiffness or the hull's mass/half-height
    // change enough to matter.
    constexpr float kInitialDraftOffset = 1.9f;
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(24.0f, 8.0f, 12.0f));
    JPH::ShapeRefC shape = shapeSettings.Create().Get();
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(startPos.x, startPos.y + kInitialDraftOffset, startPos.z),
                                        JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, JoltLayers::kMoving);
    settings.mAllowedDOFs = JPH::EAllowedDOFs::All;
    settings.mGravityFactor = 1.0f;
    settings.mLinearDamping = 0.6f;
    settings.mAngularDamping = 0.9f;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = 5000.0f;
    return bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
}

}  // namespace archipelago
