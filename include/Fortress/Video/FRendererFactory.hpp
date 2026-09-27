#ifndef FORTRESS_VIDEO_FRENDERERFACTORY_HPP
#define FORTRESS_VIDEO_FRENDERERFACTORY_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Video {

class FDisplayDevice;
class FRenderer3D;
class FSoftwareRenderer3D;

enum class ERendererBackend : Fortress::Core::uint8 {
    Software = 0,
  Null = 1,
};

class FRendererFactory {
  public:
    static ERendererBackend SelectDefaultBackend();
    static const char *GetBackendName(ERendererBackend backend);
    static bool CreateAndInitializeRenderer(ERendererBackend backend,
                                            FDisplayDevice *displayDevice,
                                            FSoftwareRenderer3D &softwareRenderer,
                                            FRenderer3D *&outRenderer);
};

} // namespace Fortress::Video

#endif