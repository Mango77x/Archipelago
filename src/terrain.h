#pragma once

// --- Fase 8.0 (Terreno procedural), paso 2: altura del fondo marino a
// partir del ruido con semilla (noise.h). Por ahora esto se mantiene
// siempre por debajo del nivel del mar (clamp a kMaxHeight, todavía
// negativo) a propósito: el paso 3 quitará ese límite para que la tierra
// emerja donde el ruido supere el nivel del mar, convirtiendo esto mismo en
// la base de las islas también, no solo del fondo. ---

#include <algorithm>
#include <cstdint>

#include "noise.h"

namespace archipelago {
namespace Terrain {

constexpr float kMinHeight = -400.0f;  // deepest sea floor
// Still comfortably underwater — paso 3 removes this ceiling so land can
// poke through instead of capping here.
constexpr float kMaxHeight = -30.0f;
constexpr float kBaseCellSize = 3000.0f;
constexpr int kOctaves = 4;

// World-space Y height of the sea floor at (x,z) for the given seed.
// Deterministic — same (x,z,seed) always gives the same height, needed for
// Guardado/Carga (and eventually Replay) to reproduce the same world.
inline float SeaFloorHeight(float x, float z, uint32_t seed) {
    float n = Noise::FractalNoise2D(x, z, kBaseCellSize, kOctaves, seed);  // [-1,1]
    float t = (n + 1.0f) * 0.5f;                                          // [0,1]
    float height = kMinHeight + t * (kMaxHeight - kMinHeight);
    return std::clamp(height, kMinHeight, kMaxHeight);
}

}  // namespace Terrain
}  // namespace archipelago
