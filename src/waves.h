#pragma once

// --- Fase 7.1: Gerstner wave field. Height() is the single source of truth
// for "how tall is the water at (x,z,t)" — used both for buoyancy sampling
// here in C++ and (mirrored by hand, same constants) in the water mesh's
// vertex shader (see render.cpp's kWaterVertexSrc), so what the hull feels
// matches what the surface visually shows. Three overlapping components
// (different direction/wavelength/amplitude/speed) give an irregular,
// non-repeating-looking chop instead of one uniform swell — amplitudes/
// speeds are tuned by feel, not derived from a real deep-water dispersion
// relation. ---

#include "common.h"

namespace archipelago {
namespace Waves {

struct Component {
    float direction;   // radians, direction the wave travels in the XZ plane
    float wavelength;   // world units, crest to crest
    float amplitude;    // world units, half of crest-to-trough height
    float speed;        // world units/second the crest travels at
};

constexpr Component kComponents[3] = {
    {0.3f, 220.0f, 3.0f, 9.0f},
    {1.1f, 130.0f, 1.8f, 6.5f},
    {2.4f, 340.0f, 2.4f, 11.0f},
};

// Water surface height (world Y) at horizontal position (x,z) at time t.
// Sum of sines, not full peaked-Gerstner displacement — good enough for
// buoyancy sampling (the visual mesh adds the horizontal peak displacement
// on top, see the vertex shader, without changing this height formula).
inline float Height(float x, float z, float t) {
    float h = 0.0f;
    for (const Component& c : kComponents) {
        float k = 2.0f * kPi / c.wavelength;
        float dirX = std::cos(c.direction);
        float dirZ = std::sin(c.direction);
        float phase = k * (dirX * x + dirZ * z) - c.speed * k * t;
        h += c.amplitude * std::sin(phase);
    }
    return h;
}

}  // namespace Waves
}  // namespace archipelago
