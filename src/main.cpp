#include <GL/glew.h>
#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "common.h"
#include "economy.h"
#include "jolt_world.h"
#include "render.h"
#include "replay.h"
#include "save_load.h"
#include "ship.h"
#include "simulation.h"

using namespace archipelago;

// Shared footprint for the water mesh and the seafloor beneath it — same
// area, generous margin around all three islands (see "la distancia es
// parte del diseno" en el vault).
constexpr float kSeaCenterX = 2500.0f;
constexpr float kSeaCenterZ = 300.0f;
constexpr float kSeaHalfExtentX = 10000.0f;
constexpr float kSeaHalfExtentZ = 10000.0f;
constexpr float kSeaFloorDepth = -250.0f;

int main(int argc, char** argv) {
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
    CreateSeaFloorBody(bodyInterface, kSeaCenterX, kSeaCenterZ, kSeaHalfExtentX, kSeaHalfExtentZ, kSeaFloorDepth);

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
    CreateCubeMesh(cubeVao, cubeVbo);

    constexpr int kDockRingSegments = 32;
    GLuint ringVao = 0, ringVbo = 0;
    glGenVertexArrays(1, &ringVao);
    glGenBuffers(1, &ringVbo);
    glBindVertexArray(ringVao);
    glBindBuffer(GL_ARRAY_BUFFER, ringVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * kDockRingSegments * 3, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // Fase 7.1: water mesh — same footprint the old flat water box covered,
    // now subdivided so the vertex shader has enough vertices to actually
    // show the wave shape.
    GLuint waterShaderProgram = CreateWaterShaderProgram();
    GLint waterColorLoc = glGetUniformLocation(waterShaderProgram, "uColor");
    GLint waterViewProjLoc = glGetUniformLocation(waterShaderProgram, "uViewProj");
    GLint waterTimeLoc = glGetUniformLocation(waterShaderProgram, "uTime");
    GLint waterLightDirLoc = glGetUniformLocation(waterShaderProgram, "uLightDir");

    constexpr int kWaterSegments = 200;
    std::vector<float> waterVertices;
    std::vector<GLuint> waterIndices;
    GenerateWaterGrid(kSeaCenterX - kSeaHalfExtentX, kSeaCenterX + kSeaHalfExtentX, kSeaCenterZ - kSeaHalfExtentZ,
                       kSeaCenterZ + kSeaHalfExtentZ, kWaterSegments, waterVertices, waterIndices);
    GLsizei waterIndexCount = static_cast<GLsizei>(waterIndices.size());

    GLuint waterVao = 0, waterVbo = 0, waterEbo = 0;
    glGenVertexArrays(1, &waterVao);
    glGenBuffers(1, &waterVbo);
    glGenBuffers(1, &waterEbo);
    glBindVertexArray(waterVao);
    glBindBuffer(GL_ARRAY_BUFFER, waterVbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(waterVertices.size() * sizeof(float)), waterVertices.data(),
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(waterIndices.size() * sizeof(GLuint)),
                 waterIndices.data(), GL_STATIC_DRAW);
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
    // Advances by kFixedDt every fixed step (not real elapsed time) so wave
    // motion stays tied to the deterministic simulation clock — same
    // reasoning as the Replay system: a real-time clock would make the sea
    // depend on how fast the machine happened to render.
    float waveTime = 0.0f;
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
            // Fase 7.1: buoyancy is a force too, so it's applied in this same
            // pre-physics-step pass, alongside thrust/steering.
            for (CargoShip& s : playerShips) {
                s.ApplyBuoyancy(waveTime);
            }
            for (CargoShip& s : aiShips) {
                s.ApplyBuoyancy(waveTime);
            }

            // One physics step for every body in the world, then per-ship
            // game logic reads back the settled position/velocity — see
            // "Fase 7.0" comment on ClampSpeed()/HandleDocking().
            physicsSystem.Update(static_cast<float>(kFixedDt), 1, &physicsTempAllocator, &physicsJobSystem);
            waveTime += kFixedDt;

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

        // Fase 7.1: water is its own wave-displaced grid + shader now (see
        // GenerateWaterGrid/CreateWaterShaderProgram) — drawn before switching
        // to the lit (rigid-box) shader used for everything else.
        glUseProgram(waterShaderProgram);
        glUniform3f(waterLightDirLoc, 0.4f, -1.0f, 0.3f);
        DrawWater(waterVao, waterIndexCount, waterViewProjLoc, waterTimeLoc, waterColorLoc, viewProj, waveTime);

        glUseProgram(litShaderProgram);
        glUniform3f(lightDirLoc, 0.4f, -1.0f, 0.3f);

        // Fase 7.1: flat seafloor, same footprint as the water (see
        // CreateSeaFloorBody) — just a visual/collision baseline for now, no
        // terrain shape until procedural world generation exists.
        DrawBox(cubeVao, litMvpLoc, litNormalMatrixLoc, litColorLoc, viewProj,
                Vec3{kSeaCenterX, kSeaFloorDepth, kSeaCenterZ},
                Vec3{kSeaHalfExtentX * 2.0f, 20.0f, kSeaHalfExtentZ * 2.0f}, 0.0f, 0.3f, 0.27f, 0.2f);

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
                    Vec3{48, 16, 24}, s.rotation(), 0.9f, 0.9f, 0.2f);
        }
        for (const CargoShip& s : aiShips) {
            // Rival AI hulls in a distinct reddish color so they read as "not yours" at a glance.
            DrawBox(cubeVao, litMvpLoc, litNormalMatrixLoc, litColorLoc, viewProj, s.position() + Vec3{0, 8, 0},
                    Vec3{48, 16, 24}, s.rotation(), 0.85f, 0.25f, 0.2f);
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
    glDeleteBuffers(1, &waterVbo);
    glDeleteBuffers(1, &waterEbo);
    glDeleteVertexArrays(1, &waterVao);
    glDeleteProgram(litShaderProgram);
    glDeleteProgram(unlitShaderProgram);
    glDeleteProgram(waterShaderProgram);
    SDL_GL_DestroyContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    return EXIT_SUCCESS;
}
