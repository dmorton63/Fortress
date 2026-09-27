#include "Fortress/Video/FSoftwareRenderer3D.hpp"

#include "Fortress/Memory/FMemoryArena.hpp"

namespace Fortress::Video {

static float FAbs(float v) {
    return (v < 0.0f) ? -v : v;
}

static float FSqrt(float v) {
    if (v <= 0.0f) {
        return 0.0f;
    }

    float x = v;
    for (int i = 0; i < 8; i++) {
        x = 0.5f * (x + v / x);
    }
    return x;
}

static float FMin(float a, float b) {
    return (a < b) ? a : b;
}

static float FMax(float a, float b) {
    return (a > b) ? a : b;
}

static Fortress::Core::int32 FloorToInt(float v) {
    Fortress::Core::int32 i = static_cast<Fortress::Core::int32>(v);
    return (v < static_cast<float>(i)) ? (i - 1) : i;
}

static Fortress::Core::int32 CeilToInt(float v) {
    Fortress::Core::int32 i = static_cast<Fortress::Core::int32>(v);
    return (v > static_cast<float>(i)) ? (i + 1) : i;
}

static float Edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static FColor ScaleColor(FColor c, float intensity) {
    auto Scale = [intensity](Fortress::Core::uint8 channel) -> Fortress::Core::uint8 {
        float value = static_cast<float>(channel) * intensity;
        if (value < 0.0f) {
            value = 0.0f;
        }
        if (value > 255.0f) {
            value = 255.0f;
        }
        return static_cast<Fortress::Core::uint8>(value);
    };

    return FColor{.R = Scale(c.R), .G = Scale(c.G), .B = Scale(c.B), .A = c.A};
}

static FVector3 Lerp(const FVector3 &a, const FVector3 &b, float t) {
    return FVector3{
        .X = a.X + (b.X - a.X) * t,
        .Y = a.Y + (b.Y - a.Y) * t,
        .Z = a.Z + (b.Z - a.Z) * t,
    };
}

FVector3 FVector3::Subtract(const FVector3 &a, const FVector3 &b) {
    return FVector3{.X = a.X - b.X, .Y = a.Y - b.Y, .Z = a.Z - b.Z};
}

float FVector3::Dot(const FVector3 &a, const FVector3 &b) {
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

FVector3 FVector3::Cross(const FVector3 &a, const FVector3 &b) {
    return FVector3{
        .X = a.Y * b.Z - a.Z * b.Y,
        .Y = a.Z * b.X - a.X * b.Z,
        .Z = a.X * b.Y - a.Y * b.X,
    };
}

FVector3 FVector3::Normalize(const FVector3 &v) {
    const float length = FSqrt(v.X * v.X + v.Y * v.Y + v.Z * v.Z);
    if (length < 0.00001f) {
        return FVector3{.X = 0.0f, .Y = 0.0f, .Z = 1.0f};
    }

    const float inv = 1.0f / length;
    return FVector3{.X = v.X * inv, .Y = v.Y * inv, .Z = v.Z * inv};
}

FMatrix4 FMatrix4::Identity() {
    FMatrix4 out{};
    out.M[0][0] = 1.0f;
    out.M[1][1] = 1.0f;
    out.M[2][2] = 1.0f;
    out.M[3][3] = 1.0f;
    return out;
}

FMatrix4 FMatrix4::Perspective(float fovRadians, float aspectRatio, float nearPlane, float farPlane) {
    FMatrix4 out{};
    const float halfFov = fovRadians * 0.5f;
    const float halfFov2 = halfFov * halfFov;
    const float halfFov4 = halfFov2 * halfFov2;
    const float halfFov6 = halfFov4 * halfFov2;
    const float tanHalfFov = halfFov + (halfFov * halfFov2) / 3.0f + (2.0f * halfFov6) / 15.0f;
    const float f = 1.0f / tanHalfFov;

    out.M[0][0] = f / aspectRatio;
    out.M[1][1] = f;
    out.M[2][2] = farPlane / (farPlane - nearPlane);
    out.M[2][3] = (-farPlane * nearPlane) / (farPlane - nearPlane);
    out.M[3][2] = 1.0f;
    return out;
}

FMatrix4 FMatrix4::RotationX(float sinTheta, float cosTheta) {
    FMatrix4 out = Identity();
    out.M[1][1] = cosTheta;
    out.M[1][2] = -sinTheta;
    out.M[2][1] = sinTheta;
    out.M[2][2] = cosTheta;
    return out;
}

FMatrix4 FMatrix4::RotationY(float sinTheta, float cosTheta) {
    FMatrix4 out = Identity();
    out.M[0][0] = cosTheta;
    out.M[0][2] = sinTheta;
    out.M[2][0] = -sinTheta;
    out.M[2][2] = cosTheta;
    return out;
}

FMatrix4 FMatrix4::Translation(float tx, float ty, float tz) {
    FMatrix4 out = Identity();
    out.M[0][3] = tx;
    out.M[1][3] = ty;
    out.M[2][3] = tz;
    return out;
}

FMatrix4 FMatrix4::Multiply(const FMatrix4 &a, const FMatrix4 &b) {
    FMatrix4 out{};
    for (Fortress::Core::usize r = 0; r < 4; r++) {
        for (Fortress::Core::usize c = 0; c < 4; c++) {
            out.M[r][c] =
                a.M[r][0] * b.M[0][c] +
                a.M[r][1] * b.M[1][c] +
                a.M[r][2] * b.M[2][c] +
                a.M[r][3] * b.M[3][c];
        }
    }
    return out;
}

bool FSoftwareRenderer3D::Initialize(FDisplayDevice *inDevice) {
    if (inDevice == nullptr) {
        return false;
    }

    Device = inDevice;
    const FDisplayMode mode = Device->GetCurrentMode();
    if (mode.Width == 0 || mode.Height == 0 || mode.PixelFormat != EPixelFormat::Masked32) {
        return false;
    }
    if (!UpdateViewport(mode.Width, mode.Height)) {
        return false;
    }

    ScreenVertices = nullptr;
    WorldVertices = nullptr;
    ScreenVertexCapacity = 0;
    NearPlane = 0.1f;
    WireframeOverlay = false;
    WireframeColor = FColor::RGB(255, 255, 255);

    return true;
}

bool FSoftwareRenderer3D::UpdateViewport(Fortress::Core::usize viewportWidth, Fortress::Core::usize viewportHeight) {
    if (viewportWidth == 0 || viewportHeight == 0) {
        return false;
    }

    if (viewportWidth == Width && viewportHeight == Height && DepthBuffer != nullptr) {
        return true;
    }

    float *newDepthBuffer = static_cast<float *>(
        Fortress::Memory::FMemoryArena::Allocate(viewportWidth * viewportHeight * sizeof(float), alignof(float)));
    if (newDepthBuffer == nullptr) {
        return false;
    }

    Width = viewportWidth;
    Height = viewportHeight;
    DepthBuffer = newDepthBuffer;
    return true;
}

void FSoftwareRenderer3D::SetNearPlane(float inNearPlane) {
    if (inNearPlane > 0.001f) {
        NearPlane = inNearPlane;
    }
}

void FSoftwareRenderer3D::SetWireframeOverlay(bool enabled, FColor color) {
    WireframeOverlay = enabled;
    WireframeColor = color;
}

FSoftwareRenderer3D::FScreenVertex FSoftwareRenderer3D::ProjectToScreen(const FVector3 &v, const FMatrix4 &projection) const {
    const FVector4 clip = Transform(v, projection);
    if (clip.W <= 0.001f) {
        return FScreenVertex{.X = 0.0f, .Y = 0.0f, .Z = 1.0f, .Valid = false};
    }

    const float invW = 1.0f / clip.W;
    const float ndcX = clip.X * invW;
    const float ndcY = clip.Y * invW;
    const float ndcZ = clip.Z * invW;

    return FScreenVertex{
        .X = (ndcX * 0.5f + 0.5f) * static_cast<float>(Width - 1),
        .Y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(Height - 1),
        .Z = ndcZ,
        .Valid = true,
    };
}

Fortress::Core::usize FSoftwareRenderer3D::ClipTriangleNearPlane(const FVector3 &a,
                                                                 const FVector3 &b,
                                                                 const FVector3 &c,
                                                                 float nearPlane,
                                                                 FClipVertex outVertices[4]) const {
    FVector3 input[3] = {a, b, c};
    FVector3 output[4] = {};
    Fortress::Core::usize outCount = 0;

    for (Fortress::Core::usize i = 0; i < 3; i++) {
        const FVector3 current = input[i];
        const FVector3 next = input[(i + 1) % 3];

        const bool currentInside = current.Z >= nearPlane;
        const bool nextInside = next.Z >= nearPlane;

        if (currentInside && nextInside) {
            output[outCount++] = next;
        } else if (currentInside && !nextInside) {
            const float denom = (next.Z - current.Z);
            if (FAbs(denom) > 0.000001f) {
                const float t = (nearPlane - current.Z) / denom;
                output[outCount++] = Lerp(current, next, t);
            }
        } else if (!currentInside && nextInside) {
            const float denom = (next.Z - current.Z);
            if (FAbs(denom) > 0.000001f) {
                const float t = (nearPlane - current.Z) / denom;
                output[outCount++] = Lerp(current, next, t);
            }
            output[outCount++] = next;
        }
    }

    if (outCount > 4) {
        outCount = 4;
    }

    for (Fortress::Core::usize i = 0; i < outCount; i++) {
        outVertices[i].Position = output[i];
    }

    return outCount;
}

void FSoftwareRenderer3D::DrawWireTriangle(const FVideoSurfaceView &surface,
                                           const FScreenVertex &a,
                                           const FScreenVertex &b,
                                           const FScreenVertex &c,
                                           FColor color) {
    if (!surface.IsValid() || !a.Valid || !b.Valid || !c.Valid) {
        return;
    }

    const Fortress::Core::uint32 packed = FVideoSurfaceOps::PackMasked32(color, surface.Desc.PixelMask);
    FVideoSurfaceOps::DrawLine32(surface,
                                 static_cast<Fortress::Core::int32>(a.X),
                                 static_cast<Fortress::Core::int32>(a.Y),
                                 static_cast<Fortress::Core::int32>(b.X),
                                 static_cast<Fortress::Core::int32>(b.Y),
                                 packed);
    FVideoSurfaceOps::DrawLine32(surface,
                                 static_cast<Fortress::Core::int32>(b.X),
                                 static_cast<Fortress::Core::int32>(b.Y),
                                 static_cast<Fortress::Core::int32>(c.X),
                                 static_cast<Fortress::Core::int32>(c.Y),
                                 packed);
    FVideoSurfaceOps::DrawLine32(surface,
                                 static_cast<Fortress::Core::int32>(c.X),
                                 static_cast<Fortress::Core::int32>(c.Y),
                                 static_cast<Fortress::Core::int32>(a.X),
                                 static_cast<Fortress::Core::int32>(a.Y),
                                 packed);
}

void FSoftwareRenderer3D::DrawMeshLit(const FVector3 *vertices,
                                      Fortress::Core::usize vertexCount,
                                      const Fortress::Core::uint16 *indices,
                                      Fortress::Core::usize indexCount,
                                      const FMatrix4 &model,
                                      const FMatrix4 &projection,
                                      FVector3 lightDirection,
                                      FColor baseColor) {
    if (Device == nullptr) {
        return;
    }

    DrawMeshLit(Device->GetBackSurface(),
                vertices,
                vertexCount,
                indices,
                indexCount,
                model,
                projection,
                lightDirection,
                baseColor);
}

void FSoftwareRenderer3D::DrawMeshLit(const FVideoSurfaceView &surface,
                                      const FVector3 *vertices,
                                      Fortress::Core::usize vertexCount,
                                      const Fortress::Core::uint16 *indices,
                                      Fortress::Core::usize indexCount,
                                      const FMatrix4 &model,
                                      const FMatrix4 &projection,
                                      FVector3 lightDirection,
                                      FColor baseColor) {
    if (vertices == nullptr || indices == nullptr || vertexCount == 0 || indexCount < 3) {
        return;
    }

    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32) {
        return;
    }

    if (ScreenVertexCapacity < vertexCount) {
        auto *newScreen = static_cast<FScreenVertex *>(
            Fortress::Memory::FMemoryArena::Allocate(vertexCount * sizeof(FScreenVertex), alignof(FScreenVertex)));
        auto *newWorld = static_cast<FVector3 *>(
            Fortress::Memory::FMemoryArena::Allocate(vertexCount * sizeof(FVector3), alignof(FVector3)));
        if (newScreen == nullptr || newWorld == nullptr) {
            return;
        }
        ScreenVertices = newScreen;
        WorldVertices = newWorld;
        ScreenVertexCapacity = vertexCount;
    }

    lightDirection = FVector3::Normalize(lightDirection);

    for (Fortress::Core::usize i = 0; i < vertexCount; i++) {
        const FVector4 world = Transform(vertices[i], model);
        WorldVertices[i] = FVector3{.X = world.X, .Y = world.Y, .Z = world.Z};
    }

    for (Fortress::Core::usize i = 0; i + 2 < indexCount; i += 3) {
        const Fortress::Core::uint16 i0 = indices[i + 0];
        const Fortress::Core::uint16 i1 = indices[i + 1];
        const Fortress::Core::uint16 i2 = indices[i + 2];

        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
            continue;
        }

        const FVector3 v0 = WorldVertices[i0];
        const FVector3 v1 = WorldVertices[i1];
        const FVector3 v2 = WorldVertices[i2];

        const FVector3 e0 = FVector3::Subtract(v1, v0);
        const FVector3 e1 = FVector3::Subtract(v2, v0);
        const FVector3 normal = FVector3::Normalize(FVector3::Cross(e0, e1));

        float light = FVector3::Dot(normal, lightDirection);
        if (light < 0.15f) {
            light = 0.15f;
        }

        const FColor shaded = ScaleColor(baseColor, light);

        FClipVertex clipped[4] = {};
        const Fortress::Core::usize clippedCount = ClipTriangleNearPlane(v0, v1, v2, NearPlane, clipped);
        if (clippedCount < 3) {
            continue;
        }

        FScreenVertex projected[4] = {};
        for (Fortress::Core::usize v = 0; v < clippedCount; v++) {
            projected[v] = ProjectToScreen(clipped[v].Position, projection);
        }

        RasterizeTriangle(surface, projected[0], projected[1], projected[2], shaded);
        if (WireframeOverlay) {
            DrawWireTriangle(surface, projected[0], projected[1], projected[2], WireframeColor);
        }

        if (clippedCount == 4) {
            RasterizeTriangle(surface, projected[0], projected[2], projected[3], shaded);
            if (WireframeOverlay) {
                DrawWireTriangle(surface, projected[0], projected[2], projected[3], WireframeColor);
            }
        }
    }
}

void FSoftwareRenderer3D::ClearDepth(float depthValue) {
    if (DepthBuffer == nullptr) {
        return;
    }

    for (Fortress::Core::usize i = 0; i < Width * Height; i++) {
        DepthBuffer[i] = depthValue;
    }
}

FVector4 FSoftwareRenderer3D::Transform(const FVector3 &v, const FMatrix4 &m) const {
    FVector4 out{};
    out.X = m.M[0][0] * v.X + m.M[0][1] * v.Y + m.M[0][2] * v.Z + m.M[0][3];
    out.Y = m.M[1][0] * v.X + m.M[1][1] * v.Y + m.M[1][2] * v.Z + m.M[1][3];
    out.Z = m.M[2][0] * v.X + m.M[2][1] * v.Y + m.M[2][2] * v.Z + m.M[2][3];
    out.W = m.M[3][0] * v.X + m.M[3][1] * v.Y + m.M[3][2] * v.Z + m.M[3][3];
    return out;
}

void FSoftwareRenderer3D::RasterizeTriangle(const FVideoSurfaceView &surface,
                                            const FScreenVertex &a,
                                            const FScreenVertex &b,
                                            const FScreenVertex &c,
                                            FColor color) {
    if (!surface.IsValid() || !a.Valid || !b.Valid || !c.Valid) {
        return;
    }

    const float area = Edge(a.X, a.Y, b.X, b.Y, c.X, c.Y);
    if (FAbs(area) < 0.0001f) {
        return;
    }

    if (area <= 0.0f) {
        return;
    }

    Fortress::Core::int32 minX = FloorToInt(FMax(0.0f, FMin(a.X, FMin(b.X, c.X))));
    Fortress::Core::int32 minY = FloorToInt(FMax(0.0f, FMin(a.Y, FMin(b.Y, c.Y))));
    Fortress::Core::int32 maxX = CeilToInt(FMin(static_cast<float>(Width - 1), FMax(a.X, FMax(b.X, c.X))));
    Fortress::Core::int32 maxY = CeilToInt(FMin(static_cast<float>(Height - 1), FMax(a.Y, FMax(b.Y, c.Y))));

    const Fortress::Core::uint32 packed = FVideoSurfaceOps::PackMasked32(color, surface.Desc.PixelMask);

    for (Fortress::Core::int32 y = minY; y <= maxY; y++) {
        for (Fortress::Core::int32 x = minX; x <= maxX; x++) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;

            float w0 = Edge(b.X, b.Y, c.X, c.Y, px, py);
            float w1 = Edge(c.X, c.Y, a.X, a.Y, px, py);
            float w2 = Edge(a.X, a.Y, b.X, b.Y, px, py);

            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                continue;
            }

            w0 /= area;
            w1 /= area;
            w2 /= area;

            const float depth = w0 * a.Z + w1 * b.Z + w2 * c.Z;
            const Fortress::Core::usize idx = static_cast<Fortress::Core::usize>(y) * Width + static_cast<Fortress::Core::usize>(x);
            if (depth < 0.0f || depth > 1.0f || depth >= DepthBuffer[idx]) {
                continue;
            }

            DepthBuffer[idx] = depth;
            FVideoSurfaceOps::DrawPixel32(surface, x, y, packed);
        }
    }
}

void FSoftwareRenderer3D::DrawMesh(const FVector3 *vertices,
                                   Fortress::Core::usize vertexCount,
                                   const Fortress::Core::uint16 *indices,
                                   Fortress::Core::usize indexCount,
                                   const FMatrix4 &mvp,
                                   FColor color) {
    if (Device == nullptr) {
        return;
    }

    DrawMesh(Device->GetBackSurface(), vertices, vertexCount, indices, indexCount, mvp, color);
}

void FSoftwareRenderer3D::DrawMesh(const FVideoSurfaceView &surface,
                                   const FVector3 *vertices,
                                   Fortress::Core::usize vertexCount,
                                   const Fortress::Core::uint16 *indices,
                                   Fortress::Core::usize indexCount,
                                   const FMatrix4 &mvp,
                                   FColor color) {
    if (vertices == nullptr || indices == nullptr || vertexCount == 0 || indexCount < 3) {
        return;
    }

    if (!surface.IsValid() || surface.Desc.PixelFormat != EPixelFormat::Masked32) {
        return;
    }

    if (ScreenVertexCapacity < vertexCount) {
        auto *newBuffer = static_cast<FScreenVertex *>(
            Fortress::Memory::FMemoryArena::Allocate(vertexCount * sizeof(FScreenVertex), alignof(FScreenVertex)));
        if (newBuffer == nullptr) {
            return;
        }
        ScreenVertices = newBuffer;
        ScreenVertexCapacity = vertexCount;
    }

    for (Fortress::Core::usize i = 0; i < vertexCount; i++) {
        const FVector4 clip = Transform(vertices[i], mvp);
        if (clip.W <= 0.001f) {
            ScreenVertices[i] = FScreenVertex{.X = 0, .Y = 0, .Z = 1.0f, .Valid = false};
            continue;
        }

        const float invW = 1.0f / clip.W;
        const float ndcX = clip.X * invW;
        const float ndcY = clip.Y * invW;
        const float ndcZ = clip.Z * invW;

        ScreenVertices[i] = FScreenVertex{
            .X = (ndcX * 0.5f + 0.5f) * static_cast<float>(Width - 1),
            .Y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(Height - 1),
            .Z = ndcZ,
            .Valid = true,
        };
    }

    for (Fortress::Core::usize i = 0; i + 2 < indexCount; i += 3) {
        const Fortress::Core::uint16 i0 = indices[i + 0];
        const Fortress::Core::uint16 i1 = indices[i + 1];
        const Fortress::Core::uint16 i2 = indices[i + 2];

        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
            continue;
        }

        RasterizeTriangle(surface, ScreenVertices[i0], ScreenVertices[i1], ScreenVertices[i2], color);
    }
}

} // namespace Fortress::Video
