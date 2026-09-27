#ifndef FORTRESS_VIDEO_FRENDERER3D_HPP
#define FORTRESS_VIDEO_FRENDERER3D_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Video/FColor.hpp"
#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Video {

struct FVector3 {
    float X;
    float Y;
    float Z;

    static FVector3 Normalize(const FVector3 &v);
    static float Dot(const FVector3 &a, const FVector3 &b);
    static FVector3 Cross(const FVector3 &a, const FVector3 &b);
    static FVector3 Subtract(const FVector3 &a, const FVector3 &b);
};

struct FVector4 {
    float X;
    float Y;
    float Z;
    float W;
};

struct FMatrix4 {
    float M[4][4];

    static FMatrix4 Identity();
    static FMatrix4 Perspective(float fovRadians, float aspectRatio, float nearPlane, float farPlane);
    static FMatrix4 RotationX(float sinTheta, float cosTheta);
    static FMatrix4 RotationY(float sinTheta, float cosTheta);
    static FMatrix4 Translation(float tx, float ty, float tz);
    static FMatrix4 Multiply(const FMatrix4 &a, const FMatrix4 &b);
};

class FRenderer3D {
  public:
    virtual bool UpdateViewport(Fortress::Core::usize viewportWidth, Fortress::Core::usize viewportHeight) = 0;
    virtual void SetNearPlane(float inNearPlane) = 0;
    virtual void SetWireframeOverlay(bool enabled, FColor color) = 0;

    virtual void ClearDepth(float depthValue) = 0;
    virtual void DrawMesh(const FVector3 *vertices,
                          Fortress::Core::usize vertexCount,
                          const Fortress::Core::uint16 *indices,
                          Fortress::Core::usize indexCount,
                          const FMatrix4 &mvp,
                          FColor color) = 0;
    virtual void DrawMesh(const FVideoSurfaceView &surface,
                          const FVector3 *vertices,
                          Fortress::Core::usize vertexCount,
                          const Fortress::Core::uint16 *indices,
                          Fortress::Core::usize indexCount,
                          const FMatrix4 &mvp,
                          FColor color) = 0;

    virtual void DrawMeshLit(const FVector3 *vertices,
                             Fortress::Core::usize vertexCount,
                             const Fortress::Core::uint16 *indices,
                             Fortress::Core::usize indexCount,
                             const FMatrix4 &model,
                             const FMatrix4 &projection,
                             FVector3 lightDirection,
                             FColor baseColor) = 0;
    virtual void DrawMeshLit(const FVideoSurfaceView &surface,
                             const FVector3 *vertices,
                             Fortress::Core::usize vertexCount,
                             const Fortress::Core::uint16 *indices,
                             Fortress::Core::usize indexCount,
                             const FMatrix4 &model,
                             const FMatrix4 &projection,
                             FVector3 lightDirection,
                             FColor baseColor) = 0;
};

} // namespace Fortress::Video

#endif