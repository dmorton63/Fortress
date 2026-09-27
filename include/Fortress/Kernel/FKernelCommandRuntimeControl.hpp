#pragma once

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FKernelRuntimeControlCommandContext {
    const char *CommandBuffer;
    bool *WireframeEnabled;
    bool *ScenePaused;
    void (*PublishWireframeSetEvent)(bool enabled);
    void (*PublishScenePauseSetEvent)(bool paused);
    void (*PushLog)(const char *line);
    bool (*IsParallelWorkersSupported)();
    bool (*IsParallelWorkersEnabled)();
    bool (*SetParallelWorkersEnabled)(bool enabled);
    bool (*IsParallelDrainEnabled)();
    bool (*AreParallelDispatchCallbacksEnabled)();
    bool (*SetParallelDrainEnabled)(bool enabled);
    bool (*StartsWithFn)(const char *value, const char *prefix);
    bool (*ParseUIntFn)(const char *value, Fortress::Core::uint64 &out);
    void (*RunParallelTestFn)(Fortress::Core::uint32 workItems);
    void (*RunParallelCanaryFn)(Fortress::Core::uint32 workItems);
    void (*RunParallelProbeFn)();
    void (*RequestPlatformShutdown)();
    void (*HaltCpuForever)();
};

bool TryProcessRuntimeControlCommand(FKernelRuntimeControlCommandContext &context);

} // namespace Fortress::Kernel
