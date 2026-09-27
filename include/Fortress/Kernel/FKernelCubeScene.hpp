#ifndef FORTRESS_KERNEL_FKERNELCUBESCENE_HPP
#define FORTRESS_KERNEL_FKERNELCUBESCENE_HPP

#include "Fortress/Core/FTypes.hpp"
#include "Fortress/Video/FRenderer3D.hpp"
#include "Fortress/Video/FVideoSurface.hpp"

namespace Fortress::Kernel {

class FKernelCubeScene {
  public:
    bool Initialize(Fortress::Video::FRenderer3D *inRenderer,
                    Fortress::Core::usize viewportWidth,
                    Fortress::Core::usize viewportHeight);
    bool UpdateViewport(Fortress::Core::usize viewportWidth, Fortress::Core::usize viewportHeight);

    void Advance(float deltaTime, bool paused);
    void Render(const Fortress::Video::FVideoSurfaceView &surface, bool wireframeEnabled);

  private:
    Fortress::Video::FRenderer3D *Renderer = nullptr;
    Fortress::Video::FMatrix4 Projection = Fortress::Video::FMatrix4::Identity();

    float SinY = 0.0f;
    float CosY = 1.0f;
    float SinX = 0.0f;
    float CosX = 1.0f;

    float FixedStep = 1.0f / 60.0f;
    float Accumulator = 0.0f;
};

} // namespace Fortress::Kernel

#endif
