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

namespace archipelago {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr double kSecondsPerSimulatedHour = 2.0;

enum class Resource { Iron, Steel };

std::string ToString(Resource r) {
    switch (r) {
        case Resource::Iron: return "Iron";
        case Resource::Steel: return "Steel";
    }
    return "Unknown";
}

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
// the fixed hourly upkeep of mine + mill + ship, charged whether or not the
// ship is moving — so profit only grows by moving more Steel, faster, and
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

// Player-piloted cargo ship: simple 2D kinematic model (thrust + drag + angular
// inertia), not a physics engine — enough to feel like a boat without pulling
// in Jolt before the gameplay actually needs collisions (see CLAUDE.md).
class CargoShip {
public:
    CargoShip(int capacity, Vec2 startPos) : capacity_(capacity), position_(startPos) {}

    void HandleInput(const bool* keys, float dt) {
        constexpr float kThrust = 220.0f;
        constexpr float kTurnRate = 2.5f;

        if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
            velocity_.x += std::cos(heading_) * kThrust * dt;
            velocity_.y += std::sin(heading_) * kThrust * dt;
        }
        if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
            velocity_.x -= std::cos(heading_) * kThrust * dt;
            velocity_.y -= std::sin(heading_) * kThrust * dt;
        }
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) angularVelocity_ -= kTurnRate * dt;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) angularVelocity_ += kTurnRate * dt;
    }

    void Update(float dt, Warehouse& warehouse, Port& port, Market& market, Economy& economy, Vec2 millDock,
                Vec2 portDock) {
        constexpr float kLinearDrag = 0.6f;
        constexpr float kAngularDrag = 0.85f;
        constexpr float kMaxSpeed = 160.0f;
        constexpr float kDockRadius = 40.0f;

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

        if (Distance(position_, millDock) <= kDockRadius && cargo_ < capacity_) {
            int available = warehouse.Get(Resource::Steel);
            int amount = std::min(available, capacity_ - cargo_);
            if (amount > 0) {
                warehouse.Withdraw(Resource::Steel, amount);
                cargo_ += amount;
                std::cout << "Cargo Ship loaded " << amount << " Steel\n";
            }
        }
        if (Distance(position_, portDock) <= kDockRadius && cargo_ > 0) {
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

    void SetState(Vec2 position, Vec2 velocity, float heading, float angularVelocity, int cargo) {
        position_ = position;
        velocity_ = velocity;
        heading_ = heading;
        angularVelocity_ = angularVelocity;
        cargo_ = cargo;
    }

private:
    int capacity_;
    Vec2 position_;
    Vec2 velocity_;
    float heading_ = 0.0f;
    float angularVelocity_ = 0.0f;
    int cargo_ = 0;
};

// Invariant that must hold every simulated hour: every unit of Steel ever
// produced is accounted for either in the warehouse, riding the ship, or
// already exported. Phase 0's one failure mode to catch, still true in Phase 1.
bool CheckMaterialBalance(const IronMine& mine, const SteelMill& mill, const Warehouse& warehouse,
                           const CargoShip& ship, const Port& port, int hour) {
    int ironBalance = mine.totalProduced() - mill.totalConsumed() - warehouse.Get(Resource::Iron);
    int steelBalance = mill.totalProduced() - warehouse.Get(Resource::Steel) - ship.cargo() - port.totalExported();

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
constexpr const char* kSaveHeader = "ARCHIPELAGO_SAVE_V1";

void SaveGame(int hour, double hourAccumulator, const IronMine& mine, const SteelMill& mill,
              const Warehouse& warehouse, const CargoShip& ship, const Port& port, const Market& market,
              const Economy& economy) {
    std::ofstream out(kSaveFile);
    if (!out) {
        std::cerr << "SaveGame: could not open " << kSaveFile << " for writing\n";
        return;
    }
    Vec2 pos = ship.position();
    Vec2 vel = ship.velocity();
    out << kSaveHeader << "\n";
    out << hour << " " << hourAccumulator << "\n";
    out << mine.totalProduced() << "\n";
    out << mill.totalConsumed() << " " << mill.totalProduced() << " " << (mill.isIdle() ? 1 : 0) << "\n";
    out << warehouse.Get(Resource::Iron) << " " << warehouse.Get(Resource::Steel) << "\n";
    out << port.totalExported() << "\n";
    out << pos.x << " " << pos.y << " " << vel.x << " " << vel.y << " " << ship.heading() << " "
        << ship.angularVelocity() << " " << ship.cargo() << "\n";
    out << market.stock() << " " << market.totalRevenue() << " " << market.totalSold() << "\n";
    out << economy.cash() << " " << economy.totalExpenses() << "\n";
    std::cout << "Game saved to " << kSaveFile << " (hour " << hour << ")\n";
}

bool LoadGame(int& hour, double& hourAccumulator, IronMine& mine, SteelMill& mill, Warehouse& warehouse,
              CargoShip& ship, Port& port, Market& market, Economy& economy) {
    std::ifstream in(kSaveFile);
    if (!in) {
        std::cerr << "LoadGame: could not open " << kSaveFile << "\n";
        return false;
    }
    std::string header;
    std::getline(in, header);
    if (header != kSaveHeader) {
        std::cerr << "LoadGame: unrecognized save file header\n";
        return false;
    }

    int mineProduced = 0;
    int millConsumed = 0, millProduced = 0, millIdleFlag = 0;
    int ironStock = 0, steelStock = 0;
    int exported = 0;
    Vec2 pos{}, vel{};
    float heading = 0.0f, angularVelocity = 0.0f;
    int cargo = 0;
    float marketStock = 0.0f;
    double marketRevenue = 0.0;
    int marketSold = 0;
    double cash = 0.0, totalExpenses = 0.0;

    in >> hour >> hourAccumulator;
    in >> mineProduced;
    in >> millConsumed >> millProduced >> millIdleFlag;
    in >> ironStock >> steelStock;
    in >> exported;
    in >> pos.x >> pos.y >> vel.x >> vel.y >> heading >> angularVelocity >> cargo;
    in >> marketStock >> marketRevenue >> marketSold;
    in >> cash >> totalExpenses;

    if (!in) {
        std::cerr << "LoadGame: save file is truncated or malformed\n";
        return false;
    }

    mine.SetTotalProduced(mineProduced);
    mill.SetTotals(millConsumed, millProduced, millIdleFlag != 0);
    warehouse.SetStock(Resource::Iron, ironStock);
    warehouse.SetStock(Resource::Steel, steelStock);
    port.SetTotalExported(exported);
    ship.SetState(pos, vel, heading, angularVelocity, cargo);
    market.SetState(marketStock, marketRevenue, marketSold);
    economy.SetState(cash, totalExpenses);

    std::cout << "Game loaded from " << kSaveFile << " (hour " << hour << ")\n";
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

Vec2 ToNdc(Vec2 worldPos) {
    return {(worldPos.x / kWindowWidth) * 2.0f - 1.0f, 1.0f - (worldPos.y / kWindowHeight) * 2.0f};
}

void DrawQuad(GLuint vao, GLuint vbo, Vec2 center, float halfW, float halfH, float r, float g, float b,
              GLint colorLoc) {
    Vec2 c0 = ToNdc({center.x - halfW, center.y - halfH});
    Vec2 c1 = ToNdc({center.x + halfW, center.y - halfH});
    Vec2 c2 = ToNdc({center.x + halfW, center.y + halfH});
    Vec2 c3 = ToNdc({center.x - halfW, center.y + halfH});
    float vertices[8] = {c0.x, c0.y, c1.x, c1.y, c2.x, c2.y, c3.x, c3.y};

    glUniform3f(colorLoc, r, g, b);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void DrawShipTriangle(GLuint vao, GLuint vbo, Vec2 center, float heading, float size, GLint colorLoc) {
    Vec2 tip = {center.x + std::cos(heading) * size, center.y + std::sin(heading) * size};
    Vec2 back1 = {center.x + std::cos(heading + 2.6f) * size * 0.6f,
                  center.y + std::sin(heading + 2.6f) * size * 0.6f};
    Vec2 back2 = {center.x + std::cos(heading - 2.6f) * size * 0.6f,
                  center.y + std::sin(heading - 2.6f) * size * 0.6f};
    Vec2 p0 = ToNdc(tip);
    Vec2 p1 = ToNdc(back1);
    Vec2 p2 = ToNdc(back2);
    float vertices[6] = {p0.x, p0.y, p1.x, p1.y, p2.x, p2.y};

    glUniform3f(colorLoc, 0.9f, 0.9f, 0.2f);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 3);
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

    SDL_Window* window = SDL_CreateWindow("Archipelago - Fase 2", kWindowWidth, kWindowHeight, SDL_WINDOW_OPENGL);
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    Warehouse warehouse;
    IronMine mine(/*rate=*/10);
    SteelMill mill(/*consumeRate=*/10, /*outputRatioPercent=*/50);
    Port port;
    Market market(/*basePrice=*/10.0, /*demandPerHour=*/12.0f, /*sensitivity=*/0.05);
    Economy economy(/*startingCash=*/1000.0);
    constexpr double kHourlyMaintenanceCost = 15.0;

    const Vec2 minePos{160, 360};
    const Vec2 warehousePos{340, 360};
    const Vec2 millPos{520, 360};
    const Vec2 millDock{620, 360};
    const Vec2 portPos{1100, 360};
    const Vec2 portDock{1000, 360};

    CargoShip ship(/*capacity=*/20, /*startPos=*/millDock);

    Uint64 lastCounter = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    double hourAccumulator = 0.0;
    int hour = 0;
    bool running = true;
    bool f5WasDown = false;
    bool f9WasDown = false;
    std::string lastSaveLoadMessage;

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
            SaveGame(hour, hourAccumulator, mine, mill, warehouse, ship, port, market, economy);
            lastSaveLoadMessage = "Guardado (hora " + std::to_string(hour) + ")";
        }
        f5WasDown = f5IsDown;

        bool f9IsDown = keys[SDL_SCANCODE_F9];
        if (f9IsDown && !f9WasDown) {
            if (LoadGame(hour, hourAccumulator, mine, mill, warehouse, ship, port, market, economy)) {
                lastSaveLoadMessage = "Cargado (hora " + std::to_string(hour) + ")";
            } else {
                lastSaveLoadMessage = "Error al cargar (ver consola)";
            }
        }
        f9WasDown = f9IsDown;

        ship.HandleInput(keys, static_cast<float>(dt));
        ship.Update(static_cast<float>(dt), warehouse, port, market, economy, millDock, portDock);

        hourAccumulator += dt;
        while (hourAccumulator >= kSecondsPerSimulatedHour) {
            hourAccumulator -= kSecondsPerSimulatedHour;
            ++hour;
            mine.Tick(warehouse, hour);
            mill.Tick(warehouse, hour);
            market.Tick(hour);
            economy.ChargeExpense(kHourlyMaintenanceCost);
            if (!CheckMaterialBalance(mine, mill, warehouse, ship, port, hour)) {
                running = false;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);

        DrawQuad(vao, vbo, minePos, 60, 40, 0.55f, 0.35f, 0.15f, colorLoc);
        DrawQuad(vao, vbo, warehousePos, 50, 40, 0.85f, 0.75f, 0.2f, colorLoc);
        DrawQuad(vao, vbo, millPos, 60, 40, 0.5f, 0.5f, 0.55f, colorLoc);
        DrawQuad(vao, vbo, portPos, 60, 40, 0.2f, 0.4f, 0.8f, colorLoc);
        DrawShipTriangle(vao, vbo, ship.position(), ship.heading(), 24.0f, colorLoc);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImDrawList* labels = ImGui::GetForegroundDrawList();
        const ImU32 labelColor = IM_COL32(255, 255, 255, 255);
        labels->AddText(ImVec2(minePos.x - 55, minePos.y - 60), labelColor, "Mina de Hierro");
        labels->AddText(ImVec2(warehousePos.x - 40, warehousePos.y - 60), labelColor, "Almacen");
        labels->AddText(ImVec2(millPos.x - 30, millPos.y - 60), labelColor, "Aceria");
        labels->AddText(ImVec2(portPos.x - 25, portPos.y - 60), labelColor, "Puerto");

        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
        ImGui::Begin("Estado de la simulacion");
        ImGui::Text("Hora simulada: %d", hour);
        ImGui::Separator();
        ImGui::Text("Almacen - Hierro: %d", warehouse.Get(Resource::Iron));
        ImGui::Text("Almacen - Acero:  %d", warehouse.Get(Resource::Steel));
        ImGui::Text("Aceria: %s", mill.isIdle() ? "PARADA (sin Hierro)" : "activa");
        ImGui::Separator();
        ImGui::Text("Barco - carga: %d", ship.cargo());
        ImGui::Text("Puerto - exportado total: %d", port.totalExported());
        ImGui::Separator();
        ImGui::Text("Hierro producido total: %d", mine.totalProduced());
        ImGui::Text("Acero producido total:  %d", mill.totalProduced());
        ImGui::Separator();
        ImGui::Text("Caja: $%.2f", economy.cash());
        ImGui::Text("Precio Acero: $%.2f/unidad", market.CurrentPrice());
        ImGui::Text("Stock de mercado: %.1f", market.stock());
        ImGui::Text("Ingresos totales: $%.2f", market.totalRevenue());
        ImGui::Text("Gastos totales: $%.2f (mantenimiento $%.0f/hora)", economy.totalExpenses(),
                    kHourlyMaintenanceCost);
        ImGui::Separator();
        ImGui::Text("F5: guardar   F9: cargar");
        if (!lastSaveLoadMessage.empty()) {
            ImGui::Text("%s", lastSaveLoadMessage.c_str());
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
