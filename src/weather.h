#pragma once

// --- Fase 7 (Entorno): storms as moving wind-intensity cells. Not a global
// "calm/storm" switch — WindIntensity(x,z,t) is a continuous field: a small,
// fixed number of storm cells that spawn, drift in a roughly straight line,
// build up and dissipate over their lifetime, then vanish for good (a new,
// different one eventually takes their generator slot — storms don't loop
// back to the same spot). At any point there might be one nearby, two
// overlapping, or none at all — real gaps of calm, not just "away from the
// nearest cell."
//
// Fully deterministic: every cell's spawn position/direction/size/strength/
// lifespan is derived from a hash of (slot, cycle index), not runtime RNG —
// same reasoning as everywhere else in this codebase (Replay needs bit-for-
// bit reproducibility). The same math is mirrored by hand in the water
// vertex shader (see render.cpp's kWaterVertexSrc) so a storm looks like a
// storm exactly where a ship feels one.

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "common.h"

namespace archipelago {
namespace Weather {

// IMPORTANT: lifespan must stay comfortably shorter than the cycle, or a
// storm gets replaced by a brand new (different position/intensity) one
// mid-buildup instead of finishing its own fade-out — a too-short cycle
// relative to lifespan is what caused an "80% to 20% instantly" jump during
// tuning (test values had a 60s cycle with the 400-600s real lifespan still
// in place).
constexpr int kStormSlots = 2;
// Each slot gets a fresh (possibly absent, see kSkipChance) storm every
// kCycleDuration seconds of sim time — long enough, relative to
// kMaxLifespan, to leave real gaps of calm between one storm and the next
// from the same slot.
constexpr float kCycleDuration = 1800.0f;
constexpr float kMinLifespan = 400.0f;
constexpr float kMaxLifespan = 600.0f;
constexpr float kMinRadius = 800.0f;
constexpr float kMaxRadius = 1500.0f;
constexpr float kMinPeakIntensity = 0.7f;
constexpr float kMaxPeakIntensity = 1.0f;
constexpr float kMinDriftSpeed = 2.0f;
constexpr float kMaxDriftSpeed = 5.0f;
// Fraction of cycles where a slot simply doesn't spawn a storm at all — the
// "sometimes there's just nothing happening anywhere" case the user asked
// for, not merely "outside every cell's radius."
constexpr float kSkipChance = 0.5f;

// Cheap, well-distributed integer hash (Wang/xorshift-multiply style) — not
// cryptographic, just needs to avalanche well enough that nearby seeds don't
// produce visibly-correlated storms. Mirrored in GLSL with the same
// constants.
inline uint32_t HashU32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

inline float HashToUnit(uint32_t seed) { return static_cast<float>(HashU32(seed)) * (1.0f / 4294967295.0f); }

struct StormCell {
    bool active = false;
    float spawnX = 0.0f, spawnZ = 0.0f;
    float dirX = 0.0f, dirZ = 0.0f;
    float speed = 0.0f;
    float radius = 0.0f;
    float peakIntensity = 0.0f;
    float lifespan = 0.0f;
};

// Deterministically derives a storm cell's fixed parameters for a given
// (slot, cycleIndex) — same inputs always produce the same storm, so two
// runs of the same replay see identical weather.
inline StormCell DeriveStormCell(int slot, long long cycleIndex) {
    uint32_t seed = static_cast<uint32_t>(slot) * 2654435761u + static_cast<uint32_t>(cycleIndex) * 40503u;
    StormCell cell;
    cell.active = HashToUnit(seed + 0u) >= kSkipChance;
    if (!cell.active) return cell;
    cell.spawnX = kSeaCenterX + (HashToUnit(seed + 1u) * 2.0f - 1.0f) * kSeaHalfExtentX;
    cell.spawnZ = kSeaCenterZ + (HashToUnit(seed + 2u) * 2.0f - 1.0f) * kSeaHalfExtentZ;
    float angle = HashToUnit(seed + 3u) * 2.0f * kPi;
    cell.dirX = std::cos(angle);
    cell.dirZ = std::sin(angle);
    cell.speed = kMinDriftSpeed + HashToUnit(seed + 4u) * (kMaxDriftSpeed - kMinDriftSpeed);
    cell.radius = kMinRadius + HashToUnit(seed + 5u) * (kMaxRadius - kMinRadius);
    cell.peakIntensity = kMinPeakIntensity + HashToUnit(seed + 6u) * (kMaxPeakIntensity - kMinPeakIntensity);
    cell.lifespan = kMinLifespan + HashToUnit(seed + 7u) * (kMaxLifespan - kMinLifespan);
    return cell;
}

// 0 at both ends of [0,1], 1 through the middle, smooth 20%-of-lifespan
// fade in/out — a storm builds up and dissipates, it doesn't switch on/off.
inline float SmoothPulse(float u) {
    float fadeIn = std::min(1.0f, u / 0.2f);
    float fadeOut = std::min(1.0f, (1.0f - u) / 0.2f);
    return std::clamp(std::min(fadeIn, fadeOut), 0.0f, 1.0f);
}

inline float SlotIntensity(int slot, float t, float x, float z) {
    long long cycleIndex = static_cast<long long>(std::floor(t / kCycleDuration));
    float tInCycle = t - static_cast<float>(cycleIndex) * kCycleDuration;
    StormCell cell = DeriveStormCell(slot, cycleIndex);
    if (!cell.active || tInCycle > cell.lifespan) return 0.0f;

    float envelope = SmoothPulse(tInCycle / cell.lifespan);
    float centerX = cell.spawnX + cell.dirX * cell.speed * tInCycle;
    float centerZ = cell.spawnZ + cell.dirZ * cell.speed * tInCycle;
    float dist = HorizontalDistance(Vec3(x, 0.0f, z), Vec3(centerX, 0.0f, centerZ));
    float falloff = 1.0f - std::clamp(dist / cell.radius, 0.0f, 1.0f);
    falloff = falloff * falloff * (3.0f - 2.0f * falloff);  // smoothstep
    return cell.peakIntensity * envelope * falloff;
}

constexpr float kAmbientMin = 0.1f;
constexpr float kAmbientMax = 0.35f;

// Baseline wind that's always present everywhere, slowly drifting between
// kAmbientMin and kAmbientMax — a real sea is essentially never perfectly
// flat (true dead calm exists but is the exception, not the every-minute
// default), so "no storm nearby" should still mean gentle chop, not glass.
// Two slow, differently-periodled sines (not one) so it doesn't feel like a
// metronome. Storm cells (below) add on top of this, they don't replace it.
inline float AmbientWind(float t) {
    float a = std::sin(2.0f * kPi * t / 900.0f + 0.7f);
    float b = std::sin(2.0f * kPi * t / 1400.0f + 2.3f);
    float raw = (a + b) * 0.5f;  // in [-1,1]
    return kAmbientMin + (kAmbientMax - kAmbientMin) * 0.5f * (raw + 1.0f);
}

// Wind intensity in [0,1] at world position (x,z) at simulated time t —
// AmbientWind(t) (spatially uniform, always on) plus every slot's current
// storm-cell contribution (cells can overlap each other and the ambient
// baseline), clamped. Never drops to true 0 the way SlotIntensity alone can.
inline float WindIntensity(float x, float z, float t) {
    float total = AmbientWind(t);
    for (int slot = 0; slot < kStormSlots; ++slot) {
        total += SlotIntensity(slot, t, x, z);
    }
    return std::clamp(total, 0.0f, 1.0f);
}

}  // namespace Weather
}  // namespace archipelago
