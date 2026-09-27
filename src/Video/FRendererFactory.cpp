#include "Fortress/Video/FRendererFactory.hpp"

#include "Fortress/Video/FDisplayDevice.hpp"
#include "Fortress/Video/FRenderer3D.hpp"
#include "Fortress/Video/FSoftwareRenderer3D.hpp"

namespace Fortress::Video {

ERendererBackend FRendererFactory::SelectDefaultBackend() {
#if defined(FORTRESS_RENDERER_BACKEND_NULL)
    return ERendererBackend::Null;
#elif defined(FORTRESS_RENDERER_BACKEND_SOFTWARE)
    return ERendererBackend::Software;
#else
    return ERendererBackend::Software;
#endif
}

const char *FRendererFactory::GetBackendName(ERendererBackend backend) {
    switch (backend) {
    case ERendererBackend::Software:
        return "SOFTWARE";
    case ERendererBackend::Null:
        return "NULL";
    default:
        return "UNKNOWN";
    }
}

bool FRendererFactory::CreateAndInitializeRenderer(ERendererBackend backend,
                                                   FDisplayDevice *displayDevice,
                                                   FSoftwareRenderer3D &softwareRenderer,
                                                   FRenderer3D *&outRenderer) {
    outRenderer = nullptr;
    if (displayDevice == nullptr) {
        return false;
    }

    switch (backend) {
    case ERendererBackend::Software:
        if (!softwareRenderer.Initialize(displayDevice)) {
            return false;
        }
        outRenderer = &softwareRenderer;
        return true;
    case ERendererBackend::Null:
        // Placeholder backend selection path. No renderer is available yet.
        return false;
    default:
        return false;
    }
}

} // namespace Fortress::Video