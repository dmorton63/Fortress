#ifndef FORTRESS_VIDEO_FSOFTWARERENDERER3D_HPP
#define FORTRESS_VIDEO_FSOFTWARERENDERER3D_HPP

#include "Fortress/Video/FDisplayDevice.hpp"
#include "Fortress/Video/FRenderer3D.hpp"

namespace Fortress::Video {

class FSoftwareRenderer3D : public FRenderer3D {
  public:
    bool Initialize(FDisplayDevice *inDevice);
    bool UpdateViewport(Fortress::Core::usize viewportWidth, Fortress::Core::usize viewportHeight) override;
    void SetNearPlane(float inNearPlane) override;
    void SetWireframeOverlay(bool enabled, FColor color) override;

    void ClearDepth(float depthValue) override;
    void DrawMesh(const FVector3 *vertices,
                  Fortress::Core::usize vertexCount,
                  const Fortress::Core::uint16 *indices,
                  Fortress::Core::usize indexCount,
                  const FMatrix4 &mvp,
                  FColor color) override;
    void DrawMesh(const FVideoSurfaceView &surface,
                  const FVector3 *vertices,
                  Fortress::Core::usize vertexCount,
                  const Fortress::Core::uint16 *indices,
                  Fortress::Core::usize indexCount,
                  const FMatrix4 &mvp,
                  FColor color) override;

    void DrawMeshLit(const FVector3 *vertices,
                     Fortress::Core::usize vertexCount,
                     const Fortress::Core::uint16 *indices,
                     Fortress::Core::usize indexCount,
                     const FMatrix4 &model,
                     const FMatrix4 &projection,
                     FVector3 lightDirection,
                     FColor baseColor) override;
    void DrawMeshLit(const FVideoSurfaceView &surface,
                     const FVector3 *vertices,
                     Fortress::Core::usize vertexCount,
                     const Fortress::Core::uint16 *indices,
                     Fortress::Core::usize indexCount,
                     const FMatrix4 &model,
                     const FMatrix4 &projection,
                     FVector3 lightDirection,
                     FColor baseColor) override;

  private:
    struct FClipVertex {
        FVector3 Position;
    };

    struct FScreenVertex {
        float X;
        float Y;
        float Z;
        bool Valid;
    };

    FVector4 Transform(const FVector3 &v, const FMatrix4 &m) const;
    FScreenVertex ProjectToScreen(const FVector3 &v, const FMatrix4 &projection) const;
    Fortress::Core::usize ClipTriangleNearPlane(const FVector3 &a,
                                                const FVector3 &b,
                                                const FVector3 &c,
                                                float nearPlane,
                                                FClipVertex outVertices[4]) const;
    void RasterizeTriangle(const FVideoSurfaceView &surface,
                           const FScreenVertex &a,
                           const FScreenVertex &b,
                           const FScreenVertex &c,
                           FColor color);
    void DrawWireTriangle(const FVideoSurfaceView &surface,
                          const FScreenVertex &a,
                          const FScreenVertex &b,
                          const FScreenVertex &c,
                          FColor color);

    FDisplayDevice *Device = nullptr;
    float *DepthBuffer = nullptr;
    FScreenVertex *ScreenVertices = nullptr;
    FVector3 *WorldVertices = nullptr;
    Fortress::Core::usize ScreenVertexCapacity = 0;
    Fortress::Core::usize Width = 0;
    Fortress::Core::usize Height = 0;
    float NearPlane = 0.1f;
    bool WireframeOverlay = false;
    FColor WireframeColor = FColor::RGB(255, 255, 255);
};

} // namespace Fortress::Video

#endif
