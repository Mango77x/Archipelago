#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace archipelago {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
// Tuned so a single ship's real-time round trip (~30s at current island
// distances/speeds) corresponds to a few simulated hours, not fifteen — see
// the user report about going broke "haga lo que haga" in the vault's
// Fase 4 notes. At 2.0 (the Fase 1-3 value, when everything was on one
// screen and travel was near-instant), the mine outproduced any possible
// shipping rate by ~10x and maintenance drained cash regardless of piloting.
constexpr double kSecondsPerSimulatedHour = 8.0;
constexpr float kPi = 3.14159265358979323846f;

// Simulation steps run at a fixed timestep, decoupled from the real render
// framerate. This is what makes the Replay system possible: the same
// recorded input sequence, fed through the same fixed dt each step, always
// produces the same result — a variable per-frame dt would make replays
// depend on how fast the machine happened to render each time.
constexpr float kFixedDt = 1.0f / 60.0f;

enum class Resource { Iron, Steel };

std::string ToString(Resource r) {
    switch (r) {
        case Resource::Iron: return "Iron";
        case Resource::Steel: return "Steel";
    }
    return "Unknown";
}

// Which pair of islands an autopiloted ship shuttles between: Isla1(mina) <->
// Isla2(aceria), or Isla2(aceria) <-> Isla3(puerto). Only steers autopilot —
// it never restricts what a ship can load/unload if you fly it somewhere
// else yourself (see CargoShip::Update).
enum class RouteKind { IronRoute, SteelRoute };

class Warehouse {
public:
    void Deposit(Resource resource, int amount) { stock_[resource] += amount; }

    bool Withdraw(Resource resource, int amount) {
        int& current = stock_[resource];
        if (current < amount) return false;
        current -= amount;
        return true;
    }

    int Get(Resource resource) const {
        auto it = stock_.find(resource);
        return it == stock_.end() ? 0 : it->second;
    }

    void SetStock(Resource resource, int amount) { stock_[resource] = amount; }

private:
    std::unordered_map<Resource, int> stock_;
};

class IronMine {
public:
    explicit IronMine(int rate) : rate_(rate) {}

    void Tick(Warehouse& warehouse, int hour) {
        warehouse.Deposit(Resource::Iron, rate_);
        totalProduced_ += rate_;
        std::cout << "[hour " << hour << "] Iron Mine produced " << rate_ << " Iron\n";
    }

    int totalProduced() const { return totalProduced_; }
    void SetTotalProduced(int value) { totalProduced_ = value; }

private:
    int rate_;
    int totalProduced_ = 0;
};

class SteelMill {
public:
    SteelMill(int consumeRate, int outputRatioPercent)
        : consumeRate_(consumeRate), outputRatioPercent_(outputRatioPercent) {}

    void Tick(Warehouse& warehouse, int hour) {
        if (!warehouse.Withdraw(Resource::Iron, consumeRate_)) {
            idle_ = true;
            std::cout << "[hour " << hour << "] Steel Mill idle (not enough Iron)\n";
            return;
        }
        idle_ = false;
        totalConsumed_ += consumeRate_;
        int produced = consumeRate_ * outputRatioPercent_ / 100;
        warehouse.Deposit(Resource::Steel, produced);
        totalProduced_ += produced;
        std::cout << "[hour " << hour << "] Steel Mill consumed " << consumeRate_ << " Iron, produced "
                   << produced << " Steel\n";
    }

    int totalConsumed() const { return totalConsumed_; }
    int totalProduced() const { return totalProduced_; }
    bool isIdle() const { return idle_; }
    void SetTotals(int consumed, int produced, bool idle) {
        totalConsumed_ = consumed;
        totalProduced_ = produced;
        idle_ = idle;
    }

private:
    int consumeRate_;
    int outputRatioPercent_;
    int totalConsumed_ = 0;
    int totalProduced_ = 0;
    bool idle_ = false;
};

class Port {
public:
    void Export(int amount) {
        totalExported_ += amount;
        std::cout << "Port exported " << amount << " Steel\n";
    }

    int totalExported() const { return totalExported_; }
    void SetTotalExported(int value) { totalExported_ = value; }

private:
    int totalExported_ = 0;
};

// Single-commodity supply/demand market for Steel. Selling adds to stock_
// (a glut); Tick() absorbs stock at a fixed demand rate, and price recovers
// toward basePrice_ as stock drains. Flood the market faster than demand
// absorbs it and the price craters — the only lever Fase 2 gives the player
// to raise profit is pacing deliveries to match demand, not just moving more.
class Market {
public:
    Market(double basePrice, float demandPerHour, double sensitivity)
        : basePrice_(basePrice), demandPerHour_(demandPerHour), sensitivity_(sensitivity) {}

    double CurrentPrice() const {
        double price = basePrice_ / (1.0 + stock_ * sensitivity_);
        return std::max(price, basePrice_ * kMinPriceFraction);
    }

    double Sell(int amount) {
        double revenue = CurrentPrice() * amount;
        stock_ += amount;
        totalRevenue_ += revenue;
        totalSold_ += amount;
        return revenue;
    }

    void Tick(int hour) {
        float absorbed = std::min(stock_, demandPerHour_);
        stock_ -= absorbed;
        std::cout << "[hour " << hour << "] Market absorbed " << absorbed << " Steel (stock=" << stock_
                   << ", price=$" << CurrentPrice() << "/unit)\n";
    }

    float stock() const { return stock_; }
    double totalRevenue() const { return totalRevenue_; }
    int totalSold() const { return totalSold_; }

    void SetState(float stock, double totalRevenue, int totalSold) {
        stock_ = stock;
        totalRevenue_ = totalRevenue;
        totalSold_ = totalSold;
    }

private:
    static constexpr double kMinPriceFraction = 0.2;
    double basePrice_;
    float demandPerHour_;
    double sensitivity_;
    float stock_ = 0.0f;
    double totalRevenue_ = 0.0;
    int totalSold_ = 0;
};

// Player's cash balance. Revenue comes only from Market sales; expenses are
// the fixed hourly upkeep of mine + mill + fleet, charged whether or not the
// ships are moving — so profit only grows by moving more Steel, faster, and
// selling it without crashing the price. No scripted rewards, per Fase 2 DoD.
class Economy {
public:
    explicit Economy(double startingCash) : cash_(startingCash) {}

    void AddRevenue(double amount) {
        cash_ += amount;
        totalRevenue_ += amount;
    }
    void ChargeExpense(double amount) {
        cash_ -= amount;
        totalExpenses_ += amount;
    }

    double cash() const { return cash_; }
    double totalExpenses() const { return totalExpenses_; }
    double totalRevenue() const { return totalRevenue_; }

    void SetState(double cash, double totalExpenses, double totalRevenue) {
        cash_ = cash;
        totalExpenses_ = totalExpenses;
        totalRevenue_ = totalRevenue;
    }

private:
    double cash_;
    double totalExpenses_ = 0.0;
    double totalRevenue_ = 0.0;
};

// World positions live in 3D now (Fase 4.5): X/Z is the horizontal plane
// ships and islands sit on (matching the old 2D X/Y), Y is height/altitude —
// always 0 for this phase (nothing flies yet), but no longer hardcoded out of
// the data model. Rendering is the only thing that actually needed 3D; the
// economy/logistics logic underneath doesn't care (principle 2: los gráficos
// visualizan la simulación, no la sustituyen).
using Vec3 = glm::vec3;

// Wraps an angle (radians) to (-pi, pi]. Used to find the shortest turn
// direction toward a target heading.
float NormalizeAngle(float angle) {
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

float HorizontalDistance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// Player-piloted (or autopiloted) cargo ship — Fase 7.0: backed by a real
// Jolt rigid body instead of a hand-rolled kinematic model. Thrust/turn are
// real forces/torques applied to the body; PhysicsSystem::Update() (called
// once per fixed step for every ship at once, in main()) does the actual
// integration. Gravity is disabled and translation/rotation are constrained
// to the X/Z plane + yaw (EAllowedDOFs) — there's no water simulation yet
// (that's Fase 7.1, buoyancy/oleaje), so for now the hull just glides on an
// implicit flat sea, but via real mass/force/damping instead of manually
// nudging a velocity vector.
//
// Loading/unloading is purely proximity + cargo-state driven: the ship reacts
// to whichever island dock it's physically at and whatever it's currently
// carrying, regardless of who's steering it. That means the player can always
// self-service any leg of the chain by flying there themselves — RouteKind
// only tells an autopiloted ship which pair of docks to shuttle between, it
// never locks a ship (player's included) out of a dock it isn't "assigned" to.
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
    // from a recorded ReplayFrame during playback (see Replay system below) —
    // CargoShip doesn't need to know or care which.
    void ApplyInput(bool thrustForward, bool thrustBackward, bool turnLeft, bool turnRight) {
        float heading = CurrentHeading();
        JPH::Vec3 forward(std::cos(heading), 0.0f, std::sin(heading));
        if (thrustForward) bodyInterface_->AddForce(bodyId_, forward * kThrustForce);
        if (thrustBackward) bodyInterface_->AddForce(bodyId_, forward * -kThrustForce);
        if (turnLeft) bodyInterface_->AddTorque(bodyId_, JPH::Vec3(0.0f, -kTurnTorque, 0.0f));
        if (turnRight) bodyInterface_->AddTorque(bodyId_, JPH::Vec3(0.0f, kTurnTorque, 0.0f));
    }

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
    // trying to correct, which looked wrong in testing (still true with real
    // physics, if anything more so).
    void AutoPilot(Vec3 island1Dock, Vec3 island2Dock, Vec3 island3Dock) {
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
            bodyInterface_->AddTorque(bodyId_, JPH::Vec3(0.0f, kTurnTorque, 0.0f));
        } else if (angleDiff < -kAngleThreshold) {
            bodyInterface_->AddTorque(bodyId_, JPH::Vec3(0.0f, -kTurnTorque, 0.0f));
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

    // Clamps linear speed as a safety net (see kMaxSpeed) — called once per
    // fixed step, after PhysicsSystem::Update() has integrated this step's
    // forces, before HandleDocking() reads the settled position.
    void ClampSpeed() {
        JPH::Vec3 v = bodyInterface_->GetLinearVelocity(bodyId_);
        float speed = std::sqrt(v.GetX() * v.GetX() + v.GetZ() * v.GetZ());
        if (speed > kMaxSpeed) {
            float scale = kMaxSpeed / speed;
            bodyInterface_->SetLinearVelocity(bodyId_, JPH::Vec3(v.GetX() * scale, v.GetY(), v.GetZ() * scale));
        }
    }

    // Game-logic half of what used to be Update(): physics integration now
    // happens once for every body via PhysicsSystem::Update() in main(), so
    // this only handles proximity-based loading/unloading, reading position
    // from the Jolt body. isBot restricts loading/unloading to this ship's
    // assigned routeKind_ — that's what stops an autopiloted ship from
    // opportunistically scooping up the wrong resource and getting stuck. The
    // player's ship always passes isBot=false: a human pilot can dock
    // anywhere and load whatever's there, no restriction.
    void HandleDocking(Warehouse& island1Warehouse, Warehouse& island2Warehouse, Market& market, Economy& economy,
                        Port& port, Vec3 island1Dock, Vec3 island2Dock, Vec3 island3Dock, bool isBot) {
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

    Vec3 position() const {
        JPH::RVec3 p = bodyInterface_->GetCenterOfMassPosition(bodyId_);
        return Vec3(static_cast<float>(p.GetX()), static_cast<float>(p.GetY()), static_cast<float>(p.GetZ()));
    }
    Vec3 velocity() const {
        JPH::Vec3 v = bodyInterface_->GetLinearVelocity(bodyId_);
        return Vec3(v.GetX(), v.GetY(), v.GetZ());
    }
    float heading() const { return CurrentHeading(); }
    float angularVelocity() const { return bodyInterface_->GetAngularVelocity(bodyId_).GetY(); }
    int cargo() const { return cargo_; }
    Resource cargoResource() const { return cargoResource_; }
    RouteKind routeKind() const { return routeKind_; }

    void SetState(Vec3 position, Vec3 velocity, float heading, float angularVelocityY, int cargo,
                  Resource cargoResource) {
        JPH::Quat rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), heading);
        bodyInterface_->SetPositionAndRotation(bodyId_, JPH::RVec3(position.x, position.y, position.z), rotation,
                                                JPH::EActivation::Activate);
        bodyInterface_->SetLinearVelocity(bodyId_, JPH::Vec3(velocity.x, velocity.y, velocity.z));
        bodyInterface_->SetAngularVelocity(bodyId_, JPH::Vec3(0.0f, angularVelocityY, 0.0f));
        cargo_ = cargo;
        cargoResource_ = cargoResource;
    }

private:
    static constexpr float kThrustForce = 45000.0f;
    static constexpr float kTurnTorque = 400000.0f;
    static constexpr float kAngleThreshold = 0.15f;
    static constexpr float kThrustAngleLimit = 1.2f;
    static constexpr float kBrakingDistance = 260.0f;

    float CurrentHeading() const {
        JPH::Vec3 forward = bodyInterface_->GetRotation(bodyId_) * JPH::Vec3::sAxisX();
        return std::atan2(forward.GetZ(), forward.GetX());
    }

    JPH::BodyInterface* bodyInterface_;
    JPH::BodyID bodyId_;
    int capacity_;
    int cargo_ = 0;
    Resource cargoResource_ = Resource::Iron;
    RouteKind routeKind_;
};

// --- Fase 7.0: Jolt Physics world setup. Layer/filter boilerplate follows
// Jolt's own "HelloWorld" sample structure — validated separately (a static
// floor + a box falling under gravity and settling on it) before wiring
// CargoShip to a real rigid body. ---

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
    JoltBroadPhaseLayerInterface() {
        objectToBroadPhase_[JoltLayers::kNonMoving] = JoltBroadPhaseLayers::kNonMoving;
        objectToBroadPhase_[JoltLayers::kMoving] = JoltBroadPhaseLayers::kMoving;
    }
    JPH::uint GetNumBroadPhaseLayers() const override { return JoltBroadPhaseLayers::kNumLayers; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return objectToBroadPhase_[layer];
    }

private:
    JPH::BroadPhaseLayer objectToBroadPhase_[JoltLayers::kNumLayers];
};

class JoltObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer1, JPH::BroadPhaseLayer layer2) const override {
        if (layer1 == JoltLayers::kNonMoving) return layer2 == JoltBroadPhaseLayers::kMoving;
        return true;
    }
};

class JoltObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object1, JPH::ObjectLayer object2) const override {
        if (object1 == JoltLayers::kNonMoving) return object2 == JoltLayers::kMoving;
        return true;
    }
};

// Creates a ship-hull rigid body: dynamic, gravity disabled, translation/
// rotation constrained to the X/Z plane + yaw (no water simulation yet — see
// CargoShip's class comment). Damping stands in for hull drag until Fase 7.1
// adds real buoyancy/wave forces.
JPH::BodyID CreateShipBody(JPH::BodyInterface& bodyInterface, Vec3 startPos) {
    JPH::BoxShapeSettings shapeSettings(JPH::Vec3(24.0f, 8.0f, 12.0f));
    JPH::ShapeRefC shape = shapeSettings.Create().Get();
    JPH::BodyCreationSettings settings(shape, JPH::RVec3(startPos.x, startPos.y, startPos.z), JPH::Quat::sIdentity(),
                                        JPH::EMotionType::Dynamic, JoltLayers::kMoving);
    settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationZ |
                             JPH::EAllowedDOFs::RotationY;
    settings.mGravityFactor = 0.0f;
    settings.mLinearDamping = 0.6f;
    settings.mAngularDamping = 0.9f;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = 5000.0f;
    return bodyInterface.CreateAndAddBody(settings, JPH::EActivation::Activate);
}

void AccumulateAfloat(const std::vector<CargoShip>& ships, int& ironAfloat, int& steelAfloat) {
    for (const CargoShip& s : ships) {
        if (s.cargo() <= 0) continue;
        if (s.cargoResource() == Resource::Iron) {
            ironAfloat += s.cargo();
        } else {
            steelAfloat += s.cargo();
        }
    }
}

// Invariant that must hold every simulated hour: every unit of Iron/Steel
// ever produced is accounted for either in an island's warehouse, riding a
// ship (player's or the AI company's — they draw from the same two
// warehouses, see Fase 5.1), or already exported.
bool CheckMaterialBalance(const IronMine& mine, const SteelMill& mill, const Warehouse& island1Warehouse,
                           const Warehouse& island2Warehouse, const std::vector<CargoShip>& playerShips,
                           const std::vector<CargoShip>& aiShips, const Port& port, int hour) {
    int ironAfloat = 0;
    int steelAfloat = 0;
    AccumulateAfloat(playerShips, ironAfloat, steelAfloat);
    AccumulateAfloat(aiShips, ironAfloat, steelAfloat);

    int ironBalance = mine.totalProduced() - mill.totalConsumed() - island1Warehouse.Get(Resource::Iron) -
                       island2Warehouse.Get(Resource::Iron) - ironAfloat;
    int steelBalance =
        mill.totalProduced() - island2Warehouse.Get(Resource::Steel) - steelAfloat - port.totalExported();

    if (ironBalance != 0 || steelBalance != 0) {
        std::cerr << "[hour " << hour << "] BALANCE VIOLATION: ironBalance=" << ironBalance
                   << " steelBalance=" << steelBalance << "\n";
        return false;
    }
    return true;
}

// Simple rule-based expansion, not a learned/adaptive AI — honest for a
// first sub-phase (principio 19: "decisiones de IA emergen de objetivos...
// cuando sea practico" is aspirational for a later Fase 5.x, not this one).
// Buys on whichever leg has fewer of its own ships, so it doesn't lopsidedly
// pile onto one route. Same $500 cost, same capacity as the player pays —
// no hidden bonuses (principios 16-18).
void RunAiDecisionLogic(JPH::BodyInterface& bodyInterface, Economy& aiEconomy, std::vector<CargoShip>& aiShips,
                         int shipCapacity, double shipCost, Vec3 island1Dock, Vec3 island2Dock, int maxShips) {
    if (static_cast<int>(aiShips.size()) >= maxShips) return;
    if (aiEconomy.cash() < shipCost * 2.0) return;

    int ironShips = 0, steelShips = 0;
    for (const CargoShip& s : aiShips) {
        if (s.routeKind() == RouteKind::IronRoute) {
            ++ironShips;
        } else {
            ++steelShips;
        }
    }

    if (ironShips <= steelShips) {
        aiShips.emplace_back(bodyInterface, CreateShipBody(bodyInterface, island1Dock), shipCapacity,
                              RouteKind::IronRoute);
    } else {
        aiShips.emplace_back(bodyInterface, CreateShipBody(bodyInterface, island2Dock), shipCapacity,
                              RouteKind::SteelRoute);
    }
    aiEconomy.ChargeExpense(shipCost);
    std::cout << "AI company bought a ship (fleet size now " << aiShips.size() << ")\n";
}

// --- Save/Load: plain-text state dump. Debugging tool first, save format
// second — a human can open this file and see exactly what broke. ---

constexpr const char* kSaveFile = "archipelago_save.txt";
constexpr const char* kSaveHeader = "ARCHIPELAGO_SAVE_V5";

void WriteFleet(std::ofstream& out, const std::vector<CargoShip>& ships) {
    out << ships.size() << "\n";
    for (const CargoShip& s : ships) {
        Vec3 pos = s.position();
        Vec3 vel = s.velocity();
        out << pos.x << " " << pos.y << " " << pos.z << " " << vel.x << " " << vel.y << " " << vel.z << " "
            << s.heading() << " " << s.angularVelocity() << " " << s.cargo() << " "
            << static_cast<int>(s.cargoResource()) << " " << static_cast<int>(s.routeKind()) << "\n";
    }
}

bool ReadFleet(JPH::BodyInterface& bodyInterface, std::ifstream& in, std::vector<CargoShip>& outShips,
               int shipCapacity) {
    size_t shipCount = 0;
    in >> shipCount;
    std::vector<CargoShip> loaded;
    for (size_t i = 0; i < shipCount && in; ++i) {
        Vec3 pos{}, vel{};
        float heading = 0.0f, angularVelocity = 0.0f;
        int cargo = 0;
        int cargoResourceInt = 0;
        int routeKindInt = 0;
        in >> pos.x >> pos.y >> pos.z >> vel.x >> vel.y >> vel.z >> heading >> angularVelocity >> cargo >>
            cargoResourceInt >> routeKindInt;

        RouteKind kind =
            (routeKindInt == static_cast<int>(RouteKind::IronRoute)) ? RouteKind::IronRoute : RouteKind::SteelRoute;
        Resource cargoResource =
            (cargoResourceInt == static_cast<int>(Resource::Iron)) ? Resource::Iron : Resource::Steel;

        CargoShip s(bodyInterface, CreateShipBody(bodyInterface, pos), shipCapacity, kind);
        s.SetState(pos, vel, heading, angularVelocity, cargo, cargoResource);
        loaded.push_back(s);
    }
    if (!in || loaded.size() != shipCount) return false;
    outShips = std::move(loaded);
    return true;
}

void SaveGame(int hour, double hourAccumulator, const IronMine& mine, const SteelMill& mill,
              const Warehouse& island1Warehouse, const Warehouse& island2Warehouse,
              const std::vector<CargoShip>& playerShips, const std::vector<CargoShip>& aiShips, const Port& port,
              const Market& market, const Economy& economy, const Economy& aiEconomy) {
    std::ofstream out(kSaveFile);
    if (!out) {
        std::cerr << "SaveGame: could not open " << kSaveFile << " for writing\n";
        return;
    }
    out << kSaveHeader << "\n";
    out << hour << " " << hourAccumulator << "\n";
    out << mine.totalProduced() << "\n";
    out << mill.totalConsumed() << " " << mill.totalProduced() << " " << (mill.isIdle() ? 1 : 0) << "\n";
    out << island1Warehouse.Get(Resource::Iron) << " " << island1Warehouse.Get(Resource::Steel) << "\n";
    out << island2Warehouse.Get(Resource::Iron) << " " << island2Warehouse.Get(Resource::Steel) << "\n";
    out << port.totalExported() << "\n";
    out << market.stock() << " " << market.totalRevenue() << " " << market.totalSold() << "\n";
    out << economy.cash() << " " << economy.totalExpenses() << " " << economy.totalRevenue() << "\n";
    out << aiEconomy.cash() << " " << aiEconomy.totalExpenses() << " " << aiEconomy.totalRevenue() << "\n";
    WriteFleet(out, playerShips);
    WriteFleet(out, aiShips);
    std::cout << "Game saved to " << kSaveFile << " (hour " << hour << ", " << playerShips.size()
               << " player ships, " << aiShips.size() << " AI ships)\n";
}

bool LoadGame(JPH::BodyInterface& bodyInterface, int& hour, double& hourAccumulator, IronMine& mine, SteelMill& mill,
              Warehouse& island1Warehouse, Warehouse& island2Warehouse, std::vector<CargoShip>& playerShips,
              std::vector<CargoShip>& aiShips, Port& port, Market& market, Economy& economy, Economy& aiEconomy,
              int shipCapacity) {
    std::ifstream in(kSaveFile);
    if (!in) {
        std::cerr << "LoadGame: could not open " << kSaveFile << "\n";
        return false;
    }
    std::string header;
    std::getline(in, header);
    if (header != kSaveHeader) {
        std::cerr << "LoadGame: unrecognized save file header (old save from a previous phase?)\n";
        return false;
    }

    int mineProduced = 0;
    int millConsumed = 0, millProduced = 0, millIdleFlag = 0;
    int island1Iron = 0, island1Steel = 0;
    int island2Iron = 0, island2Steel = 0;
    int exported = 0;
    float marketStock = 0.0f;
    double marketRevenue = 0.0;
    int marketSold = 0;
    double cash = 0.0, totalExpenses = 0.0, totalRevenue = 0.0;
    double aiCash = 0.0, aiTotalExpenses = 0.0, aiTotalRevenue = 0.0;

    in >> hour >> hourAccumulator;
    in >> mineProduced;
    in >> millConsumed >> millProduced >> millIdleFlag;
    in >> island1Iron >> island1Steel;
    in >> island2Iron >> island2Steel;
    in >> exported;
    in >> marketStock >> marketRevenue >> marketSold;
    in >> cash >> totalExpenses >> totalRevenue;
    in >> aiCash >> aiTotalExpenses >> aiTotalRevenue;

    std::vector<CargoShip> loadedPlayerShips;
    std::vector<CargoShip> loadedAiShips;
    bool okPlayer = ReadFleet(bodyInterface, in, loadedPlayerShips, shipCapacity);
    bool okAi = ReadFleet(bodyInterface, in, loadedAiShips, shipCapacity);

    if (!in || !okPlayer || !okAi) {
        std::cerr << "LoadGame: save file is truncated or malformed\n";
        return false;
    }

    mine.SetTotalProduced(mineProduced);
    mill.SetTotals(millConsumed, millProduced, millIdleFlag != 0);
    island1Warehouse.SetStock(Resource::Iron, island1Iron);
    island1Warehouse.SetStock(Resource::Steel, island1Steel);
    island2Warehouse.SetStock(Resource::Iron, island2Iron);
    island2Warehouse.SetStock(Resource::Steel, island2Steel);
    port.SetTotalExported(exported);
    market.SetState(marketStock, marketRevenue, marketSold);
    economy.SetState(cash, totalExpenses, totalRevenue);
    aiEconomy.SetState(aiCash, aiTotalExpenses, aiTotalRevenue);
    playerShips = std::move(loadedPlayerShips);
    aiShips = std::move(loadedAiShips);

    std::cout << "Game loaded from " << kSaveFile << " (hour " << hour << ", " << playerShips.size()
               << " player ships, " << aiShips.size() << " AI ships)\n";
    return true;
}

// --- Replay: records the player's resolved input each fixed step, not world
// state — a debugging tool for reproducing a specific run bit-for-bit (see
// "Sistema de Replay" en el vault), not a save format. Only meaningful when
// played back from the same starting state the recording began at (restart
// the app, then start playback immediately) — it doesn't snapshot anything,
// it just re-feeds the same inputs against a fresh simulation.

struct ReplayFrame {
    bool thrustForward = false;
    bool thrustBackward = false;
    bool turnLeft = false;
    bool turnRight = false;
    int buyAction = 0;  // 0 = none, 1 = buy Iron-route ship, 2 = buy Steel-route ship
};

constexpr const char* kReplayFile = "archipelago_replay.txt";
constexpr const char* kReplayHeader = "ARCHIPELAGO_REPLAY_V1";

void SaveReplay(const std::vector<ReplayFrame>& frames) {
    std::ofstream out(kReplayFile);
    if (!out) {
        std::cerr << "SaveReplay: could not open " << kReplayFile << " for writing\n";
        return;
    }
    out << kReplayHeader << "\n";
    out << frames.size() << "\n";
    for (const ReplayFrame& f : frames) {
        out << (f.thrustForward ? 1 : 0) << " " << (f.thrustBackward ? 1 : 0) << " " << (f.turnLeft ? 1 : 0) << " "
            << (f.turnRight ? 1 : 0) << " " << f.buyAction << "\n";
    }
    std::cout << "Replay saved to " << kReplayFile << " (" << frames.size() << " frames)\n";
}

bool LoadReplay(std::vector<ReplayFrame>& frames) {
    std::ifstream in(kReplayFile);
    if (!in) {
        std::cerr << "LoadReplay: could not open " << kReplayFile << "\n";
        return false;
    }
    std::string header;
    std::getline(in, header);
    if (header != kReplayHeader) {
        std::cerr << "LoadReplay: unrecognized replay file header\n";
        return false;
    }
    size_t count = 0;
    in >> count;
    frames.clear();
    frames.reserve(count);
    for (size_t i = 0; i < count && in; ++i) {
        int fwd, back, left, right, buy;
        in >> fwd >> back >> left >> right >> buy;
        ReplayFrame f;
        f.thrustForward = fwd != 0;
        f.thrustBackward = back != 0;
        f.turnLeft = left != 0;
        f.turnRight = right != 0;
        f.buyAction = buy;
        frames.push_back(f);
    }
    if (!in || frames.size() != count) {
        std::cerr << "LoadReplay: replay file is truncated or malformed\n";
        return false;
    }
    std::cout << "Replay loaded from " << kReplayFile << " (" << frames.size() << " frames)\n";
    return true;
}

// --- Rendering: real 3D now (Fase 4.5) — perspective projection, depth
// testing, simple box/hull geometry. Still placeholder shapes (no imported
// models, no lighting) — fidelity stays last priority, but it's a real 3D
// space now, not a 2D trick. ---

GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << "\n";
    }
    return shader;
}

// Flat, unlit color — used only for the dock rings (line loops have no faces
// to shade against a light).
GLuint CreateUnlitShaderProgram() {
    static const char* kVertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 uMVP;
        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";
    static const char* kFragmentSrc = R"(
        #version 330 core
        uniform vec3 uColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(uColor, 1.0);
        }
    )";
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Program link error: " << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Simple directional (Lambertian) shading for boxes — one "sun", no shadows,
// no textures. Cheap enough to add now (no new assets, just per-vertex
// normals + a dot product) and it already reads much better as solid objects
// than flat unlit color. Real lighting/materials/shadows stay deferred —
// this is one rung up the ladder, not the top of it.
GLuint CreateLitShaderProgram() {
    static const char* kVertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        uniform mat4 uMVP;
        uniform mat3 uNormalMatrix;
        out vec3 vNormal;
        void main() {
            vNormal = uNormalMatrix * aNormal;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";
    static const char* kFragmentSrc = R"(
        #version 330 core
        in vec3 vNormal;
        uniform vec3 uColor;
        uniform vec3 uLightDir;  // direction the light travels (sun -> surface)
        out vec4 FragColor;
        void main() {
            vec3 n = normalize(vNormal);
            float diff = max(dot(n, -uLightDir), 0.0);
            vec3 ambient = 0.35 * uColor;
            vec3 diffuse = 0.65 * uColor * diff;
            FragColor = vec4(ambient + diffuse, 1.0);
        }
    )";
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Program link error: " << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// Unit cube, centered at origin, extents -0.5..0.5. Reused (scaled +
// translated + rotated via the model matrix) for every box-shaped thing:
// island buildings, the ship hull, and the water plane (scaled flat and huge).
// 36 vertices, unindexed triangles, position + per-face normal interleaved.
constexpr float kCubeVertices[] = {
    // -Z face (normal 0,0,-1)
    -0.5f, -0.5f, -0.5f, 0, 0, -1,  0.5f, -0.5f, -0.5f, 0, 0, -1,  0.5f, 0.5f, -0.5f, 0, 0, -1,
    0.5f, 0.5f, -0.5f, 0, 0, -1,  -0.5f, 0.5f, -0.5f, 0, 0, -1,  -0.5f, -0.5f, -0.5f, 0, 0, -1,
    // +Z face (normal 0,0,1)
    -0.5f, -0.5f, 0.5f, 0, 0, 1,  0.5f, -0.5f, 0.5f, 0, 0, 1,  0.5f, 0.5f, 0.5f, 0, 0, 1,
    0.5f, 0.5f, 0.5f, 0, 0, 1,  -0.5f, 0.5f, 0.5f, 0, 0, 1,  -0.5f, -0.5f, 0.5f, 0, 0, 1,
    // -X face (normal -1,0,0)
    -0.5f, 0.5f, 0.5f, -1, 0, 0,  -0.5f, 0.5f, -0.5f, -1, 0, 0,  -0.5f, -0.5f, -0.5f, -1, 0, 0,
    -0.5f, -0.5f, -0.5f, -1, 0, 0,  -0.5f, -0.5f, 0.5f, -1, 0, 0,  -0.5f, 0.5f, 0.5f, -1, 0, 0,
    // +X face (normal 1,0,0)
    0.5f, 0.5f, 0.5f, 1, 0, 0,  0.5f, 0.5f, -0.5f, 1, 0, 0,  0.5f, -0.5f, -0.5f, 1, 0, 0,
    0.5f, -0.5f, -0.5f, 1, 0, 0,  0.5f, -0.5f, 0.5f, 1, 0, 0,  0.5f, 0.5f, 0.5f, 1, 0, 0,
    // -Y face (normal 0,-1,0)
    -0.5f, -0.5f, -0.5f, 0, -1, 0,  0.5f, -0.5f, -0.5f, 0, -1, 0,  0.5f, -0.5f, 0.5f, 0, -1, 0,
    0.5f, -0.5f, 0.5f, 0, -1, 0,  -0.5f, -0.5f, 0.5f, 0, -1, 0,  -0.5f, -0.5f, -0.5f, 0, -1, 0,
    // +Y face (normal 0,1,0)
    -0.5f, 0.5f, -0.5f, 0, 1, 0,  0.5f, 0.5f, -0.5f, 0, 1, 0,  0.5f, 0.5f, 0.5f, 0, 1, 0,
    0.5f, 0.5f, 0.5f, 0, 1, 0,  -0.5f, 0.5f, 0.5f, 0, 1, 0,  -0.5f, 0.5f, -0.5f, 0, 1, 0,
};
constexpr int kCubeVertexCount = 36;
constexpr int kCubeFloatsPerVertex = 6;  // position (3) + normal (3)

void DrawBox(GLuint cubeVao, GLint mvpLoc, GLint normalMatrixLoc, GLint colorLoc, const glm::mat4& viewProj,
             Vec3 center, Vec3 fullExtents, float headingRadians, float r, float g, float b) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    // Local +X is the model's "forward"; rotate by -heading so it lines up
    // with the world-forward convention used everywhere else
    // (cos(heading), 0, sin(heading)) — see CargoShip::ApplyInput.
    model = glm::rotate(model, -headingRadians, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, fullExtents);
    glm::mat4 mvp = viewProj * model;
    // Non-uniform scale (buildings/ship/water all have different extents per
    // axis) means normals need the inverse-transpose, not the model matrix
    // directly, or they'd skew and lighting would look wrong on stretched boxes.
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniform3f(colorLoc, r, g, b);
    glBindVertexArray(cubeVao);
    glDrawArrays(GL_TRIANGLES, 0, kCubeVertexCount);
}

constexpr int kDockRingSegments = 32;

// Marks the actual loading/unloading zone (the same radius Update() checks)
// as a ring on the water, so it's visible where a ship needs to be, not just
// where the building happens to sit.
void DrawDockRing(GLuint ringVao, GLuint ringVbo, GLint mvpLoc, GLint colorLoc, const glm::mat4& viewProj,
                   Vec3 center, float radius) {
    float vertices[kDockRingSegments * 3];
    for (int i = 0; i < kDockRingSegments; ++i) {
        float angle = (static_cast<float>(i) / kDockRingSegments) * 2.0f * kPi;
        vertices[i * 3 + 0] = center.x + std::cos(angle) * radius;
        vertices[i * 3 + 1] = 1.0f;
        vertices[i * 3 + 2] = center.z + std::sin(angle) * radius;
    }

    glm::mat4 mvp = viewProj;  // vertices already in world space, model = identity
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
    glBindVertexArray(ringVao);
    glBindBuffer(GL_ARRAY_BUFFER, ringVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_LINE_LOOP, 0, kDockRingSegments);
}

enum class CameraMode { ThirdPerson, FirstPerson };

// Third person: chases behind and above the ship. First person: at the ship's
// position, eye height, looking down its heading. Same forward-vector
// convention as movement (cos(heading), 0, sin(heading)).
glm::mat4 ComputeViewProj(CameraMode mode, Vec3 shipPos, float heading, float aspect) {
    glm::vec3 forward(std::cos(heading), 0.0f, std::sin(heading));
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 eye, target;

    if (mode == CameraMode::ThirdPerson) {
        constexpr float kChaseDistance = 180.0f;
        constexpr float kChaseHeight = 90.0f;
        eye = shipPos - forward * kChaseDistance + up * kChaseHeight;
        target = shipPos + up * 15.0f;
    } else {
        constexpr float kEyeHeight = 14.0f;
        eye = shipPos + up * kEyeHeight;
        target = eye + forward * 10.0f;
    }

    glm::mat4 view = glm::lookAt(eye, target, up);
    glm::mat4 projection = glm::perspective(glm::radians(65.0f), aspect, 1.0f, 50000.0f);
    return projection * view;
}

// Projects a world position through view*projection into screen pixel space,
// for placing ImGui text labels over 3D objects. Returns false if the point
// is behind the camera (label shouldn't be drawn).
bool WorldToScreen(Vec3 worldPos, const glm::mat4& viewProj, ImVec2& outScreen) {
    glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    outScreen.x = (ndc.x * 0.5f + 0.5f) * kWindowWidth;
    outScreen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * kWindowHeight;
    return true;
}


}  // namespace archipelago

int main(int argc, char** argv) {
    using namespace archipelago;
    (void)argc;
    (void)argv;

    // Jolt Physics world — set up once, lives for the whole program. See
    // "Fase 7.0 - Motor de fisica real (Jolt)" en el vault.
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    JPH::TempAllocatorImpl physicsTempAllocator(10 * 1024 * 1024);
    unsigned int physicsWorkerThreads = std::max(
        1u, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency() - 1 : 1u);
    JPH::JobSystemThreadPool physicsJobSystem(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                               static_cast<int>(physicsWorkerThreads));

    JoltBroadPhaseLayerInterface broadPhaseLayerInterface;
    JoltObjectVsBroadPhaseLayerFilter objectVsBroadPhaseFilter;
    JoltObjectLayerPairFilter objectLayerPairFilter;

    JPH::PhysicsSystem physicsSystem;
    physicsSystem.Init(/*maxBodies=*/1024, /*numBodyMutexes=*/0, /*maxBodyPairs=*/1024,
                        /*maxContactConstraints=*/1024, broadPhaseLayerInterface, objectVsBroadPhaseFilter,
                        objectLayerPairFilter);
    JPH::BodyInterface& bodyInterface = physicsSystem.GetBodyInterface();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("Archipelago - Fase 4.5 (3D)", kWindowWidth, kWindowHeight,
                                           SDL_WINDOW_OPENGL);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }
    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewStatus) << "\n";
        return EXIT_FAILURE;
    }

    glViewport(0, 0, kWindowWidth, kWindowHeight);
    glClearColor(0.55f, 0.75f, 0.95f, 1.0f);  // sky
    glEnable(GL_DEPTH_TEST);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplSDL3_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 330");

    GLuint litShaderProgram = CreateLitShaderProgram();
    GLint litColorLoc = glGetUniformLocation(litShaderProgram, "uColor");
    GLint litMvpLoc = glGetUniformLocation(litShaderProgram, "uMVP");
    GLint litNormalMatrixLoc = glGetUniformLocation(litShaderProgram, "uNormalMatrix");
    GLint lightDirLoc = glGetUniformLocation(litShaderProgram, "uLightDir");

    GLuint unlitShaderProgram = CreateUnlitShaderProgram();
    GLint unlitColorLoc = glGetUniformLocation(unlitShaderProgram, "uColor");
    GLint unlitMvpLoc = glGetUniformLocation(unlitShaderProgram, "uMVP");

    GLuint cubeVao = 0, cubeVbo = 0;
    glGenVertexArrays(1, &cubeVao);
    glGenBuffers(1, &cubeVbo);
    glBindVertexArray(cubeVao);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kCubeFloatsPerVertex * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kCubeFloatsPerVertex * sizeof(float),
                           reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint ringVao = 0, ringVbo = 0;
    glGenVertexArrays(1, &ringVao);
    glGenBuffers(1, &ringVbo);
    glBindVertexArray(ringVao);
    glBindBuffer(GL_ARRAY_BUFFER, ringVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * kDockRingSegments * 3, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // Isla 1 (Minera) -> Isla 2 (Industrial) -> Isla 3 (Portuaria). Real
    // distance between them on purpose (thousands of units, not hundreds) —
    // see "la distancia es parte del diseno" en el vault.
    Warehouse island1Warehouse;
    Warehouse island2Warehouse;
    island2Warehouse.Deposit(Resource::Iron, 30);  // small starting stockpile: mill isn't dead on turn one
    IronMine mine(/*rate=*/10);
    mine.SetTotalProduced(30);  // matches the seeded stockpile above, so the balance invariant holds from tick 1
    SteelMill mill(/*consumeRate=*/10, /*outputRatioPercent=*/50);
    Port port;
    Market market(/*basePrice=*/10.0, /*demandPerHour=*/12.0f, /*sensitivity=*/0.05);
    Economy economy(/*startingCash=*/2000.0);
    constexpr double kBaseMaintenanceCost = 3.0;      // mine + mill upkeep, independent of fleet size
    constexpr double kPerShipMaintenanceCost = 5.0;   // each ship (player's included) adds its own upkeep
    constexpr double kShipPurchaseCost = 500.0;
    constexpr int kShipCapacity = 20;

    // Fase 5.1: a rival AI company, same starting cash, same costs, same
    // market — it competes with the player for the same finite Hierro/Acero
    // supply and the same Mercado, no hidden bonuses (principios 16-18).
    Economy aiEconomy(/*startingCash=*/2000.0);
    constexpr int kAiMaxShips = 5;

    const Vec3 minePos{300, 0, 300};
    const Vec3 island1Dock{450, 0, 300};
    const Vec3 millPos{2200, 0, 450};
    const Vec3 island2Dock{2350, 0, 450};
    const Vec3 portPos{4300, 0, 150};
    const Vec3 island3Dock{4450, 0, 150};

    std::vector<CargoShip> playerShips;
    playerShips.emplace_back(bodyInterface, CreateShipBody(bodyInterface, island2Dock), kShipCapacity,
                              RouteKind::SteelRoute);

    std::vector<CargoShip> aiShips;
    aiShips.emplace_back(bodyInterface, CreateShipBody(bodyInterface, island1Dock), kShipCapacity,
                          RouteKind::IronRoute);

    physicsSystem.OptimizeBroadPhase();

    CameraMode cameraMode = CameraMode::ThirdPerson;
    bool cWasDown = false;

    Uint64 lastCounter = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    double hourAccumulator = 0.0;
    double simAccumulator = 0.0;
    int hour = 0;
    bool running = true;
    bool f5WasDown = false;
    bool f9WasDown = false;
    bool f6WasDown = false;
    bool f7WasDown = false;
    std::string lastSaveLoadMessage;
    std::string lastReplayMessage;

    bool isRecording = false;
    bool isPlayingBack = false;
    size_t playbackIndex = 0;
    std::vector<ReplayFrame> recordedFrames;
    int pendingBuyAction = 0;  // set by the Comprar barco buttons, consumed on the next fixed step

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
        }

        Uint64 nowCounter = SDL_GetPerformanceCounter();
        double dt = static_cast<double>(nowCounter - lastCounter) / frequency;
        lastCounter = nowCounter;
        if (dt > 0.25) dt = 0.25;  // clamp: avoid spiral-of-death after a stall/breakpoint

        const bool* keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_ESCAPE]) running = false;

        bool cIsDown = keys[SDL_SCANCODE_C];
        if (cIsDown && !cWasDown) {
            cameraMode = (cameraMode == CameraMode::ThirdPerson) ? CameraMode::FirstPerson : CameraMode::ThirdPerson;
        }
        cWasDown = cIsDown;

        bool f5IsDown = keys[SDL_SCANCODE_F5];
        if (f5IsDown && !f5WasDown) {
            SaveGame(hour, hourAccumulator, mine, mill, island1Warehouse, island2Warehouse, playerShips, aiShips,
                     port, market, economy, aiEconomy);
            lastSaveLoadMessage = "Guardado (hora " + std::to_string(hour) + ")";
        }
        f5WasDown = f5IsDown;

        bool f9IsDown = keys[SDL_SCANCODE_F9];
        if (f9IsDown && !f9WasDown) {
            if (LoadGame(bodyInterface, hour, hourAccumulator, mine, mill, island1Warehouse, island2Warehouse,
                         playerShips, aiShips, port, market, economy, aiEconomy, kShipCapacity)) {
                lastSaveLoadMessage = "Cargado (hora " + std::to_string(hour) + ")";
            } else {
                lastSaveLoadMessage = "Error al cargar (ver consola)";
            }
        }
        f9WasDown = f9IsDown;

        // F6/F7 are meta/debug actions, same as F5/F9 — not part of the
        // recorded replay stream itself, just real-frame-granularity toggles.
        bool f6IsDown = keys[SDL_SCANCODE_F6];
        if (f6IsDown && !f6WasDown) {
            if (!isRecording) {
                isRecording = true;
                isPlayingBack = false;
                recordedFrames.clear();
                lastReplayMessage = "Grabando replay...";
            } else {
                isRecording = false;
                SaveReplay(recordedFrames);
                lastReplayMessage = "Replay guardado (" + std::to_string(recordedFrames.size()) + " pasos)";
            }
        }
        f6WasDown = f6IsDown;

        bool f7IsDown = keys[SDL_SCANCODE_F7];
        if (f7IsDown && !f7WasDown) {
            std::vector<ReplayFrame> loaded;
            if (LoadReplay(loaded)) {
                recordedFrames = std::move(loaded);
                playbackIndex = 0;
                isPlayingBack = true;
                isRecording = false;
                lastReplayMessage = "Reproduciendo replay (" + std::to_string(recordedFrames.size()) + " pasos)";
            } else {
                lastReplayMessage = "Error al cargar replay (ver consola)";
            }
        }
        f7WasDown = f7IsDown;

        // Fixed timestep: the simulation advances in constant kFixedDt
        // chunks regardless of real framerate, so a recorded input sequence
        // always plays back identically (see Replay system above).
        simAccumulator += dt;
        while (simAccumulator >= kFixedDt) {
            simAccumulator -= kFixedDt;

            bool thrustForward, thrustBackward, turnLeft, turnRight;
            int buyAction;
            if (isPlayingBack) {
                if (playbackIndex < recordedFrames.size()) {
                    const ReplayFrame& f = recordedFrames[playbackIndex++];
                    thrustForward = f.thrustForward;
                    thrustBackward = f.thrustBackward;
                    turnLeft = f.turnLeft;
                    turnRight = f.turnRight;
                    buyAction = f.buyAction;
                } else {
                    isPlayingBack = false;
                    lastReplayMessage = "Replay terminado";
                    thrustForward = thrustBackward = turnLeft = turnRight = false;
                    buyAction = 0;
                }
            } else {
                thrustForward = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
                thrustBackward = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
                turnLeft = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
                turnRight = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
                buyAction = pendingBuyAction;
                pendingBuyAction = 0;
            }

            if (isRecording) {
                ReplayFrame f;
                f.thrustForward = thrustForward;
                f.thrustBackward = thrustBackward;
                f.turnLeft = turnLeft;
                f.turnRight = turnRight;
                f.buyAction = buyAction;
                recordedFrames.push_back(f);
            }

            if (buyAction == 1) {
                playerShips.emplace_back(bodyInterface, CreateShipBody(bodyInterface, island1Dock), kShipCapacity,
                                          RouteKind::IronRoute);
                economy.ChargeExpense(kShipPurchaseCost);
            } else if (buyAction == 2) {
                playerShips.emplace_back(bodyInterface, CreateShipBody(bodyInterface, island2Dock), kShipCapacity,
                                          RouteKind::SteelRoute);
                economy.ChargeExpense(kShipPurchaseCost);
            }

            // Ship 0 is always the player's — you're one person, you can only
            // pilot one hull at a time (see "Encarnacion y capa de mando" en
            // el vault). Fly it to any island; loading/unloading just works
            // based on proximity and what you're carrying. Every other ship
            // in the player's fleet runs on autopilot, shuttling its assigned
            // pair of docks — same as every ship in the rival AI company's
            // fleet (Fase 5.1), which is bot-only, no manual control at all.
            playerShips[0].ApplyInput(thrustForward, thrustBackward, turnLeft, turnRight);
            for (size_t i = 1; i < playerShips.size(); ++i) {
                playerShips[i].AutoPilot(island1Dock, island2Dock, island3Dock);
            }
            for (CargoShip& s : aiShips) {
                s.AutoPilot(island1Dock, island2Dock, island3Dock);
            }

            // One physics step for every body in the world, then per-ship
            // game logic reads back the settled position/velocity — see
            // "Fase 7.0" comment on ClampSpeed()/HandleDocking().
            physicsSystem.Update(static_cast<float>(kFixedDt), 1, &physicsTempAllocator, &physicsJobSystem);

            for (size_t i = 0; i < playerShips.size(); ++i) {
                bool isBot = (i != 0);
                playerShips[i].ClampSpeed();
                playerShips[i].HandleDocking(island1Warehouse, island2Warehouse, market, economy, port, island1Dock,
                                              island2Dock, island3Dock, isBot);
            }

            for (CargoShip& s : aiShips) {
                s.ClampSpeed();
                s.HandleDocking(island1Warehouse, island2Warehouse, market, aiEconomy, port, island1Dock, island2Dock,
                                 island3Dock, /*isBot=*/true);
            }

            hourAccumulator += kFixedDt;
            while (hourAccumulator >= kSecondsPerSimulatedHour) {
                hourAccumulator -= kSecondsPerSimulatedHour;
                ++hour;
                mine.Tick(island1Warehouse, hour);
                mill.Tick(island2Warehouse, hour);
                market.Tick(hour);
                double maintenance =
                    kBaseMaintenanceCost + kPerShipMaintenanceCost * static_cast<double>(playerShips.size());
                economy.ChargeExpense(maintenance);
                double aiMaintenance =
                    kBaseMaintenanceCost + kPerShipMaintenanceCost * static_cast<double>(aiShips.size());
                aiEconomy.ChargeExpense(aiMaintenance);
                RunAiDecisionLogic(bodyInterface, aiEconomy, aiShips, kShipCapacity, kShipPurchaseCost, island1Dock,
                                    island2Dock, kAiMaxShips);
                if (!CheckMaterialBalance(mine, mill, island1Warehouse, island2Warehouse, playerShips, aiShips, port,
                                           hour)) {
                    running = false;
                }
            }
        }

        float aspect = static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight);
        glm::mat4 viewProj = ComputeViewProj(cameraMode, playerShips[0].position(), playerShips[0].heading(), aspect);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(litShaderProgram);
        glUniform3f(lightDirLoc, 0.4f, -1.0f, 0.3f);

        // Water: the same cube mesh, scaled flat and huge, centered to cover all three islands.
        DrawBox(cubeVao, litMvpLoc, litNormalMatrixLoc, litColorLoc, viewProj, Vec3{2500, -2, 300},
                Vec3{20000, 4, 20000}, 0.0f, 0.15f, 0.35f, 0.55f);

        DrawBox(cubeVao, litMvpLoc, litNormalMatrixLoc, litColorLoc, viewProj, minePos + Vec3{0, 40, 0},
                Vec3{120, 80, 80}, 0.0f, 0.55f, 0.35f, 0.15f);
        DrawBox(cubeVao, litMvpLoc, litNormalMatrixLoc, litColorLoc, viewProj, millPos + Vec3{0, 40, 0},
                Vec3{120, 80, 80}, 0.0f, 0.5f, 0.5f, 0.55f);
        DrawBox(cubeVao, litMvpLoc, litNormalMatrixLoc, litColorLoc, viewProj, portPos + Vec3{0, 40, 0},
                Vec3{120, 80, 80}, 0.0f, 0.2f, 0.4f, 0.8f);

        for (size_t i = 0; i < playerShips.size(); ++i) {
            const CargoShip& s = playerShips[i];
            // Don't draw your own hull in first-person — you're standing inside it.
            if (i == 0 && cameraMode == CameraMode::FirstPerson) continue;
            DrawBox(cubeVao, litMvpLoc, litNormalMatrixLoc, litColorLoc, viewProj, s.position() + Vec3{0, 8, 0},
                    Vec3{48, 16, 24}, s.heading(), 0.9f, 0.9f, 0.2f);
        }
        for (const CargoShip& s : aiShips) {
            // Rival AI hulls in a distinct reddish color so they read as "not yours" at a glance.
            DrawBox(cubeVao, litMvpLoc, litNormalMatrixLoc, litColorLoc, viewProj, s.position() + Vec3{0, 8, 0},
                    Vec3{48, 16, 24}, s.heading(), 0.85f, 0.25f, 0.2f);
        }

        glUseProgram(unlitShaderProgram);
        DrawDockRing(ringVao, ringVbo, unlitMvpLoc, unlitColorLoc, viewProj, island1Dock, CargoShip::kDockRadius);
        DrawDockRing(ringVao, ringVbo, unlitMvpLoc, unlitColorLoc, viewProj, island2Dock, CargoShip::kDockRadius);
        DrawDockRing(ringVao, ringVbo, unlitMvpLoc, unlitColorLoc, viewProj, island3Dock, CargoShip::kDockRadius);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImDrawList* labels = ImGui::GetForegroundDrawList();
        const ImU32 labelColor = IM_COL32(255, 255, 255, 255);
        ImVec2 screenPos;
        if (WorldToScreen(minePos + Vec3{0, 120, 0}, viewProj, screenPos)) {
            labels->AddText(ImVec2(screenPos.x - 55, screenPos.y), labelColor, "Isla 1: Mina de Hierro");
        }
        if (WorldToScreen(millPos + Vec3{0, 120, 0}, viewProj, screenPos)) {
            labels->AddText(ImVec2(screenPos.x - 45, screenPos.y), labelColor, "Isla 2: Aceria");
        }
        if (WorldToScreen(portPos + Vec3{0, 120, 0}, viewProj, screenPos)) {
            labels->AddText(ImVec2(screenPos.x - 40, screenPos.y), labelColor, "Isla 3: Puerto");
        }

        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
        ImGui::Begin("Estado de la simulacion");
        ImGui::Text("Hora simulada: %d", hour);
        ImGui::Text("Camara: %s (tecla C para cambiar)",
                    cameraMode == CameraMode::ThirdPerson ? "tercera persona" : "primera persona");
        ImGui::Separator();
        ImGui::Text("Isla 1 (Minera) - Hierro: %d", island1Warehouse.Get(Resource::Iron));
        ImGui::Text("Isla 2 (Industrial) - Hierro: %d, Acero: %d", island2Warehouse.Get(Resource::Iron),
                    island2Warehouse.Get(Resource::Steel));
        ImGui::Text("Aceria: %s", mill.isIdle() ? "PARADA (sin Hierro)" : "activa");
        ImGui::Text("Isla 3 (Puerto) - exportado total: %d", port.totalExported());
        ImGui::Separator();
        ImGui::Text("Tu barco - carga: %d %s", playerShips[0].cargo(),
                    ToString(playerShips[0].cargoResource()).c_str());
        ImGui::Separator();
        ImGui::Text("Hierro producido total: %d", mine.totalProduced());
        ImGui::Text("Acero producido total:  %d", mill.totalProduced());
        ImGui::Separator();
        ImGui::Text("Caja: $%.2f", economy.cash());
        ImGui::Text("Precio Acero: $%.2f/unidad", market.CurrentPrice());
        ImGui::Text("Stock de mercado: %.1f", market.stock());
        ImGui::Text("Ingresos totales (mercado): $%.2f", market.totalRevenue());
        double currentMaintenance =
            kBaseMaintenanceCost + kPerShipMaintenanceCost * static_cast<double>(playerShips.size());
        ImGui::Text("Gastos totales: $%.2f (mantenimiento $%.0f/hora con %zu barco%s)", economy.totalExpenses(),
                    currentMaintenance, playerShips.size(), playerShips.size() == 1 ? "" : "s");
        ImGui::Separator();
        ImGui::Text("Flota: %zu barco%s (autopilotados todos salvo el tuyo)", playerShips.size(),
                    playerShips.size() == 1 ? "" : "s");
        ImGui::BeginDisabled(economy.cash() < kShipPurchaseCost);
        if (ImGui::Button("Comprar barco - Ruta Hierro Isla1->2 ($500)")) {
            pendingBuyAction = 1;
            lastSaveLoadMessage = "";
        }
        if (ImGui::Button("Comprar barco - Ruta Acero Isla2->3 ($500)")) {
            pendingBuyAction = 2;
            lastSaveLoadMessage = "";
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::Text("Empresa rival (IA):");
        ImGui::Text("Caja: $%.2f", aiEconomy.cash());
        ImGui::Text("Ingresos: $%.2f   Gastos: $%.2f", aiEconomy.totalRevenue(), aiEconomy.totalExpenses());
        int aiIronShips = 0, aiSteelShips = 0;
        for (const CargoShip& s : aiShips) {
            if (s.routeKind() == RouteKind::IronRoute) {
                ++aiIronShips;
            } else {
                ++aiSteelShips;
            }
        }
        ImGui::Text("Flota: %zu barcos (%d ruta Hierro, %d ruta Acero)", aiShips.size(), aiIronShips, aiSteelShips);
        ImGui::Separator();
        ImGui::Text("F5: guardar   F9: cargar");
        if (!lastSaveLoadMessage.empty()) {
            ImGui::Text("%s", lastSaveLoadMessage.c_str());
        }
        ImGui::Separator();
        ImGui::Text("F6: grabar/parar replay   F7: reproducir replay");
        if (isRecording) {
            ImGui::Text("Grabando... (%zu pasos)", recordedFrames.size());
        }
        if (isPlayingBack) {
            ImGui::Text("Reproduciendo... (%zu/%zu)", playbackIndex, recordedFrames.size());
        }
        if (!lastReplayMessage.empty()) {
            ImGui::Text("%s", lastReplayMessage.c_str());
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    std::cout << "\n=== Simulation finished ===\n";
    std::cout << "Total Iron produced: " << mine.totalProduced() << "\n";
    std::cout << "Total Steel produced: " << mill.totalProduced() << "\n";
    std::cout << "Total Steel exported: " << port.totalExported() << "\n";
    std::cout << "Total revenue (market): $" << market.totalRevenue() << "\n";
    std::cout << "Player - expenses: $" << economy.totalExpenses() << ", final cash: $" << economy.cash() << "\n";
    std::cout << "AI company - expenses: $" << aiEconomy.totalExpenses() << ", final cash: $" << aiEconomy.cash()
               << ", fleet size: " << aiShips.size() << "\n";

    glDeleteBuffers(1, &cubeVbo);
    glDeleteVertexArrays(1, &cubeVao);
    glDeleteBuffers(1, &ringVbo);
    glDeleteVertexArrays(1, &ringVao);
    glDeleteProgram(litShaderProgram);
    glDeleteProgram(unlitShaderProgram);
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    return EXIT_SUCCESS;
}
