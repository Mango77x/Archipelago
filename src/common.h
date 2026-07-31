#pragma once

// Shared types/constants/helpers with no meaningful logic of their own —
// every other module includes this. Kept header-only (functions marked
// inline) since it's included from multiple .cpp files.

#include <cmath>
#include <string>

#include <glm/glm.hpp>

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

inline std::string ToString(Resource r) {
    switch (r) {
        case Resource::Iron: return "Iron";
        case Resource::Steel: return "Steel";
    }
    return "Unknown";
}

// Which pair of islands an autopiloted ship shuttles between: Isla1(mina) <->
// Isla2(aceria), or Isla2(aceria) <-> Isla3(puerto). Only steers autopilot —
// it never restricts what a ship can load/unload if you fly it somewhere
// else yourself (see CargoShip::HandleDocking).
enum class RouteKind { IronRoute, SteelRoute };

// World positions live in 3D now (Fase 4.5): X/Z is the horizontal plane
// ships and islands sit on (matching the old 2D X/Y), Y is height/altitude.
// Rendering is the only thing that actually needed 3D; the economy/logistics
// logic underneath doesn't care (principle 2: los graficos visualizan la
// simulacion, no la sustituyen).
using Vec3 = glm::vec3;

// Wraps an angle (radians) to (-pi, pi]. Used to find the shortest turn
// direction toward a target heading.
inline float NormalizeAngle(float angle) {
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

inline float HorizontalDistance(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// Shared footprint for the water mesh, the seafloor beneath it, and (Fase
// 7.1 storms) where storm cells are allowed to spawn — same area, generous
// margin around all three islands (see "la distancia es parte del diseno"
// en el vault).
constexpr float kSeaCenterX = 2500.0f;
constexpr float kSeaCenterZ = 300.0f;
// Fase 8.0 (Terreno procedural): bumped 4x per axis (10000->40000, so the
// full sea span goes from 20000x20000 to 80000x80000) once the map/pan/zoom
// made a bigger world actually navigable — the existing 3 fixed islands sit
// right around this center already, so they stay put near the middle of the
// bigger world instead of needing to move.
constexpr float kSeaHalfExtentX = 40000.0f;
constexpr float kSeaHalfExtentZ = 40000.0f;

}  // namespace archipelago
