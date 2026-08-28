#include "pch.h"
#include "debug_draw.h"

struct DebugEyeRenderState {
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::mat4 viewProjection = glm::mat4(1.0f);
    bool hasView = false;
    bool hasProjection = false;

    void Reset() {
        view = glm::mat4(1.0f);
        projection = glm::mat4(1.0f);
        viewProjection = glm::mat4(1.0f);
        hasView = false;
        hasProjection = false;
    }

    void UpdateView(const glm::mat4& newView) {
        view = newView;
        hasView = true;
        if (hasProjection) {
            viewProjection = projection * view;
        }
    }

    void UpdateProjection(const glm::mat4& newProjection) {
        projection = newProjection;
        hasProjection = true;
        if (hasView) {
            viewProjection = projection * view;
        }
    }

    bool IsValid() const {
        return hasView && hasProjection;
    }
};

static std::mutex s_debugEyeStateMutex;
static std::array<DebugEyeRenderState, 2> s_latestDebugEyeStates = {};
static std::array<std::array<DebugEyeRenderState, 2>, 2> s_frameDebugEyeStates = {};

static constexpr int BOX_EDGES[12][2] = {
    { 0, 1 },
    { 1, 2 },
    { 2, 3 },
    { 3, 0 },
    { 4, 5 },
    { 5, 6 },
    { 6, 7 },
    { 7, 4 },
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 },
};

static constexpr int BOX_FACES[6][4] = {
    { 0, 1, 2, 3 },
    { 4, 5, 6, 7 },
    { 0, 1, 5, 4 },
    { 3, 2, 6, 7 },
    { 0, 4, 7, 3 },
    { 1, 5, 6, 2 },
};

static constexpr int kPolylineTubeSideCount = 6;

static uint8_t GetColorChannel(uint32_t color, int shift) {
    return (uint8_t)((color >> shift) & 0xFFu);
}

static float GetColorAlpha01(uint32_t color) {
    return (float)GetColorChannel(color, 24) / 255.0f;
}

static uint32_t ScaleColor(uint32_t color, float rgbScale, float alphaScale = 1.0f) {
    const uint8_t r = (uint8_t)glm::clamp((int)std::lround((float)GetColorChannel(color, 0) * rgbScale), 0, 255);
    const uint8_t g = (uint8_t)glm::clamp((int)std::lround((float)GetColorChannel(color, 8) * rgbScale), 0, 255);
    const uint8_t b = (uint8_t)glm::clamp((int)std::lround((float)GetColorChannel(color, 16) * rgbScale), 0, 255);
    const uint8_t a = (uint8_t)glm::clamp((int)std::lround((float)GetColorChannel(color, 24) * alphaScale), 0, 255);
    return DebugDrawColor(r, g, b, a);
}

static int ResolveCircleSegments(float radius, int segments) {
    if (segments > 0) {
        return segments;
    }

    return glm::clamp((int)std::ceil(radius * 24.0f), 24, 96);
}

static int ResolveArcSegments(float duration, int segments) {
    if (segments > 0) {
        return segments;
    }

    return glm::clamp((int)std::ceil(duration * 36.0f), 12, 96);
}

static float ResolveArcWidth(float duration) {
    return glm::clamp(duration * 0.03f, 0.025f, 0.06f);
}

static float ResolvePolylineRadius(float thickness) {
    return glm::clamp(thickness * 0.008f, 0.012f, 0.04f);
}

static void AddRenderLine(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, uint32_t color) {
    renderData.lineVertices.push_back({ a, color });
    renderData.lineVertices.push_back({ b, color });
}

static void AddRenderXRayLine(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, uint32_t color) {
    renderData.xrayLineVertices.push_back({ a, color });
    renderData.xrayLineVertices.push_back({ b, color });
}

static void AddPlanarCircle(DebugDrawRenderData& renderData, const glm::vec3& center, const glm::vec3& axisX, const glm::vec3& axisY, uint32_t color, int segments, bool xray = false) {
    if (segments < 3) {
        return;
    }

    glm::vec3 previousPoint = center + axisX;
    for (int i = 1; i <= segments; ++i) {
        const float angle = (glm::two_pi<float>() * (float)i) / (float)segments;
        const glm::vec3 point = center + (axisX * std::cos(angle)) + (axisY * std::sin(angle));
        if (xray) {
            AddRenderXRayLine(renderData, previousPoint, point, color);
        }
        else {
            AddRenderLine(renderData, previousPoint, point, color);
        }
        previousPoint = point;
    }
}

static void AddArcLines(DebugDrawRenderData& renderData, const glm::vec3& center, const glm::vec3& axisA, const glm::vec3& axisB, float startAngle, float endAngle, uint32_t color, int segments, bool xray = false) {
    if (segments < 1) {
        return;
    }

    glm::vec3 previousPoint = center + (axisA * std::cos(startAngle)) + (axisB * std::sin(startAngle));
    for (int i = 1; i <= segments; ++i) {
        const float t = (float)i / (float)segments;
        const float angle = startAngle + ((endAngle - startAngle) * t);
        const glm::vec3 point = center + (axisA * std::cos(angle)) + (axisB * std::sin(angle));
        if (xray) {
            AddRenderXRayLine(renderData, previousPoint, point, color);
        }
        else {
            AddRenderLine(renderData, previousPoint, point, color);
        }
        previousPoint = point;
    }
}

static void AddTriangle(DebugDrawRenderData& renderData, const DebugDrawTriangleVertex& a, const DebugDrawTriangleVertex& b, const DebugDrawTriangleVertex& c, bool xray = false) {
    auto& triangleVertices = xray ? renderData.xrayTriangleVertices : renderData.triangleVertices;
    triangleVertices.push_back(a);
    triangleVertices.push_back(b);
    triangleVertices.push_back(c);
}

static void AddTriangle(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, uint32_t color) {
    AddTriangle(renderData, DebugDrawTriangleVertex{ a, color }, DebugDrawTriangleVertex{ b, color }, DebugDrawTriangleVertex{ c, color });
}

static void AddXRayTriangle(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, uint32_t color) {
    AddTriangle(renderData, DebugDrawTriangleVertex{ a, color }, DebugDrawTriangleVertex{ b, color }, DebugDrawTriangleVertex{ c, color }, true);
}

static void AddQuad(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, uint32_t color) {
    AddTriangle(renderData, a, b, c, color);
    AddTriangle(renderData, a, c, d, color);
}

static void AddXRayQuad(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, uint32_t color) {
    AddXRayTriangle(renderData, a, b, c, color);
    AddXRayTriangle(renderData, a, c, d, color);
}

static glm::vec3 ComputePolylineTangent(const glm::vec3* points, uint32_t pointCount, uint32_t index) {
    if (pointCount < 2) {
        return glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::vec3 prev = index > 0 ? points[index] - points[index - 1] : glm::vec3(0.0f);
    const glm::vec3 next = index + 1 < pointCount ? points[index + 1] - points[index] : glm::vec3(0.0f);
    const float prevLengthSq = glm::length2(prev);
    const float nextLengthSq = glm::length2(next);
    if (prevLengthSq > 0.000001f && nextLengthSq > 0.000001f) {
        const glm::vec3 combined = glm::normalize(prev) + glm::normalize(next);
        if (glm::length2(combined) > 0.000001f) {
            return glm::normalize(combined);
        }
    }

    if (nextLengthSq > 0.000001f) {
        return glm::normalize(next);
    }

    if (prevLengthSq > 0.000001f) {
        return glm::normalize(prev);
    }

    return glm::vec3(0.0f, 0.0f, 1.0f);
}

static glm::vec3 ComputeTubeFrameNormal(const glm::vec3& tangent, const glm::vec3& previousNormal) {
    glm::vec3 normal = previousNormal - tangent * glm::dot(previousNormal, tangent);
    if (glm::length2(normal) > 0.000001f) {
        return glm::normalize(normal);
    }

    normal = glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), tangent);
    if (glm::length2(normal) > 0.000001f) {
        return glm::normalize(normal);
    }

    normal = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), tangent);
    if (glm::length2(normal) > 0.000001f) {
        return glm::normalize(normal);
    }

    normal = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), tangent);
    if (glm::length2(normal) > 0.000001f) {
        return glm::normalize(normal);
    }

    return glm::vec3(1.0f, 0.0f, 0.0f);
}

static uint32_t ShadeTubeColor(uint32_t color, const glm::vec3& normal) {
    static const glm::vec3 s_lightDir = glm::normalize(glm::vec3(-0.32f, 0.84f, 0.44f));
    static const glm::vec3 s_fillDir = glm::normalize(glm::vec3(0.58f, 0.18f, -0.79f));
    const float keyLight = glm::max(glm::dot(normal, s_lightDir), 0.0f);
    const float fillLight = glm::max(glm::dot(normal, s_fillDir), 0.0f);
    const float skyLight = glm::max(normal.y, 0.0f);
    const float shade = glm::clamp(0.42f + (keyLight * 0.34f) + (fillLight * 0.12f) + (skyLight * 0.16f), 0.34f, 1.04f);
    return ScaleColor(color, shade);
}

static void AddShadedQuad(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& normalA, const glm::vec3& normalB, const glm::vec3& normalC, const glm::vec3& normalD, uint32_t previousColor, uint32_t currentColor, bool xray) {
    AddTriangle(
        renderData,
        DebugDrawTriangleVertex{ a, ShadeTubeColor(previousColor, normalA) },
        DebugDrawTriangleVertex{ b, ShadeTubeColor(previousColor, normalB) },
        DebugDrawTriangleVertex{ c, ShadeTubeColor(currentColor, normalC) },
        xray
    );
    AddTriangle(
        renderData,
        DebugDrawTriangleVertex{ a, ShadeTubeColor(previousColor, normalA) },
        DebugDrawTriangleVertex{ c, ShadeTubeColor(currentColor, normalC) },
        DebugDrawTriangleVertex{ d, ShadeTubeColor(currentColor, normalD) },
        xray
    );
}

static void AddShadedQuad(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, const glm::vec3& normalA, const glm::vec3& normalB, const glm::vec3& normalC, const glm::vec3& normalD, uint32_t color, bool xray) {
    AddShadedQuad(renderData, a, b, c, d, normalA, normalB, normalC, normalD, color, color, xray);
}

static void AddPolylineTube(DebugDrawRenderData& renderData, const glm::vec3* points, uint32_t pointCount, float radius, uint32_t color, bool xray) {
    if (points == nullptr || pointCount < 2 || radius <= 0.0f) {
        return;
    }

    auto& triangleVertices = xray ? renderData.xrayTriangleVertices : renderData.triangleVertices;
    triangleVertices.reserve(triangleVertices.size() + ((pointCount - 1) * kPolylineTubeSideCount * 6));

    std::array<glm::vec3, kPolylineTubeSideCount> previousRingPositions = {};
    std::array<glm::vec3, kPolylineTubeSideCount> previousRingNormals = {};
    std::array<glm::vec3, kPolylineTubeSideCount> currentRingPositions = {};
    std::array<glm::vec3, kPolylineTubeSideCount> currentRingNormals = {};
    glm::vec3 previousFrameNormal = ComputeTubeFrameNormal(ComputePolylineTangent(points, pointCount, 0), glm::vec3(0.0f, 1.0f, 0.0f));

    for (uint32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        const glm::vec3 tangent = ComputePolylineTangent(points, pointCount, pointIndex);
        const glm::vec3 ringNormal = ComputeTubeFrameNormal(tangent, previousFrameNormal);
        const glm::vec3 ringBitangent = glm::normalize(glm::cross(tangent, ringNormal));
        previousFrameNormal = ringNormal;

        auto& ringPositions = pointIndex == 0 ? previousRingPositions : currentRingPositions;
        auto& ringNormals = pointIndex == 0 ? previousRingNormals : currentRingNormals;
        for (int side = 0; side < kPolylineTubeSideCount; ++side) {
            const float angle = (glm::two_pi<float>() * (float)side) / (float)kPolylineTubeSideCount;
            const glm::vec3 radial = (ringNormal * std::cos(angle)) + (ringBitangent * std::sin(angle));
            ringPositions[side] = points[pointIndex] + radial * radius;
            ringNormals[side] = radial;
        }

        if (pointIndex == 0) {
            continue;
        }

        for (int side = 0; side < kPolylineTubeSideCount; ++side) {
            const int nextSide = (side + 1) % kPolylineTubeSideCount;
            AddShadedQuad(
                renderData,
                previousRingPositions[side],
                previousRingPositions[nextSide],
                currentRingPositions[nextSide],
                currentRingPositions[side],
                previousRingNormals[side],
                previousRingNormals[nextSide],
                currentRingNormals[nextSide],
                currentRingNormals[side],
                color,
                xray
            );
        }

        previousRingPositions = currentRingPositions;
        previousRingNormals = currentRingNormals;
    }
}

static void AddPolylineTube(DebugDrawRenderData& renderData, const glm::vec3* points, uint32_t pointCount, float radius, const uint32_t* colors, bool xray) {
    if (points == nullptr || pointCount < 2 || radius <= 0.0f || colors == nullptr) {
        return;
    }

    auto& triangleVertices = xray ? renderData.xrayTriangleVertices : renderData.triangleVertices;
    triangleVertices.reserve(triangleVertices.size() + ((pointCount - 1) * kPolylineTubeSideCount * 6));

    std::array<glm::vec3, kPolylineTubeSideCount> previousRingPositions = {};
    std::array<glm::vec3, kPolylineTubeSideCount> previousRingNormals = {};
    std::array<glm::vec3, kPolylineTubeSideCount> currentRingPositions = {};
    std::array<glm::vec3, kPolylineTubeSideCount> currentRingNormals = {};
    glm::vec3 previousFrameNormal = ComputeTubeFrameNormal(ComputePolylineTangent(points, pointCount, 0), glm::vec3(0.0f, 1.0f, 0.0f));

    for (uint32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
        const glm::vec3 tangent = ComputePolylineTangent(points, pointCount, pointIndex);
        const glm::vec3 ringNormal = ComputeTubeFrameNormal(tangent, previousFrameNormal);
        const glm::vec3 ringBitangent = glm::normalize(glm::cross(tangent, ringNormal));
        previousFrameNormal = ringNormal;

        auto& ringPositions = pointIndex == 0 ? previousRingPositions : currentRingPositions;
        auto& ringNormals = pointIndex == 0 ? previousRingNormals : currentRingNormals;
        for (int side = 0; side < kPolylineTubeSideCount; ++side) {
            const float angle = (glm::two_pi<float>() * (float)side) / (float)kPolylineTubeSideCount;
            const glm::vec3 radial = (ringNormal * std::cos(angle)) + (ringBitangent * std::sin(angle));
            ringPositions[side] = points[pointIndex] + radial * radius;
            ringNormals[side] = radial;
        }

        if (pointIndex == 0) {
            continue;
        }

        const uint32_t previousColor = colors[pointIndex - 1];
        const uint32_t currentColor = colors[pointIndex];
        const float previousRadiusScale = glm::smoothstep(0.0f, 1.0f, GetColorAlpha01(previousColor));
        const float currentRadiusScale = glm::smoothstep(0.0f, 1.0f, GetColorAlpha01(currentColor));
        const float previousRadius = glm::max(radius * previousRadiusScale, radius * 0.04f);
        const float currentRadius = glm::max(radius * currentRadiusScale, radius * 0.04f);

        for (int side = 0; side < kPolylineTubeSideCount; ++side) {
            previousRingPositions[side] = points[pointIndex - 1] + previousRingNormals[side] * previousRadius;
            currentRingPositions[side] = points[pointIndex] + currentRingNormals[side] * currentRadius;
        }

        for (int side = 0; side < kPolylineTubeSideCount; ++side) {
            const int nextSide = (side + 1) % kPolylineTubeSideCount;
            AddShadedQuad(
                renderData,
                previousRingPositions[side],
                previousRingPositions[nextSide],
                currentRingPositions[nextSide],
                currentRingPositions[side],
                previousRingNormals[side],
                previousRingNormals[nextSide],
                currentRingNormals[nextSide],
                currentRingNormals[side],
                previousColor,
                currentColor,
                xray
            );
        }

        previousRingPositions = currentRingPositions;
        previousRingNormals = currentRingNormals;
    }
}

static void AddSolidBox(DebugDrawRenderData& renderData, const glm::vec3* corners, uint32_t color, bool xray) {
    static constexpr float FACE_SHADING[6] = {
        0.78f,
        1.0f,
        0.9f,
        0.7f,
        0.82f,
        0.62f,
    };

    const float baseAlphaScale = GetColorChannel(color, 24) < 255 ? 0.65f : 0.35f;
    for (int i = 0; i < 6; ++i) {
        const int* face = BOX_FACES[i];
        const uint32_t faceColor = ScaleColor(color, FACE_SHADING[i], baseAlphaScale);
        if (xray) {
            AddXRayQuad(renderData, corners[face[0]], corners[face[1]], corners[face[2]], corners[face[3]], faceColor);
        }
        else {
            AddQuad(renderData, corners[face[0]], corners[face[1]], corners[face[2]], corners[face[3]], faceColor);
        }
    }
}

void DebugDraw::Line(const glm::vec3& a, const glm::vec3& b, uint32_t color, float thickness, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::LINE, color, thickness };
    primitive.a = a;
    primitive.b = b;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::Dot(const glm::vec3& position, float radius, uint32_t color, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::DOT, color, 1.0f };
    primitive.a = position;
    primitive.radius = radius;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::Sphere(const glm::vec3& position, float radius, uint32_t color, int segments, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::SPHERE, color, 1.0f };
    primitive.a = position;
    primitive.radius = radius;
    primitive.segments = segments;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::Capsule(const glm::vec3& center, float radius, float halfHeight, uint32_t color, float thickness, int segments, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::CAPSULE, color, thickness };
    primitive.a = center;
    primitive.radius = radius;
    primitive.duration = halfHeight;
    primitive.segments = segments;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::PhysicsBody(const glm::vec3& center, float radius, float halfHeight, uint32_t color, float thickness, int segments, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::PHYSICS_BODY, color, thickness };
    primitive.a = center;
    primitive.radius = radius;
    primitive.duration = halfHeight;
    primitive.segments = segments;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::Circle(const glm::vec3& position, float radius, uint32_t color, float thickness, int segments, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::CIRCLE, color, thickness };
    primitive.a = position;
    primitive.radius = radius;
    primitive.segments = segments;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::Polyline(std::span<const glm::vec3> points, uint32_t color, float thickness, bool xray) {
    if (points.size() < 2) {
        return;
    }

    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::POLYLINE, color, thickness };
    primitive.xray = xray;
    primitive.points.assign(points.begin(), points.end());
    m_primitives.push_back(std::move(primitive));
}

void DebugDraw::Polyline(std::span<const glm::vec3> points, std::span<const uint32_t> colors, float thickness, bool xray) {
    if (points.size() < 2 || colors.size() < 2) {
        return;
    }

    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::POLYLINE, 0, thickness };
    primitive.xray = xray;
    primitive.points.assign(points.begin(), points.end());
    primitive.pointColors.assign(colors.begin(), colors.end());
    m_primitives.push_back(std::move(primitive));
}

void DebugDraw::Arc(const glm::vec3& start, const glm::vec3& initialVelocity, const glm::vec3& acceleration, float duration, uint32_t color, int segments, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::ARC, color, 1.0f };
    primitive.a = start;
    primitive.b = initialVelocity;
    primitive.c = acceleration;
    primitive.duration = duration;
    primitive.segments = segments;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::Box(const glm::vec3& min, const glm::vec3& max, uint32_t color, float thickness, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::AABB, color, thickness };
    primitive.a = min;
    primitive.b = max;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::Box(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, uint32_t color, float thickness, bool xray) {
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::ORIENTED_BOX, color, thickness };
    primitive.a = center;
    primitive.b = halfExtents;
    primitive.rotation = rotation;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::Frustum(const glm::mat4& viewProjection, uint32_t color, float thickness, bool xray) {
    glm::mat4 inv = glm::inverse(viewProjection);
    std::lock_guard lk(m_mutex);
    DebugPrimitive primitive = { PrimitiveType::FRUSTUM, color, thickness };
    primitive.inverseVP = inv;
    primitive.xray = xray;
    m_primitives.push_back(primitive);
}

void DebugDraw::UpdateEyeView(uint32_t eyeIndex, const glm::mat4& view) {
    if (eyeIndex >= s_latestDebugEyeStates.size()) {
        return;
    }

    std::scoped_lock lock(s_debugEyeStateMutex);
    s_latestDebugEyeStates[eyeIndex].UpdateView(view);
}

void DebugDraw::UpdateEyeProjection(uint32_t eyeIndex, const glm::mat4& projection) {
    if (eyeIndex >= s_latestDebugEyeStates.size()) {
        return;
    }

    std::scoped_lock lock(s_debugEyeStateMutex);
    s_latestDebugEyeStates[eyeIndex].UpdateProjection(projection);
}

void DebugDraw::SnapshotEyeState(uint32_t eyeIndex, long frameIdx) {
    if (eyeIndex >= s_latestDebugEyeStates.size() || frameIdx < 0 || frameIdx >= (long)s_frameDebugEyeStates.size()) {
        return;
    }

    std::scoped_lock lock(s_debugEyeStateMutex);
    s_frameDebugEyeStates[frameIdx][eyeIndex] = s_latestDebugEyeStates[eyeIndex];
}

void DebugDraw::AddLine(DebugDrawRenderData& renderData, const glm::vec3& a, const glm::vec3& b, uint32_t color, bool xray) {
    if (xray) {
        renderData.xrayLineVertices.push_back({ a, color });
        renderData.xrayLineVertices.push_back({ b, color });
        return;
    }

    renderData.lineVertices.push_back({ a, color });
    renderData.lineVertices.push_back({ b, color });
}

void DebugDraw::AddEdges(DebugDrawRenderData& renderData, const glm::vec3* corners, const int (*edges)[2], int edgeCount, uint32_t color, bool xray) {
    for (int i = 0; i < edgeCount; ++i) {
        AddLine(renderData, corners[edges[i][0]], corners[edges[i][1]], color, xray);
    }
}

DebugDrawRenderData DebugDraw::TakeRenderData(long frameIdx) {
    std::vector<DebugPrimitive> primitives;
    {
        std::lock_guard lk(m_mutex);
        primitives.swap(m_primitives);
    }

    DebugDrawRenderData renderData;
    renderData.lineVertices.reserve(primitives.size() * 4);
    renderData.xrayLineVertices.reserve(primitives.size() * 4);
    renderData.triangleVertices.reserve(primitives.size() * 12);
    renderData.xrayTriangleVertices.reserve(primitives.size() * 12);

    {
        std::scoped_lock lock(s_debugEyeStateMutex);
        if (frameIdx >= 0 && frameIdx < (long)s_frameDebugEyeStates.size()) {
            for (uint32_t eyeIndex = 0; eyeIndex < s_frameDebugEyeStates[frameIdx].size(); ++eyeIndex) {
                const DebugEyeRenderState& eyeState = s_frameDebugEyeStates[frameIdx][eyeIndex];
                if (eyeState.IsValid()) {
                    renderData.viewProjections[eyeIndex] = eyeState.viewProjection;
                    renderData.hasViewProjections[eyeIndex] = true;
                    renderData.cameraPos = glm::vec3(glm::inverse(eyeState.view)[3]);
                }
                s_frameDebugEyeStates[frameIdx][eyeIndex].Reset();
            }
        }
        else {
            for (uint32_t eyeIndex = 0; eyeIndex < s_latestDebugEyeStates.size(); ++eyeIndex) {
                const DebugEyeRenderState& eyeState = s_latestDebugEyeStates[eyeIndex];
                if (eyeState.IsValid()) {
                    renderData.viewProjections[eyeIndex] = eyeState.viewProjection;
                    renderData.hasViewProjections[eyeIndex] = true;
                    renderData.cameraPos = glm::vec3(glm::inverse(eyeState.view)[3]);
                }
            }
        }
    }

    for (const auto& prim : primitives) {
        switch (prim.type) {
            case PrimitiveType::LINE: {
                AddLine(renderData, prim.a, prim.b, prim.color, prim.xray);
                break;
            }

            case PrimitiveType::DOT: {
                const glm::vec3 xAxis = glm::vec3(prim.radius, 0.0f, 0.0f);
                const glm::vec3 yAxis = glm::vec3(0.0f, prim.radius, 0.0f);
                const glm::vec3 zAxis = glm::vec3(0.0f, 0.0f, prim.radius);
                AddLine(renderData, prim.a - xAxis, prim.a + xAxis, prim.color, prim.xray);
                AddLine(renderData, prim.a - yAxis, prim.a + yAxis, prim.color, prim.xray);
                AddLine(renderData, prim.a - zAxis, prim.a + zAxis, prim.color, prim.xray);
                break;
            }

            case PrimitiveType::SPHERE: {
                const int segments = ResolveCircleSegments(prim.radius, prim.segments);
                if (segments < 3 || prim.radius <= 0.0f) {
                    break;
                }

                AddPlanarCircle(renderData, prim.a, glm::vec3(prim.radius, 0.0f, 0.0f), glm::vec3(0.0f, prim.radius, 0.0f), prim.color, segments, prim.xray);
                AddPlanarCircle(renderData, prim.a, glm::vec3(prim.radius, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, prim.radius), prim.color, segments, prim.xray);
                AddPlanarCircle(renderData, prim.a, glm::vec3(0.0f, prim.radius, 0.0f), glm::vec3(0.0f, 0.0f, prim.radius), prim.color, segments, prim.xray);
                break;
            }

            case PrimitiveType::CAPSULE: {
                const float radius = prim.radius;
                const float halfHeight = std::max(prim.duration, radius);
                const int segments = ResolveCircleSegments(radius, prim.segments);
                if (radius <= 0.0f || segments < 4) {
                    break;
                }

                const float cylinderHalfHeight = std::max(halfHeight - radius, 0.0f);
                const glm::vec3 cylinderOffset = glm::vec3(0.0f, cylinderHalfHeight, 0.0f);
                const glm::vec3 topHemisphereCenter = prim.a + cylinderOffset;
                const glm::vec3 bottomHemisphereCenter = prim.a - cylinderOffset;
                const glm::vec3 radiusX = glm::vec3(radius, 0.0f, 0.0f);
                const glm::vec3 radiusY = glm::vec3(0.0f, radius, 0.0f);
                const glm::vec3 radiusZ = glm::vec3(0.0f, 0.0f, radius);
                const uint32_t seamColor = ScaleColor(prim.color, 0.82f, 0.72f);
                const int arcSegments = std::max(segments / 2, 2);

                AddPlanarCircle(renderData, topHemisphereCenter, radiusX, radiusZ, seamColor, segments, prim.xray);
                if (cylinderHalfHeight > 0.0f) {
                    AddPlanarCircle(renderData, bottomHemisphereCenter, radiusX, radiusZ, seamColor, segments, prim.xray);
                }

                AddArcLines(renderData, topHemisphereCenter, radiusY, radiusZ, glm::half_pi<float>(), 0.0f, prim.color, arcSegments, prim.xray);
                AddArcLines(renderData, topHemisphereCenter, radiusY, radiusZ, 0.0f, -glm::half_pi<float>(), prim.color, arcSegments, prim.xray);
                AddArcLines(renderData, topHemisphereCenter, radiusY, radiusX, glm::half_pi<float>(), 0.0f, prim.color, arcSegments, prim.xray);
                AddArcLines(renderData, topHemisphereCenter, radiusY, radiusX, 0.0f, -glm::half_pi<float>(), prim.color, arcSegments, prim.xray);

                AddArcLines(renderData, bottomHemisphereCenter, radiusY, radiusZ, glm::half_pi<float>(), glm::pi<float>(), prim.color, arcSegments, prim.xray);
                AddArcLines(renderData, bottomHemisphereCenter, radiusY, radiusZ, glm::pi<float>(), glm::pi<float>() + glm::half_pi<float>(), prim.color, arcSegments, prim.xray);
                AddArcLines(renderData, bottomHemisphereCenter, radiusY, radiusX, glm::half_pi<float>(), glm::pi<float>(), prim.color, arcSegments, prim.xray);
                AddArcLines(renderData, bottomHemisphereCenter, radiusY, radiusX, glm::pi<float>(), glm::pi<float>() + glm::half_pi<float>(), prim.color, arcSegments, prim.xray);

                AddLine(renderData, topHemisphereCenter + radiusX, bottomHemisphereCenter + radiusX, prim.color, prim.xray);
                AddLine(renderData, topHemisphereCenter - radiusX, bottomHemisphereCenter - radiusX, prim.color, prim.xray);
                AddLine(renderData, topHemisphereCenter + radiusZ, bottomHemisphereCenter + radiusZ, prim.color, prim.xray);
                AddLine(renderData, topHemisphereCenter - radiusZ, bottomHemisphereCenter - radiusZ, prim.color, prim.xray);
                break;
            }

            case PrimitiveType::PHYSICS_BODY: {
                const float radius = prim.radius;
                const float halfHeight = prim.duration;
                const int segments = ResolveCircleSegments(radius, prim.segments);
                if (radius <= 0.0f || halfHeight <= 0.0f || segments < 3) {
                    break;
                }

                const glm::vec3 topCenter = prim.a + glm::vec3(0.0f, halfHeight, 0.0f);
                const glm::vec3 bottomCenter = prim.a - glm::vec3(0.0f, halfHeight, 0.0f);
                const glm::vec3 radiusX = glm::vec3(radius, 0.0f, 0.0f);
                const glm::vec3 radiusZ = glm::vec3(0.0f, 0.0f, radius);

                AddPlanarCircle(renderData, topCenter, radiusX, radiusZ, prim.color, segments, prim.xray);
                AddPlanarCircle(renderData, prim.a, radiusX, radiusZ, ScaleColor(prim.color, 0.85f, 0.7f), segments, prim.xray);
                AddPlanarCircle(renderData, bottomCenter, radiusX, radiusZ, prim.color, segments, prim.xray);

                AddLine(renderData, topCenter + radiusX, bottomCenter + radiusX, prim.color, prim.xray);
                AddLine(renderData, topCenter - radiusX, bottomCenter - radiusX, prim.color, prim.xray);
                AddLine(renderData, topCenter + radiusZ, bottomCenter + radiusZ, prim.color, prim.xray);
                AddLine(renderData, topCenter - radiusZ, bottomCenter - radiusZ, prim.color, prim.xray);
                AddLine(renderData, bottomCenter, topCenter, ScaleColor(prim.color, 0.7f, 0.6f), prim.xray);
                break;
            }

            case PrimitiveType::CIRCLE: {
                const int segments = ResolveCircleSegments(prim.radius, prim.segments);
                if (segments < 3 || prim.radius <= 0.0f) {
                    break;
                }

                glm::vec3 previousPoint = prim.a + glm::vec3(prim.radius, 0.0f, 0.0f);
                for (int i = 1; i <= segments; ++i) {
                    const float angle = (glm::two_pi<float>() * (float)i) / (float)segments;
                    const glm::vec3 point = prim.a + glm::vec3(std::cos(angle) * prim.radius, 0.0f, std::sin(angle) * prim.radius);
                    AddLine(renderData, previousPoint, point, prim.color, prim.xray);
                    previousPoint = point;
                }
                break;
            }

            case PrimitiveType::POLYLINE: {
                if (prim.points.size() < 2) {
                    break;
                }

                if (!prim.pointColors.empty() && prim.pointColors.size() >= prim.points.size()) {
                    AddPolylineTube(renderData, prim.points.data(), (uint32_t)prim.points.size(), ResolvePolylineRadius(prim.thickness), prim.pointColors.data(), prim.xray);
                }
                else {
                    AddPolylineTube(renderData, prim.points.data(), (uint32_t)prim.points.size(), ResolvePolylineRadius(prim.thickness), prim.color, prim.xray);
                }
                break;
            }

            case PrimitiveType::ARC: {
                const int segments = ResolveArcSegments(prim.duration, prim.segments);
                if (segments < 2 || prim.duration <= 0.0f) {
                    break;
                }

                auto evaluatePoint = [&](float t) {
                    return prim.a + prim.b * t + prim.c * (0.5f * t * t);
                };

                std::array<glm::vec3, 97> arcPoints = {};
                arcPoints[0] = evaluatePoint(0.0f);
                for (int i = 1; i <= segments; ++i) {
                    const float t = (prim.duration * (float)i) / (float)segments;
                    arcPoints[i] = evaluatePoint(t);
                }
                AddPolylineTube(renderData, arcPoints.data(), (uint32_t)segments + 1, ResolveArcWidth(prim.duration), prim.color, prim.xray);
                break;
            }

            case PrimitiveType::AABB: {
                const glm::vec3& mn = prim.a;
                const glm::vec3& mx = prim.b;
                glm::vec3 corners[8] = {
                    { mn.x, mn.y, mn.z },
                    { mx.x, mn.y, mn.z },
                    { mx.x, mn.y, mx.z },
                    { mn.x, mn.y, mx.z },
                    { mn.x, mx.y, mn.z },
                    { mx.x, mx.y, mn.z },
                    { mx.x, mx.y, mx.z },
                    { mn.x, mx.y, mx.z },
                };
                AddSolidBox(renderData, corners, prim.color, prim.xray);
                AddEdges(renderData, corners, BOX_EDGES, 12, prim.color, prim.xray);
                break;
            }

            case PrimitiveType::ORIENTED_BOX: {
                const glm::vec3& center = prim.a;
                const glm::vec3& half = prim.b;
                const glm::mat3 rot = glm::mat3_cast(prim.rotation);
                const glm::vec3 localCorners[8] = {
                    { -half.x, -half.y, -half.z },
                    { +half.x, -half.y, -half.z },
                    { +half.x, -half.y, +half.z },
                    { -half.x, -half.y, +half.z },
                    { -half.x, +half.y, -half.z },
                    { +half.x, +half.y, -half.z },
                    { +half.x, +half.y, +half.z },
                    { -half.x, +half.y, +half.z },
                };

                glm::vec3 corners[8];
                for (int i = 0; i < 8; ++i) {
                    corners[i] = center + rot * localCorners[i];
                }

                AddSolidBox(renderData, corners, prim.color, prim.xray);
                AddEdges(renderData, corners, BOX_EDGES, 12, prim.color, prim.xray);
                break;
            }

            case PrimitiveType::FRUSTUM: {
                static constexpr glm::vec4 ndcCorners[8] = {
                    { -1, -1, -1, 1 },
                    { +1, -1, -1, 1 },
                    { +1, +1, -1, 1 },
                    { -1, +1, -1, 1 },
                    { -1, -1, +1, 1 },
                    { +1, -1, +1, 1 },
                    { +1, +1, +1, 1 },
                    { -1, +1, +1, 1 },
                };

                glm::vec3 corners[8];
                for (int i = 0; i < 8; ++i) {
                    const glm::vec4 world = prim.inverseVP * ndcCorners[i];
                    corners[i] = glm::vec3(world) / world.w;
                }

                AddEdges(renderData, corners, BOX_EDGES, 12, prim.color, prim.xray);
                break;
            }
        }
    }

    return renderData;
}

void DebugDraw::Clear() {
    std::lock_guard lk(m_mutex);
    m_primitives.clear();
}
