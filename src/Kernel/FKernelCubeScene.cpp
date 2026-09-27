#include "Fortress/Kernel/FKernelCubeScene.hpp"

#include "Fortress/Kernel/FKernelConfig.hpp"

namespace Fortress::Kernel {

using Fortress::Video::FColor;
using Fortress::Video::FMatrix4;
using Fortress::Video::FVector3;

static const FVector3 GCubeVertices[] = {
    {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f},
};

static const Fortress::Core::uint16 GCubeIndices[] = {
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    0, 4, 5, 0, 5, 1,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    0, 3, 7, 0, 7, 4,
};

bool FKernelCubeScene::Initialize(Fortress::Video::FRenderer3D *inRenderer,
                                  Fortress::Core::usize viewportWidth,
                                  Fortress::Core::usize viewportHeight) {
    if (inRenderer == nullptr || viewportWidth == 0 || viewportHeight == 0) {
        return false;
    }

    Renderer = inRenderer;
    if (!UpdateViewport(viewportWidth, viewportHeight)) {
        return false;
    }

    FixedStep = FKernelConfig::SceneFixedStepSeconds;
    Accumulator = 0.0f;

    SinY = 0.0f;
    CosY = 1.0f;
    SinX = 0.0f;
    CosX = 1.0f;

    return true;
}

bool FKernelCubeScene::UpdateViewport(Fortress::Core::usize viewportWidth, Fortress::Core::usize viewportHeight) {
    if (Renderer == nullptr || viewportWidth == 0 || viewportHeight == 0) {
        return false;
    }

    if (!Renderer->UpdateViewport(viewportWidth, viewportHeight)) {
        return false;
    }

    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    Projection = FMatrix4::Perspective(FKernelConfig::SceneFovRadians,
                                       aspect,
                                       FKernelConfig::SceneNearPlane,
                                       FKernelConfig::SceneFarPlane);
    Renderer->SetNearPlane(FKernelConfig::SceneNearPlane);
    return true;
}

void FKernelCubeScene::Advance(float deltaTime, bool paused) {
    if (Renderer == nullptr) {
        return;
    }

    Accumulator += deltaTime;
    if (Accumulator > 0.25f) {
        Accumulator = 0.25f;
    }

    while (Accumulator >= FixedStep) {
        if (!paused) {
            const float stepY = FixedStep * FKernelConfig::SceneYawRate;
            const float stepX = FixedStep * FKernelConfig::ScenePitchRate;

            const float sinStepY = stepY;
            const float cosStepY = 1.0f - (stepY * stepY * 0.5f);
            const float sinStepX = stepX;
            const float cosStepX = 1.0f - (stepX * stepX * 0.5f);

            const float newSinY = SinY * cosStepY + CosY * sinStepY;
            const float newCosY = CosY * cosStepY - SinY * sinStepY;
            SinY = newSinY;
            CosY = newCosY;

            const float newSinX = SinX * cosStepX + CosX * sinStepX;
            const float newCosX = CosX * cosStepX - SinX * sinStepX;
            SinX = newSinX;
            CosX = newCosX;
        }
        Accumulator -= FixedStep;
    }
}

void FKernelCubeScene::Render(const Fortress::Video::FVideoSurfaceView &surface, bool wireframeEnabled) {
    if (Renderer == nullptr) {
        return;
    }

    Renderer->ClearDepth(1.0f);
    Renderer->SetWireframeOverlay(wireframeEnabled, FColor::RGB(255, 240, 120));

    const FMatrix4 rotationY = FMatrix4::RotationY(SinY, CosY);
    const FMatrix4 rotationX = FMatrix4::RotationX(SinX, CosX);
    const FMatrix4 model = FMatrix4::Multiply(
        FMatrix4::Translation(0.0f, 0.0f, FKernelConfig::SceneDistance),
        FMatrix4::Multiply(rotationY, rotationX));

    Renderer->DrawMeshLit(surface,
                          GCubeVertices,
                          sizeof(GCubeVertices) / sizeof(GCubeVertices[0]),
                          GCubeIndices,
                          sizeof(GCubeIndices) / sizeof(GCubeIndices[0]),
                          model,
                          Projection,
                          FVector3{.X = 0.5f, .Y = 0.7f, .Z = -1.0f},
                          FColor::RGB(41, 198, 255));
}

} // namespace Fortress::Kernel
