#pragma once

// --- Fase 8.0 (Terreno procedural), paso 1: ruido 2D determinista con
// semilla. Base de todo lo que viene después (fondo marino con
// profundidad real, forma de las islas) — nada más depende de esto todavía,
// pero todo lo que sigue depende de esto. Value noise (interpolación suave
// entre valores de una rejilla hasheados por semilla) combinado en varias
// octavas (fBm) para que el terreno tenga forma a gran escala (masas de
// tierra) y detalle fino (costa irregular) a la vez, no un único blob
// uniforme. Mismo espíritu que weather.h: determinista por semilla, no
// RNG en tiempo real — necesario para que un mundo generado sea
// reproducible (Guardado/Carga, y eventualmente Replay). ---

#include <cmath>
#include <cstdint>

namespace archipelago {
namespace Noise {

// Cheap, well-distributed integer hash over (ix, iz, seed) — same
// avalanche-hash family as Weather::HashU32, just mixing three inputs
// instead of one.
inline uint32_t HashGrid(int ix, int iz, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(ix) * 374761393u + static_cast<uint32_t>(iz) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return h;
}

// Pseudo-random value in [0,1) for a grid corner.
inline float HashToUnit(int ix, int iz, uint32_t seed) {
    return static_cast<float>(HashGrid(ix, iz, seed)) * (1.0f / 4294967295.0f);
}

inline float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

// Single-octave value noise at world position (x,z): bilinear (smoothed)
// interpolation between the 4 hashed corners of the grid cell containing
// (x,z), at the given cellSize (world units per grid cell — bigger
// cellSize means broader, slower-varying features). Returns a value in
// [-1,1].
inline float ValueNoise2D(float x, float z, float cellSize, uint32_t seed) {
    float gx = x / cellSize;
    float gz = z / cellSize;
    int ix0 = static_cast<int>(std::floor(gx));
    int iz0 = static_cast<int>(std::floor(gz));
    float fx = gx - static_cast<float>(ix0);
    float fz = gz - static_cast<float>(iz0);

    float v00 = HashToUnit(ix0, iz0, seed);
    float v10 = HashToUnit(ix0 + 1, iz0, seed);
    float v01 = HashToUnit(ix0, iz0 + 1, seed);
    float v11 = HashToUnit(ix0 + 1, iz0 + 1, seed);

    float sx = SmoothStep(fx);
    float sz = SmoothStep(fz);

    float top = v00 + (v10 - v00) * sx;
    float bottom = v01 + (v11 - v01) * sx;
    float value = top + (bottom - top) * sz;  // in [0,1]
    return value * 2.0f - 1.0f;
}

// Fractal Brownian motion: sums several octaves of ValueNoise2D, each at
// double the frequency (half the cellSize) and half the amplitude of the
// last, then normalizes back to roughly [-1,1]. This is what actually
// makes it look like terrain instead of a single smooth bump: broad
// landmass shape from the first octave, progressively finer wrinkles
// (coastline irregularity) from the later ones.
inline float FractalNoise2D(float x, float z, float baseCellSize, int octaves, uint32_t seed) {
    float total = 0.0f;
    float amplitude = 1.0f;
    float amplitudeSum = 0.0f;
    float cellSize = baseCellSize;
    for (int i = 0; i < octaves; ++i) {
        total += ValueNoise2D(x, z, cellSize, seed + static_cast<uint32_t>(i) * 101u) * amplitude;
        amplitudeSum += amplitude;
        amplitude *= 0.5f;
        cellSize *= 0.5f;
    }
    return total / amplitudeSum;
}

}  // namespace Noise
}  // namespace archipelago
