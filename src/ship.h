#pragma once

// Player-piloted (or autopiloted) cargo ship — Fase 7.0: backed by a real
// Jolt rigid body instead of a hand-rolled kinematic model. Thrust/turn are
// real forces/torques applied to the body; PhysicsSystem::Update() (called
// once per fixed step for every ship at once, in main()) does the actual
// integration. Fase 7.1 added real buoyancy (see ApplyBuoyancy) — full 6
// degrees of freedom, real gravity, waves push the hull around.
//
// Loading/unloading is purely proximity + cargo-state driven: the ship reacts
// to whichever island dock it's physically at and whatever it's currently
// carrying, regardless of who's steering it. That means the player can always
// self-service any leg of the chain by flying there themselves — RouteKind
// only tells an autopiloted ship which pair of docks to shuttle between, it
// never locks a ship (player's included) out of a dock it isn't "assigned" to.

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <glm/gtc/quaternion.hpp>

#include "common.h"
#include "economy.h"

namespace archipelago {

class CargoShip {
public:
    // Radius (world units) around a dock where loading/unloading triggers.
    // Public so rendering can draw the same zone the simulation actually uses.
    static constexpr float kDockRadius = 40.0f;
    // Safety-net clamp while thrust/damping values are still being tuned by
    // feel — not a permanent kludge, drop it once the equilibrium speed from
    // real force-vs-damping is dialed in and this stops being necessary.
    static constexpr float kMaxSpeed = 160.0f;

    // The Jolt body must already exist (see CreateShipBody) — CargoShip
    // doesn't own its lifetime. BodyID is a small, trivially-copyable handle;
    // ships are never destroyed once created in this game, so explicit
    // cleanup isn't needed yet (revisit if/when ships can sink or despawn).
    CargoShip(JPH::BodyInterface& bodyInterface, JPH::BodyID bodyId, int capacity, RouteKind routeKind)
        : bodyInterface_(&bodyInterface), bodyId_(bodyId), capacity_(capacity), routeKind_(routeKind) {}

    // Takes resolved intent flags rather than raw SDL key state, so the same
    // call works whether the flags came from a live keyboard this frame or
    // from a recorded ReplayFrame during playback — CargoShip doesn't need to
    // know or care which.
    void ApplyInput(bool thrustForward, bool thrustBackward, bool turnLeft, bool turnRight);

    // Autonomous ships steer with the same thrust/torque model as the player
    // (principle: the same rules apply, no special-cased movement for
    // AI-controlled assets). Target is the ship's assigned pair of docks,
    // picked by cargo state: empty means "go get more," carrying means "go
    // deliver."
    //
    // This is "arrive" steering, not just "seek": the desired speed tapers
    // down as distance to the target shrinks, and the ship actively brakes
    // (force opposing current velocity) whenever it's going faster than
    // that — otherwise it reaches the dock at full speed and skids/overshoots
    // trying to correct.
    void AutoPilot(Vec3 island1Dock, Vec3 island2Dock, Vec3 island3Dock);

    // Fase 7.1: buoyancy. Samples the wave surface at the 4 bottom corners of
    // the hull, in its *current* orientation (not just yaw), and pushes each
    // corner up proportional to how far it sits below the local wave height.
    // Applying the force at each corner's actual world point (not once at the
    // center) is what makes pitch/roll come out of real physics — Jolt turns
    // an unevenly-submerged hull into the right torque on its own, no manual
    // pitch/roll simulation needed. Called once per fixed step, before
    // PhysicsSystem::Update(), same as thrust/steering forces.
    void ApplyBuoyancy(float waveTime);

    // Clamps linear speed as a safety net (see kMaxSpeed) — called once per
    // fixed step, after PhysicsSystem::Update() has integrated this step's
    // forces, before HandleDocking() reads the settled position.
    void ClampSpeed();

    // Game-logic half of what used to be Update(): physics integration now
    // happens once for every body via PhysicsSystem::Update() in main(), so
    // this only handles proximity-based loading/unloading, reading position
    // from the Jolt body. isBot restricts loading/unloading to this ship's
    // assigned routeKind_ — that's what stops an autopiloted ship from
    // opportunistically scooping up the wrong resource and getting stuck. The
    // player's ship always passes isBot=false: a human pilot can dock
    // anywhere and load whatever's there, no restriction.
    void HandleDocking(Warehouse& island1Warehouse, Warehouse& island2Warehouse, Market& market, Economy& economy,
                        Port& port, Vec3 island1Dock, Vec3 island2Dock, Vec3 island3Dock, bool isBot);

    Vec3 position() const;
    Vec3 velocity() const;
    float heading() const;
    // Full body rotation (pitch/roll included), for rendering the hull —
    // heading() stays yaw-only and is what steering/camera use. Passed
    // straight through with no sign flips: CurrentHeading()'s yaw is the
    // negative of Jolt's raw rotation angle, and the renderer's yaw-only
    // DrawBox overload negates it again, so the two negations already cancel
    // out — using the raw rotation here reproduces the exact same yaw case
    // and correctly extends it to pitch/roll.
    glm::quat rotation() const;
    float angularVelocity() const;
    int cargo() const { return cargo_; }
    Resource cargoResource() const { return cargoResource_; }
    RouteKind routeKind() const { return routeKind_; }

    void SetState(Vec3 position, Vec3 velocity, float heading, float angularVelocityY, int cargo,
                  Resource cargoResource);

private:
    // Tuned so the thrust-vs-damping equilibrium speed (F / (mass * linearDamping),
    // see CreateShipBody) lands near kMaxSpeed instead of far below it — the previous
    // value gave an equilibrium of ~15 units/s, which read as "ridiculously slow"
    // in testing even though kMaxSpeed allowed up to 160.
    static constexpr float kThrustForce = 400000.0f;
    // Scaled up from the original 400000 so turning keeps pace with the
    // faster cruise speed (otherwise the ship's turning radius grows and it
    // overshoots docks) — but 3200000 turned it into a spinning top, not a
    // ship. This is a middle ground: still enough authority to line up on
    // approach, but slow/heavy like a real hull, not twitchy.
    static constexpr float kTurnTorque = 1000000.0f;
    static constexpr float kAngleThreshold = 0.15f;
    static constexpr float kThrustAngleLimit = 1.2f;
    // Force per corner = kBuoyancyStiffness * submersion depth (world units).
    // Picked so the hull's ~49000 (mass 5000 * gravity 9.81) weight is
    // balanced with roughly half the 8-unit half-height submerged at rest —
    // a real ship's waterline, not skimming the surface. The first tuning
    // pass (3000, no damping) floated far too high and bounced clean out of
    // the water; see kBuoyancyDamping.
    static constexpr float kBuoyancyStiffness = 2000.0f;
    // Near-critical damping for kBuoyancyStiffness against ~1/4 the hull's
    // mass per corner (2*sqrt(stiffness*mass) ballpark) — stops the vertical
    // bounce from the spring term without killing all natural bobbing.
    static constexpr float kBuoyancyDamping = 3200.0f;
    static constexpr float kBrakingDistance = 450.0f;

    float CurrentHeading() const;

    JPH::BodyInterface* bodyInterface_;
    JPH::BodyID bodyId_;
    int capacity_;
    int cargo_ = 0;
    Resource cargoResource_ = Resource::Iron;
    RouteKind routeKind_;
};

// Creates a ship-hull rigid body: dynamic, full 6 degrees of freedom, real
// gravity. Fase 7.0 locked this to the X/Z plane + yaw with gravity off,
// gliding on an implicit flat sea; Fase 7.1 relaxes that now that buoyancy
// (see CargoShip::ApplyBuoyancy) actually holds the hull up and lets waves
// push it around — heave, pitch and roll all come from real forces now.
JPH::BodyID CreateShipBody(JPH::BodyInterface& bodyInterface, Vec3 startPos);

}  // namespace archipelago
