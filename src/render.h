#pragma once

// --- Rendering: real 3D (Fase 4.5+) — perspective projection, depth testing,
// simple box/hull geometry, a wave-displaced water mesh (Fase 7.1). Still
// placeholder shapes (no imported models) — fidelity stays last priority. ---

#include <cstdint>
#include <vector>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>

#include "common.h"

namespace archipelago {

GLuint CompileShader(GLenum type, const char* source);

// Flat, unlit color — used only for the dock rings (line loops have no faces
// to shade against a light).
GLuint CreateUnlitShaderProgram();

// Simple directional (Lambertian) shading, shared by every lit surface
// (boxes and the water mesh) — one "sun", no shadows, no textures.
GLuint CreateLitShaderProgram();

// Fase 7.1: the water mesh's vertex shader displaces a flat world-space grid
// using the same Gerstner wave field as Waves::Height() in waves.h (kept in
// sync by hand — GLSL can't include the C++ header).
GLuint CreateWaterShaderProgram();

// Unit cube, centered at origin, extents -0.5..0.5, reused (scaled/
// translated/rotated via the model matrix) for island buildings and ship
// hulls. Uploads kCubeVertices into cubeVao/cubeVbo (both out-params).
void CreateCubeMesh(GLuint& cubeVao, GLuint& cubeVbo);

// Yaw-only overload — buildings/docks never rotate.
void DrawBox(GLuint cubeVao, GLint mvpLoc, GLint normalMatrixLoc, GLint colorLoc, const glm::mat4& viewProj,
             Vec3 center, Vec3 fullExtents, float headingRadians, float r, float g, float b);

// Fase 7.1: full-rotation overload, for ship hulls now that they actually
// pitch/roll on waves. rotation is CargoShip::rotation() — Jolt's raw body
// rotation, passed straight through with no sign adjustment (see that
// accessor's comment for why that's still consistent with the yaw-only path).
void DrawBox(GLuint cubeVao, GLint mvpLoc, GLint normalMatrixLoc, GLint colorLoc, const glm::mat4& viewProj,
             Vec3 center, Vec3 fullExtents, const glm::quat& rotation, float r, float g, float b);

// Fase 7.1: a flat XZ grid, one quad per cell, positions only (Y left at 0 —
// the water vertex shader displaces it per-frame using the wave field).
// Generated once and uploaded as a static buffer; only the shader's uTime
// uniform needs to change per frame, not the mesh itself.
void GenerateWaterGrid(float minX, float maxX, float minZ, float maxZ, int segments, std::vector<float>& outVertices,
                        std::vector<GLuint>& outIndices);

void DrawWater(GLuint waterVao, GLsizei indexCount, GLint viewProjLoc, GLint timeLoc, GLint colorLoc,
               const glm::mat4& viewProj, float waveTime);

// Marks the actual loading/unloading zone (the same radius CargoShip checks)
// as a ring on the water, so it's visible where a ship needs to be.
void DrawDockRing(GLuint ringVao, GLuint ringVbo, GLint mvpLoc, GLint colorLoc, const glm::mat4& viewProj,
                   Vec3 center, float radius);

// Fase 8.0 (Terreno procedural), paso 2/5: unlike the water grid, terrain
// doesn't animate, so its mesh is fully computed once on the CPU (position
// AND normal per vertex, normal via finite differences on
// Terrain::Height — that function is fBm noise, not a closed form
// like the wave field, so no clean analytic derivative the way the water
// shader has). Same interleaved position+normal layout as kCubeVertices, so
// it draws with the existing lit shader — no dedicated terrain shader
// needed, just an identity model matrix since the mesh is already in world
// space.
// offsetX/offsetZ shift the underlying noise sampling (see
// Terrain::FindBiggestIslandOffset) so the biggest island renders centered
// on the world, matching where CreateSeaFloorHeightFieldBody puts its
// collision.
void GenerateTerrainMesh(float centerX, float centerZ, float halfExtentX, float halfExtentZ, int segments,
                          uint32_t seed, float offsetX, float offsetZ, std::vector<float>& outVertices,
                          std::vector<GLuint>& outIndices);

void DrawTerrain(GLuint terrainVao, GLsizei indexCount, GLint mvpLoc, GLint normalMatrixLoc, GLint colorLoc,
                  const glm::mat4& viewProj, float r, float g, float b);

enum class CameraMode { ThirdPerson, FirstPerson };

// Third person: chases behind and above the ship. First person: at the ship's
// position, eye height, looking down its heading. Same forward-vector
// convention as movement (cos(heading), 0, sin(heading)). Yaw-only on
// purpose — even though the hull itself pitches/rolls on waves (Fase 7.1),
// the camera doesn't inherit that, so it doesn't feel seasick to watch.
glm::mat4 ComputeViewProj(CameraMode mode, Vec3 shipPos, float heading, float aspect);

// Fase 8.0 (Terreno procedural): top-down orthographic view of the whole sea
// — reuses every existing draw call (terrain, water, ships, docks) as-is,
// just seen from directly above with a different projection. No separate
// baked map texture needed. seaHalfExtent assumes a square sea (X and Z
// half-extents equal, true for kSeaHalfExtentX/Z). panX/panZ offset the
// view center (world units, click-drag); zoom scales the visible half-
// extent (<1 = zoomed in, >1 = zoomed out, scroll wheel).
glm::mat4 ComputeMapViewProj(float seaCenterX, float seaCenterZ, float seaHalfExtent, float aspect, float panX,
                              float panZ, float zoom);

// Projects a world position through view*projection into screen pixel space,
// for placing ImGui text labels over 3D objects. Returns false if the point
// is behind the camera (label shouldn't be drawn).
bool WorldToScreen(Vec3 worldPos, const glm::mat4& viewProj, ImVec2& outScreen);

}  // namespace archipelago
