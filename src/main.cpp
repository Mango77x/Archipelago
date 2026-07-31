#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
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

    void AddRevenue(double amount) { cash_ += amount; }
    void ChargeExpense(double amount) {
        cash_ -= amount;
        totalExpenses_ += amount;
    }

    double cash() const { return cash_; }
    double totalExpenses() const { return totalExpenses_; }

    void SetState(double cash, double totalExpenses) {
        cash_ = cash;
        totalExpenses_ = totalExpenses;
    }

private:
    double cash_;
    double totalExpenses_ = 0.0;
};

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

// Wraps an angle (radians) to (-pi, pi]. Used to find the shortest turn
// direction toward a target heading.
float NormalizeAngle(float angle) {
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

// Player-piloted (or autopiloted) cargo ship: simple 2D kinematic model
// (thrust + drag + angular inertia), not a physics engine — enough to feel
// like a boat without pulling in Jolt before the gameplay actually needs
// collisions (see CLAUDE.md / vault).
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

    CargoShip(int capacity, Vec2 startPos, RouteKind routeKind)
        : capacity_(capacity), position_(startPos), routeKind_(routeKind) {}

    // Takes resolved intent flags rather than raw SDL key state, so the same
    // call works whether the flags came from a live keyboard this frame or
    // from a recorded ReplayFrame during playback (see Replay system below) —
    // CargoShip doesn't need to know or care which.
    void ApplyInput(bool thrustForward, bool thrustBackward, bool turnLeft, bool turnRight, float dt) {
        constexpr float kThrust = 220.0f;
        constexpr float kTurnRate = 2.5f;

        if (thrustForward) {
            velocity_.x += std::cos(heading_) * kThrust * dt;
            velocity_.y += std::sin(heading_) * kThrust * dt;
        }
        if (thrustBackward) {
            velocity_.x -= std::cos(heading_) * kThrust * dt;
            velocity_.y -= std::sin(heading_) * kThrust * dt;
        }
        if (turnLeft) angularVelocity_ -= kTurnRate * dt;
        if (turnRight) angularVelocity_ += kTurnRate * dt;
    }

    // Autonomous ships steer with the same thrust/turn-rate model as the
    // player (principle: the same rules apply, no special-cased movement for
    // AI-controlled assets). Target is the ship's assigned pair of docks,
    // picked by cargo state: empty means "go get more," carrying means "go
    // deliver."
    //
    // This is "arrive" steering, not just "seek": the desired speed tapers
    // down as distance to the target shrinks, and the ship actively brakes
    // (thrust opposing current velocity) whenever it's going faster than
    // that — otherwise it reaches the dock at full speed and skids/overshoots
    // trying to correct, which looked wrong in testing.
    void AutoPilot(float dt, Vec2 island1Dock, Vec2 island2Dock, Vec2 island3Dock) {
        constexpr float kThrust = 220.0f;
        constexpr float kTurnRate = 2.5f;
        constexpr float kAngleThreshold = 0.15f;
        constexpr float kThrustAngleLimit = 1.2f;
        constexpr float kBrakingDistance = 260.0f;

        Vec2 origin = (routeKind_ == RouteKind::IronRoute) ? island1Dock : island2Dock;
        Vec2 destination = (routeKind_ == RouteKind::IronRoute) ? island2Dock : island3Dock;
        Vec2 target = (cargo_ <= 0) ? origin : destination;

        float dx = target.x - position_.x;
        float dy = target.y - position_.y;
        float distance = std::sqrt(dx * dx + dy * dy);
        if (distance < 1.0f) return;

        float desiredHeading = std::atan2(dy, dx);
        float angleDiff = NormalizeAngle(desiredHeading - heading_);

        if (angleDiff > kAngleThreshold) {
            angularVelocity_ += kTurnRate * dt;
        } else if (angleDiff < -kAngleThreshold) {
            angularVelocity_ -= kTurnRate * dt;
        }

        float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y);
        float desiredSpeed = kMaxSpeed * std::min(1.0f, distance / kBrakingDistance);

        if (speed > desiredSpeed) {
            float newSpeed = std::max(desiredSpeed, speed - kThrust * dt);
            if (speed > 0.01f) {
                float scale = newSpeed / speed;
                velocity_.x *= scale;
                velocity_.y *= scale;
            }
        } else if (std::abs(angleDiff) < kThrustAngleLimit) {
            velocity_.x += std::cos(heading_) * kThrust * dt;
            velocity_.y += std::sin(heading_) * kThrust * dt;
        }
    }

    // isBot restricts loading/unloading to this ship's assigned routeKind_ —
    // that's what stops an autopiloted ship from opportunistically scooping
    // up the wrong resource and getting stuck (see comment below). The
    // player's ship always passes isBot=false: a human pilot can dock
    // anywhere and load whatever's there, no restriction.
    void Update(float dt, Warehouse& island1Warehouse, Warehouse& island2Warehouse, Market& market,
                Economy& economy, Port& port, Vec2 island1Dock, Vec2 island2Dock, Vec2 island3Dock, bool isBot) {
        constexpr float kLinearDrag = 0.6f;
        constexpr float kAngularDrag = 0.85f;

        velocity_.x *= std::pow(1.0f - kLinearDrag, dt);
        velocity_.y *= std::pow(1.0f - kLinearDrag, dt);
        angularVelocity_ *= std::pow(1.0f - kAngularDrag, dt);

        float speed = std::sqrt(velocity_.x * velocity_.x + velocity_.y * velocity_.y);
        if (speed > kMaxSpeed) {
            velocity_.x = velocity_.x / speed * kMaxSpeed;
            velocity_.y = velocity_.y / speed * kMaxSpeed;
        }

        position_.x += velocity_.x * dt;
        position_.y += velocity_.y * dt;
        heading_ += angularVelocity_ * dt;

        // For bots, routeKind_ is a hold configuration, not just a steering
        // hint — an Iron-route bot only ever loads/carries Iron. Without
        // this, a bot that just delivered Iron at Isla 2 and is still
        // sitting right there would immediately scoop up Steel it was never
        // meant to carry, and then never leave (its "Iron route" destination
        // IS Isla 2). The player isn't gated by this — fly anywhere, load
        // whatever's there.
        bool canHandleIron = !isBot || routeKind_ == RouteKind::IronRoute;
        bool canHandleSteel = !isBot || routeKind_ == RouteKind::SteelRoute;

        // Isla 1 (Mina): pick up Iron if the hold is empty.
        if (canHandleIron && cargo_ <= 0 && Distance(position_, island1Dock) <= kDockRadius) {
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
        if (Distance(position_, island2Dock) <= kDockRadius) {
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
        if (cargo_ > 0 && cargoResource_ == Resource::Steel && Distance(position_, island3Dock) <= kDockRadius) {
            double revenue = market.Sell(cargo_);
            economy.AddRevenue(revenue);
            port.Export(cargo_);
            std::cout << "Cargo Ship sold " << cargo_ << " Steel for $" << revenue << "\n";
            cargo_ = 0;
        }
    }

    static float Distance(Vec2 a, Vec2 b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    Vec2 position() const { return position_; }
    Vec2 velocity() const { return velocity_; }
    float heading() const { return heading_; }
    float angularVelocity() const { return angularVelocity_; }
    int cargo() const { return cargo_; }
    Resource cargoResource() const { return cargoResource_; }
    RouteKind routeKind() const { return routeKind_; }

    void SetState(Vec2 position, Vec2 velocity, float heading, float angularVelocity, int cargo,
                  Resource cargoResource) {
        position_ = position;
        velocity_ = velocity;
        heading_ = heading;
        angularVelocity_ = angularVelocity;
        cargo_ = cargo;
        cargoResource_ = cargoResource;
    }

private:
    static constexpr float kMaxSpeed = 160.0f;

    int capacity_;
    Vec2 position_;
    Vec2 velocity_;
    float heading_ = 0.0f;
    float angularVelocity_ = 0.0f;
    int cargo_ = 0;
    Resource cargoResource_ = Resource::Iron;
    RouteKind routeKind_;
};

// Invariant that must hold every simulated hour: every unit of Iron/Steel
// ever produced is accounted for either in an island's warehouse, riding a
// ship, or already exported. Phase 0's one failure mode to catch, still true
// across two warehouses and a fleet in Phase 4.
bool CheckMaterialBalance(const IronMine& mine, const SteelMill& mill, const Warehouse& island1Warehouse,
                           const Warehouse& island2Warehouse, const std::vector<CargoShip>& ships, const Port& port,
                           int hour) {
    int ironAfloat = 0;
    int steelAfloat = 0;
    for (const CargoShip& s : ships) {
        if (s.cargo() <= 0) continue;
        if (s.cargoResource() == Resource::Iron) {
            ironAfloat += s.cargo();
        } else {
            steelAfloat += s.cargo();
        }
    }

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

// --- Save/Load: plain-text state dump. Debugging tool first, save format
// second — a human can open this file and see exactly what broke. ---

constexpr const char* kSaveFile = "archipelago_save.txt";
constexpr const char* kSaveHeader = "ARCHIPELAGO_SAVE_V3";

void SaveGame(int hour, double hourAccumulator, const IronMine& mine, const SteelMill& mill,
              const Warehouse& island1Warehouse, const Warehouse& island2Warehouse,
              const std::vector<CargoShip>& ships, const Port& port, const Market& market, const Economy& economy) {
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
    out << economy.cash() << " " << economy.totalExpenses() << "\n";
    out << ships.size() << "\n";
    for (const CargoShip& s : ships) {
        Vec2 pos = s.position();
        Vec2 vel = s.velocity();
        out << pos.x << " " << pos.y << " " << vel.x << " " << vel.y << " " << s.heading() << " "
            << s.angularVelocity() << " " << s.cargo() << " " << static_cast<int>(s.cargoResource()) << " "
            << static_cast<int>(s.routeKind()) << "\n";
    }
    std::cout << "Game saved to " << kSaveFile << " (hour " << hour << ", " << ships.size() << " ships)\n";
}

bool LoadGame(int& hour, double& hourAccumulator, IronMine& mine, SteelMill& mill, Warehouse& island1Warehouse,
              Warehouse& island2Warehouse, std::vector<CargoShip>& ships, Port& port, Market& market,
              Economy& economy, int shipCapacity) {
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
    double cash = 0.0, totalExpenses = 0.0;
    size_t shipCount = 0;

    in >> hour >> hourAccumulator;
    in >> mineProduced;
    in >> millConsumed >> millProduced >> millIdleFlag;
    in >> island1Iron >> island1Steel;
    in >> island2Iron >> island2Steel;
    in >> exported;
    in >> marketStock >> marketRevenue >> marketSold;
    in >> cash >> totalExpenses;
    in >> shipCount;

    std::vector<CargoShip> loadedShips;
    for (size_t i = 0; i < shipCount && in; ++i) {
        Vec2 pos{}, vel{};
        float heading = 0.0f, angularVelocity = 0.0f;
        int cargo = 0;
        int cargoResourceInt = 0;
        int routeKindInt = 0;
        in >> pos.x >> pos.y >> vel.x >> vel.y >> heading >> angularVelocity >> cargo >> cargoResourceInt >>
            routeKindInt;

        RouteKind kind =
            (routeKindInt == static_cast<int>(RouteKind::IronRoute)) ? RouteKind::IronRoute : RouteKind::SteelRoute;
        Resource cargoResource =
            (cargoResourceInt == static_cast<int>(Resource::Iron)) ? Resource::Iron : Resource::Steel;

        CargoShip s(shipCapacity, pos, kind);
        s.SetState(pos, vel, heading, angularVelocity, cargo, cargoResource);
        loadedShips.push_back(s);
    }

    if (!in || loadedShips.size() != shipCount) {
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
    economy.SetState(cash, totalExpenses);
    ships = std::move(loadedShips);

    std::cout << "Game loaded from " << kSaveFile << " (hour " << hour << ", " << ships.size() << " ships)\n";
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

// --- Rendering (placeholder: flat-colored quads/triangle, throwaway per Fase 1) ---

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

GLuint CreateShaderProgram() {
    static const char* kVertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec2 aPos;
        void main() {
            gl_Position = vec4(aPos, 0.0, 1.0);
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

// World space is no longer the same thing as screen space (Fase 4): islands
// are thousands of units apart, so everything is drawn relative to a camera
// that follows the player's ship. Camera position maps to the center of the
// screen.
Vec2 ToNdc(Vec2 worldPos, Vec2 cameraPos) {
    float relX = worldPos.x - cameraPos.x;
    float relY = worldPos.y - cameraPos.y;
    return {relX / (kWindowWidth * 0.5f), -(relY / (kWindowHeight * 0.5f))};
}

ImVec2 WorldToScreen(Vec2 worldPos, Vec2 cameraPos) {
    return ImVec2((worldPos.x - cameraPos.x) + kWindowWidth * 0.5f, (worldPos.y - cameraPos.y) + kWindowHeight * 0.5f);
}

void DrawQuad(GLuint vao, GLuint vbo, Vec2 center, float halfW, float halfH, float r, float g, float b,
              GLint colorLoc, Vec2 cameraPos) {
    Vec2 c0 = ToNdc({center.x - halfW, center.y - halfH}, cameraPos);
    Vec2 c1 = ToNdc({center.x + halfW, center.y - halfH}, cameraPos);
    Vec2 c2 = ToNdc({center.x + halfW, center.y + halfH}, cameraPos);
    Vec2 c3 = ToNdc({center.x - halfW, center.y + halfH}, cameraPos);
    float vertices[8] = {c0.x, c0.y, c1.x, c1.y, c2.x, c2.y, c3.x, c3.y};

    glUniform3f(colorLoc, r, g, b);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void DrawShipTriangle(GLuint vao, GLuint vbo, Vec2 center, float heading, float size, GLint colorLoc,
                       Vec2 cameraPos) {
    Vec2 tip = {center.x + std::cos(heading) * size, center.y + std::sin(heading) * size};
    Vec2 back1 = {center.x + std::cos(heading + 2.6f) * size * 0.6f,
                  center.y + std::sin(heading + 2.6f) * size * 0.6f};
    Vec2 back2 = {center.x + std::cos(heading - 2.6f) * size * 0.6f,
                  center.y + std::sin(heading - 2.6f) * size * 0.6f};
    Vec2 p0 = ToNdc(tip, cameraPos);
    Vec2 p1 = ToNdc(back1, cameraPos);
    Vec2 p2 = ToNdc(back2, cameraPos);
    float vertices[6] = {p0.x, p0.y, p1.x, p1.y, p2.x, p2.y};

    glUniform3f(colorLoc, 0.9f, 0.9f, 0.2f);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

constexpr int kDockRingSegments = 32;

// Marks the actual loading/unloading zone (the same radius Update() checks)
// as a ring on the ground, so it's visible where a ship needs to be, not
// just where the building sprite happens to sit.
void DrawDockRing(GLuint vao, GLuint vbo, Vec2 center, float radius, GLint colorLoc, Vec2 cameraPos) {
    float vertices[kDockRingSegments * 2];
    for (int i = 0; i < kDockRingSegments; ++i) {
        float angle = (static_cast<float>(i) / kDockRingSegments) * 2.0f * kPi;
        Vec2 worldPoint = {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
        Vec2 ndc = ToNdc(worldPoint, cameraPos);
        vertices[i * 2 + 0] = ndc.x;
        vertices[i * 2 + 1] = ndc.y;
    }

    glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_LINE_LOOP, 0, kDockRingSegments);
}

}  // namespace archipelago

int main(int argc, char** argv) {
    using namespace archipelago;
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return EXIT_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Archipelago - Fase 4", kWindowWidth, kWindowHeight, SDL_WINDOW_OPENGL);
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
    glClearColor(0.05f, 0.15f, 0.25f, 1.0f);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplSDL3_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 330");

    GLuint shaderProgram = CreateShaderProgram();
    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");

    GLuint vao = 0;
    GLuint vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * kDockRingSegments * 2, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // Isla 1 (Minera) -> Isla 2 (Industrial) -> Isla 3 (Portuaria). Real
    // distance between them on purpose (thousands of units, not hundreds) —
    // see "la distancia es parte del diseno" en el vault.
    Warehouse island1Warehouse;
    Warehouse island2Warehouse;
    island2Warehouse.Deposit(Resource::Iron, 30);  // small starting stockpile: mill isn't dead on turn one
    IronMine mine(/*rate=*/10);
    mine.SetTotalProduced(30);  // matches the seeded stockpile below, so the balance invariant holds from tick 1
    SteelMill mill(/*consumeRate=*/10, /*outputRatioPercent=*/50);
    Port port;
    Market market(/*basePrice=*/10.0, /*demandPerHour=*/12.0f, /*sensitivity=*/0.05);
    Economy economy(/*startingCash=*/2000.0);
    constexpr double kBaseMaintenanceCost = 3.0;      // mine + mill upkeep, independent of fleet size
    constexpr double kPerShipMaintenanceCost = 5.0;   // each ship (player's included) adds its own upkeep
    constexpr double kShipPurchaseCost = 500.0;
    constexpr int kShipCapacity = 20;

    const Vec2 minePos{300, 300};
    const Vec2 island1Dock{450, 300};
    const Vec2 millPos{2200, 450};
    const Vec2 island2Dock{2350, 450};
    const Vec2 portPos{4300, 150};
    const Vec2 island3Dock{4450, 150};

    std::vector<CargoShip> ships;
    ships.emplace_back(kShipCapacity, island2Dock, RouteKind::SteelRoute);

    Vec2 cameraPos = ships[0].position();

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

        bool f5IsDown = keys[SDL_SCANCODE_F5];
        if (f5IsDown && !f5WasDown) {
            SaveGame(hour, hourAccumulator, mine, mill, island1Warehouse, island2Warehouse, ships, port, market,
                     economy);
            lastSaveLoadMessage = "Guardado (hora " + std::to_string(hour) + ")";
        }
        f5WasDown = f5IsDown;

        bool f9IsDown = keys[SDL_SCANCODE_F9];
        if (f9IsDown && !f9WasDown) {
            if (LoadGame(hour, hourAccumulator, mine, mill, island1Warehouse, island2Warehouse, ships, port, market,
                         economy, kShipCapacity)) {
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
                ships.emplace_back(kShipCapacity, island1Dock, RouteKind::IronRoute);
                economy.ChargeExpense(kShipPurchaseCost);
            } else if (buyAction == 2) {
                ships.emplace_back(kShipCapacity, island2Dock, RouteKind::SteelRoute);
                economy.ChargeExpense(kShipPurchaseCost);
            }

            // Ship 0 is always the player's — you're one person, you can only
            // pilot one hull at a time (see "Encarnacion y capa de mando" en
            // el vault). Fly it to any island; loading/unloading just works
            // based on proximity and what you're carrying. Every other ship
            // in the fleet runs on autopilot, shuttling its assigned pair of
            // docks.
            ships[0].ApplyInput(thrustForward, thrustBackward, turnLeft, turnRight, kFixedDt);
            for (size_t i = 1; i < ships.size(); ++i) {
                ships[i].AutoPilot(kFixedDt, island1Dock, island2Dock, island3Dock);
            }
            for (size_t i = 0; i < ships.size(); ++i) {
                bool isBot = (i != 0);
                ships[i].Update(kFixedDt, island1Warehouse, island2Warehouse, market, economy, port, island1Dock,
                                 island2Dock, island3Dock, isBot);
            }

            hourAccumulator += kFixedDt;
            while (hourAccumulator >= kSecondsPerSimulatedHour) {
                hourAccumulator -= kSecondsPerSimulatedHour;
                ++hour;
                mine.Tick(island1Warehouse, hour);
                mill.Tick(island2Warehouse, hour);
                market.Tick(hour);
                double maintenance =
                    kBaseMaintenanceCost + kPerShipMaintenanceCost * static_cast<double>(ships.size());
                economy.ChargeExpense(maintenance);
                if (!CheckMaterialBalance(mine, mill, island1Warehouse, island2Warehouse, ships, port, hour)) {
                    running = false;
                }
            }
        }

        cameraPos = ships[0].position();

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);

        DrawQuad(vao, vbo, minePos, 60, 40, 0.55f, 0.35f, 0.15f, colorLoc, cameraPos);
        DrawQuad(vao, vbo, millPos, 60, 40, 0.5f, 0.5f, 0.55f, colorLoc, cameraPos);
        DrawQuad(vao, vbo, portPos, 60, 40, 0.2f, 0.4f, 0.8f, colorLoc, cameraPos);
        DrawDockRing(vao, vbo, island1Dock, CargoShip::kDockRadius, colorLoc, cameraPos);
        DrawDockRing(vao, vbo, island2Dock, CargoShip::kDockRadius, colorLoc, cameraPos);
        DrawDockRing(vao, vbo, island3Dock, CargoShip::kDockRadius, colorLoc, cameraPos);
        for (const CargoShip& s : ships) {
            DrawShipTriangle(vao, vbo, s.position(), s.heading(), 24.0f, colorLoc, cameraPos);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImDrawList* labels = ImGui::GetForegroundDrawList();
        const ImU32 labelColor = IM_COL32(255, 255, 255, 255);
        ImVec2 mineLabelPos = WorldToScreen(minePos, cameraPos);
        ImVec2 millLabelPos = WorldToScreen(millPos, cameraPos);
        ImVec2 portLabelPos = WorldToScreen(portPos, cameraPos);
        labels->AddText(ImVec2(mineLabelPos.x - 55, mineLabelPos.y - 60), labelColor, "Isla 1: Mina de Hierro");
        labels->AddText(ImVec2(millLabelPos.x - 45, millLabelPos.y - 60), labelColor, "Isla 2: Aceria");
        labels->AddText(ImVec2(portLabelPos.x - 40, portLabelPos.y - 60), labelColor, "Isla 3: Puerto");

        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
        ImGui::Begin("Estado de la simulacion");
        ImGui::Text("Hora simulada: %d", hour);
        ImGui::Separator();
        ImGui::Text("Isla 1 (Minera) - Hierro: %d", island1Warehouse.Get(Resource::Iron));
        ImGui::Text("Isla 2 (Industrial) - Hierro: %d, Acero: %d", island2Warehouse.Get(Resource::Iron),
                    island2Warehouse.Get(Resource::Steel));
        ImGui::Text("Aceria: %s", mill.isIdle() ? "PARADA (sin Hierro)" : "activa");
        ImGui::Text("Isla 3 (Puerto) - exportado total: %d", port.totalExported());
        ImGui::Separator();
        ImGui::Text("Tu barco - carga: %d %s", ships[0].cargo(), ToString(ships[0].cargoResource()).c_str());
        ImGui::Separator();
        ImGui::Text("Hierro producido total: %d", mine.totalProduced());
        ImGui::Text("Acero producido total:  %d", mill.totalProduced());
        ImGui::Separator();
        ImGui::Text("Caja: $%.2f", economy.cash());
        ImGui::Text("Precio Acero: $%.2f/unidad", market.CurrentPrice());
        ImGui::Text("Stock de mercado: %.1f", market.stock());
        ImGui::Text("Ingresos totales: $%.2f", market.totalRevenue());
        double currentMaintenance = kBaseMaintenanceCost + kPerShipMaintenanceCost * static_cast<double>(ships.size());
        ImGui::Text("Gastos totales: $%.2f (mantenimiento $%.0f/hora con %zu barco%s)", economy.totalExpenses(),
                    currentMaintenance, ships.size(), ships.size() == 1 ? "" : "s");
        ImGui::Separator();
        ImGui::Text("Flota: %zu barco%s (autopilotados todos salvo el tuyo)", ships.size(),
                    ships.size() == 1 ? "" : "s");
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
    std::cout << "Total revenue: $" << market.totalRevenue() << "\n";
    std::cout << "Total expenses: $" << economy.totalExpenses() << "\n";
    std::cout << "Final cash: $" << economy.cash() << "\n";

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shaderProgram);
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}
