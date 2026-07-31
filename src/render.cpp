#include "render.h"

#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace archipelago {

namespace {

// Shared by every lit surface (boxes and the water mesh) — one "sun", no
// shadows, no textures. Just needs a world-space-ish normal in vNormal;
// doesn't care how the vertex shader got there (a normal matrix for rigid
// boxes, wave-derivative math for the water).
constexpr const char* kLitFragmentSrc = R"(
    #version 330 core
    in vec3 vNormal;
    uniform vec3 uColor;
    uniform vec3 uLightDir;  // direction the light travels (sun -> surface)
    out vec4 FragColor;
    void main() {
        vec3 n = normalize(vNormal);
        float diff = max(dot(n, -uLightDir), 0.0);
        vec3 ambient = 0.35 * uColor;
        vec3 diffuse = 0.65 * uColor * diff;
        FragColor = vec4(ambient + diffuse, 1.0);
    }
)";

// Unit cube, centered at origin, extents -0.5..0.5. Reused (scaled +
// translated + rotated via the model matrix) for every box-shaped thing:
// island buildings and the ship hull (the water plane got its own wave-
// displaced grid mesh in Fase 7.1, see GenerateWaterGrid/DrawWater).
// 36 vertices, unindexed triangles, position + per-face normal interleaved.
constexpr float kCubeVertices[] = {
    // -Z face (normal 0,0,-1)
    -0.5f, -0.5f, -0.5f, 0, 0, -1,  0.5f, -0.5f, -0.5f, 0, 0, -1,  0.5f, 0.5f, -0.5f, 0, 0, -1,
    0.5f, 0.5f, -0.5f, 0, 0, -1,  -0.5f, 0.5f, -0.5f, 0, 0, -1,  -0.5f, -0.5f, -0.5f, 0, 0, -1,
    // +Z face (normal 0,0,1)
    -0.5f, -0.5f, 0.5f, 0, 0, 1,  0.5f, -0.5f, 0.5f, 0, 0, 1,  0.5f, 0.5f, 0.5f, 0, 0, 1,
    0.5f, 0.5f, 0.5f, 0, 0, 1,  -0.5f, 0.5f, 0.5f, 0, 0, 1,  -0.5f, -0.5f, 0.5f, 0, 0, 1,
    // -X face (normal -1,0,0)
    -0.5f, 0.5f, 0.5f, -1, 0, 0,  -0.5f, 0.5f, -0.5f, -1, 0, 0,  -0.5f, -0.5f, -0.5f, -1, 0, 0,
    -0.5f, -0.5f, -0.5f, -1, 0, 0,  -0.5f, -0.5f, 0.5f, -1, 0, 0,  -0.5f, 0.5f, 0.5f, -1, 0, 0,
    // +X face (normal 1,0,0)
    0.5f, 0.5f, 0.5f, 1, 0, 0,  0.5f, 0.5f, -0.5f, 1, 0, 0,  0.5f, -0.5f, -0.5f, 1, 0, 0,
    0.5f, -0.5f, -0.5f, 1, 0, 0,  0.5f, -0.5f, 0.5f, 1, 0, 0,  0.5f, 0.5f, 0.5f, 1, 0, 0,
    // -Y face (normal 0,-1,0)
    -0.5f, -0.5f, -0.5f, 0, -1, 0,  0.5f, -0.5f, -0.5f, 0, -1, 0,  0.5f, -0.5f, 0.5f, 0, -1, 0,
    0.5f, -0.5f, 0.5f, 0, -1, 0,  -0.5f, -0.5f, 0.5f, 0, -1, 0,  -0.5f, -0.5f, -0.5f, 0, -1, 0,
    // +Y face (normal 0,1,0)
    -0.5f, 0.5f, -0.5f, 0, 1, 0,  0.5f, 0.5f, -0.5f, 0, 1, 0,  0.5f, 0.5f, 0.5f, 0, 1, 0,
    0.5f, 0.5f, 0.5f, 0, 1, 0,  -0.5f, 0.5f, 0.5f, 0, 1, 0,  -0.5f, 0.5f, -0.5f, 0, 1, 0,
};
constexpr int kCubeVertexCount = 36;
constexpr int kCubeFloatsPerVertex = 6;  // position (3) + normal (3)

GLuint LinkProgram(GLuint vs, GLuint fs) {
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

}  // namespace

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

GLuint CreateUnlitShaderProgram() {
    static const char* kVertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 uMVP;
        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
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
    return LinkProgram(vs, fs);
}

// Simple directional (Lambertian) shading for boxes — one "sun", no shadows,
// no textures. Cheap enough to add now (no new assets, just per-vertex
// normals + a dot product) and it already reads much better as solid objects
// than flat unlit color. Real lighting/materials/shadows stay deferred —
// this is one rung up the ladder, not the top of it.
GLuint CreateLitShaderProgram() {
    static const char* kVertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        uniform mat4 uMVP;
        uniform mat3 uNormalMatrix;
        out vec3 vNormal;
        void main() {
            vNormal = uNormalMatrix * aNormal;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kLitFragmentSrc);
    return LinkProgram(vs, fs);
}

// Fase 7.1: the water mesh's vertex shader displaces a flat world-space grid
// using the same Gerstner wave field as Waves::Height() in C++ (kept in sync
// by hand — GLSL can't include the C++ header). Adds a modest horizontal
// "peak" displacement (kSteepness) on top of the height, for the pointier
// crests/rounder troughs look real Gerstner waves have — small enough not to
// affect where the hull actually floats (that still only samples height).
// The normal is the analytic derivative of the height field only (ignoring
// the horizontal term's small effect on it) — an accepted approximation at
// this steepness.
GLuint CreateWaterShaderProgram() {
    static const char* kVertexSrc = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        uniform mat4 uViewProj;
        uniform float uTime;
        out vec3 vNormal;

        // Mirrors Waves::kComponents in waves.h.
        const float kDirection[3] = float[3](0.3, 1.1, 2.4);
        const float kWavelength[3] = float[3](220.0, 130.0, 340.0);
        const float kAmplitude[3] = float[3](3.0, 1.8, 2.4);
        const float kSpeed[3] = float[3](9.0, 6.5, 11.0);
        const float kSteepness = 0.3;

        void main() {
            float x = aPos.x;
            float z = aPos.z;
            float h = 0.0;
            float dx = 0.0;
            float dz = 0.0;
            float dhdx = 0.0;
            float dhdz = 0.0;
            for (int i = 0; i < 3; ++i) {
                float k = 6.28318530718 / kWavelength[i];
                float dirX = cos(kDirection[i]);
                float dirZ = sin(kDirection[i]);
                float phase = k * (dirX * x + dirZ * z) - kSpeed[i] * k * uTime;
                float s = sin(phase);
                float c = cos(phase);
                h += kAmplitude[i] * s;
                dx += kSteepness * kAmplitude[i] * dirX * c;
                dz += kSteepness * kAmplitude[i] * dirZ * c;
                dhdx += kAmplitude[i] * k * dirX * c;
                dhdz += kAmplitude[i] * k * dirZ * c;
            }
            vNormal = normalize(vec3(-dhdx, 1.0, -dhdz));
            vec3 worldPos = vec3(x + dx, h, z + dz);
            gl_Position = uViewProj * vec4(worldPos, 1.0);
        }
    )";
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kLitFragmentSrc);
    return LinkProgram(vs, fs);
}

void CreateCubeMesh(GLuint& cubeVao, GLuint& cubeVbo) {
    glGenVertexArrays(1, &cubeVao);
    glGenBuffers(1, &cubeVbo);
    glBindVertexArray(cubeVao);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kCubeVertices), kCubeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kCubeFloatsPerVertex * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kCubeFloatsPerVertex * sizeof(float),
                           reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void DrawBox(GLuint cubeVao, GLint mvpLoc, GLint normalMatrixLoc, GLint colorLoc, const glm::mat4& viewProj,
             Vec3 center, Vec3 fullExtents, float headingRadians, float r, float g, float b) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    // Local +X is the model's "forward"; rotate by -heading so it lines up
    // with the world-forward convention used everywhere else
    // (cos(heading), 0, sin(heading)) — see CargoShip::ApplyInput.
    model = glm::rotate(model, -headingRadians, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, fullExtents);
    glm::mat4 mvp = viewProj * model;
    // Non-uniform scale (buildings/ship have different extents per axis)
    // means normals need the inverse-transpose, not the model matrix
    // directly, or they'd skew and lighting would look wrong on stretched boxes.
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniform3f(colorLoc, r, g, b);
    glBindVertexArray(cubeVao);
    glDrawArrays(GL_TRIANGLES, 0, kCubeVertexCount);
}

void DrawBox(GLuint cubeVao, GLint mvpLoc, GLint normalMatrixLoc, GLint colorLoc, const glm::mat4& viewProj,
             Vec3 center, Vec3 fullExtents, const glm::quat& rotation, float r, float g, float b) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model = model * glm::mat4_cast(rotation);
    model = glm::scale(model, fullExtents);
    glm::mat4 mvp = viewProj * model;
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniform3f(colorLoc, r, g, b);
    glBindVertexArray(cubeVao);
    glDrawArrays(GL_TRIANGLES, 0, kCubeVertexCount);
}

void GenerateWaterGrid(float minX, float maxX, float minZ, float maxZ, int segments, std::vector<float>& outVertices,
                        std::vector<GLuint>& outIndices) {
    int verticesPerSide = segments + 1;
    outVertices.clear();
    outVertices.reserve(static_cast<size_t>(verticesPerSide) * verticesPerSide * 3);
    for (int iz = 0; iz < verticesPerSide; ++iz) {
        float z = minZ + (maxZ - minZ) * (static_cast<float>(iz) / static_cast<float>(segments));
        for (int ix = 0; ix < verticesPerSide; ++ix) {
            float x = minX + (maxX - minX) * (static_cast<float>(ix) / static_cast<float>(segments));
            outVertices.push_back(x);
            outVertices.push_back(0.0f);
            outVertices.push_back(z);
        }
    }

    outIndices.clear();
    outIndices.reserve(static_cast<size_t>(segments) * segments * 6);
    for (int iz = 0; iz < segments; ++iz) {
        for (int ix = 0; ix < segments; ++ix) {
            GLuint topLeft = static_cast<GLuint>(iz * verticesPerSide + ix);
            GLuint topRight = topLeft + 1;
            GLuint bottomLeft = static_cast<GLuint>((iz + 1) * verticesPerSide + ix);
            GLuint bottomRight = bottomLeft + 1;
            outIndices.push_back(topLeft);
            outIndices.push_back(bottomLeft);
            outIndices.push_back(topRight);
            outIndices.push_back(topRight);
            outIndices.push_back(bottomLeft);
            outIndices.push_back(bottomRight);
        }
    }
}

void DrawWater(GLuint waterVao, GLsizei indexCount, GLint viewProjLoc, GLint timeLoc, GLint colorLoc,
               const glm::mat4& viewProj, float waveTime) {
    glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform1f(timeLoc, waveTime);
    glUniform3f(colorLoc, 0.15f, 0.35f, 0.55f);
    glBindVertexArray(waterVao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

namespace {
constexpr int kDockRingSegments = 32;
}  // namespace

void DrawDockRing(GLuint ringVao, GLuint ringVbo, GLint mvpLoc, GLint colorLoc, const glm::mat4& viewProj,
                   Vec3 center, float radius) {
    float vertices[kDockRingSegments * 3];
    for (int i = 0; i < kDockRingSegments; ++i) {
        float angle = (static_cast<float>(i) / kDockRingSegments) * 2.0f * kPi;
        vertices[i * 3 + 0] = center.x + std::cos(angle) * radius;
        vertices[i * 3 + 1] = 1.0f;
        vertices[i * 3 + 2] = center.z + std::sin(angle) * radius;
    }

    glm::mat4 mvp = viewProj;  // vertices already in world space, model = identity
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
    glBindVertexArray(ringVao);
    glBindBuffer(GL_ARRAY_BUFFER, ringVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_LINE_LOOP, 0, kDockRingSegments);
}

glm::mat4 ComputeViewProj(CameraMode mode, Vec3 shipPos, float heading, float aspect) {
    glm::vec3 forward(std::cos(heading), 0.0f, std::sin(heading));
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 eye, target;

    if (mode == CameraMode::ThirdPerson) {
        constexpr float kChaseDistance = 180.0f;
        constexpr float kChaseHeight = 90.0f;
        eye = shipPos - forward * kChaseDistance + up * kChaseHeight;
        target = shipPos + up * 15.0f;
    } else {
        constexpr float kEyeHeight = 14.0f;
        eye = shipPos + up * kEyeHeight;
        target = eye + forward * 10.0f;
    }

    glm::mat4 view = glm::lookAt(eye, target, up);
    glm::mat4 projection = glm::perspective(glm::radians(65.0f), aspect, 1.0f, 50000.0f);
    return projection * view;
}

bool WorldToScreen(Vec3 worldPos, const glm::mat4& viewProj, ImVec2& outScreen) {
    glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    outScreen.x = (ndc.x * 0.5f + 0.5f) * kWindowWidth;
    outScreen.y = (1.0f - (ndc.y * 0.5f + 0.5f)) * kWindowHeight;
    return true;
}

}  // namespace archipelago
