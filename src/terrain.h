#pragma once

// --- Fase 8.0 (Terreno procedural): altura del terreno a partir del ruido
// con semilla (noise.h). Paso 2 lo usó solo para el fondo marino (siempre
// por debajo del nivel del mar); paso 3 quita ese límite: donde el ruido
// supera kSeaLevelNoiseThreshold, el mismo campo de altura se convierte en
// tierra por encima del nivel del mar — las islas emergen del mismo terreno
// que el fondo marino, no son un sistema aparte. La semilla decide cuántas
// islas salen, dónde, y su forma de costa. ---

#include <cstdint>

#include "common.h"
#include "noise.h"

namespace archipelago {
namespace Terrain {

// Bumped 3000->15000 alongside the world going from 20000x20000 to
// 80000x80000 (see kSeaHalfExtentX/Z in common.h): a fixed noise cell size
// on a 4x bigger world just gives 4x more islands of the same small size —
// "thousands of tiny useless islands," not a playable archipelago. 15000
// was picked by counting connected land components across seeds (Python
// replica of this exact formula): gives roughly a dozen islands, several of
// them large enough (7000-20000 world units across) to actually build on,
// drive vehicles across, and fight over — not just decoration.
constexpr float kBaseCellSize = 15000.0f;
constexpr int kOctaves = 4;

// Calibrated empirically (Python replica of this exact formula, ASCII-map
// sanity check across several seeds): 0.4 gives an archipelago spread of
// small-to-medium islands with plenty of open sea between them, not a
// continent (0.0 would put ~50% of the map above water) and not a
// near-empty ocean (0.7 leaves barely a handful of tiny specks).
constexpr float kSeaLevelNoiseThreshold = 0.4f;
constexpr float kMinDepth = -400.0f;     // deepest sea floor
constexpr float kMaxLandHeight = 150.0f;  // tallest island peaks

// World-space Y height of the terrain at (x,z) for the given seed — negative
// (underwater) where the noise is below kSeaLevelNoiseThreshold, positive
// (land) above it. Continuous across the threshold (both branches give
// exactly 0 right at the boundary, so there's no seam/cliff at the
// shoreline). Deterministic — same (x,z,seed) always gives the same height,
// needed for Guardado/Carga (and eventually Replay) to reproduce the same
// world.
inline float Height(float x, float z, uint32_t seed) {
    float n = Noise::FractalNoise2D(x, z, kBaseCellSize, kOctaves, seed);  // [-1,1]
    if (n <= kSeaLevelNoiseThreshold) {
        float t = (n + 1.0f) / (kSeaLevelNoiseThreshold + 1.0f);  // [0,1]
        return kMinDepth + t * (0.0f - kMinDepth);
    }
    float t = (n - kSeaLevelNoiseThreshold) / (1.0f - kSeaLevelNoiseThreshold);  // (0,1]
    return t * kMaxLandHeight;
}

// A world-space (x,z) shift such that sampling Height(x - offsetX, z -
// offsetZ, seed) puts the biggest connected landmass right at
// (seaCenterX, seaCenterZ) — the user wants the biggest island always
// centered (easy to find, good starting spot) regardless of what the
// noise/seed happen to produce, not scattered wherever it lands naturally.
struct CenterOffset {
    float x = 0.0f;
    float z = 0.0f;
};

// A building on land plus its dock in the adjacent water — same shape the
// old fixed mine/mill/port + island1/2/3Dock pairs had, just computed from
// the generated terrain instead of hand-picked coordinates.
struct BuildingSite {
    Vec3 buildingPos;
    Vec3 dockPos;
};

// Fase 8.0, paso 4: mina, acería y puerto no pueden seguir en coordenadas
// fijas ahora que hay tierra generada de verdad ahí — un barco navegando a
// esas coordenadas encontraría tierra sólida en vez de un muelle. Esto
// encuentra la isla más grande (vía flood fill sobre una rejilla de
// muestreo — no por fotograma, una sola vez al arrancar) y planta los 3
// edificios en su costa, bien repartidos entre sí (muestreo del punto más
// lejano: el segundo sitio es el más lejano del primero, el tercero
// maximiza la distancia mínima a los otros dos), cada uno con su muelle en
// la celda de mar más cercana. También devuelve el CenterOffset para que
// el muestreo de terreno (render + colisión) y la colocación de edificios
// usen exactamente la misma isla.
struct WorldLayout {
    CenterOffset offset;
    BuildingSite mine;
    BuildingSite mill;
    BuildingSite port;
};

WorldLayout ComputeWorldLayout(uint32_t seed, float seaCenterX, float seaCenterZ, float seaHalfExtentX,
                                float seaHalfExtentZ);

}  // namespace Terrain
}  // namespace archipelago
