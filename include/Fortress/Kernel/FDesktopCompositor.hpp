#ifndef FORTRESS_KERNEL_FDESKTOPCOMPOSITOR_HPP
#define FORTRESS_KERNEL_FDESKTOPCOMPOSITOR_HPP

#include "Fortress/Core/FTypes.hpp"

namespace Fortress::Kernel {

struct FDesktopRect {
    Fortress::Core::int32 X = 0;
    Fortress::Core::int32 Y = 0;
    Fortress::Core::int32 Width = 0;
    Fortress::Core::int32 Height = 0;
};

using FDesktopSurfaceId = Fortress::Core::uint32;
static constexpr FDesktopSurfaceId DesktopInvalidSurfaceId = 0u;

struct FDesktopCompositorStats {
    Fortress::Core::uint32 SurfaceCount = 0;
    Fortress::Core::uint32 DirtySurfaceCount = 0;
    Fortress::Core::uint64 DirtyPixelArea = 0;
    Fortress::Core::uint64 CoalescedDirtyPixelArea = 0;
    Fortress::Core::uint64 DirtyAcknowledgeCount = 0;
    Fortress::Core::uint64 DirtyAcknowledgePixels = 0;
    Fortress::Core::uint32 HighestZOrder = 0;
};

struct FDesktopSurfaceSnapshot {
    FDesktopSurfaceId SurfaceId = DesktopInvalidSurfaceId;
    FDesktopSurfaceId ParentSurfaceId = DesktopInvalidSurfaceId;
    FDesktopRect Bounds = {};
    Fortress::Core::uint32 ZOrder = 0;
    bool Visible = false;
    bool Dirty = false;
};

struct FDesktopDirtyContributor {
    FDesktopSurfaceId SurfaceId = DesktopInvalidSurfaceId;
    Fortress::Core::uint64 DirtyPixels = 0;
};

class FDesktopCompositor {
  public:
    bool Initialize(Fortress::Core::uint32 desktopWidth, Fortress::Core::uint32 desktopHeight);
    bool IsReady() const;

    bool CreateSurface(FDesktopSurfaceId parentId,
               const FDesktopRect &bounds,
               Fortress::Core::uint32 zOrder,
               FDesktopSurfaceId &outSurfaceId);
    bool CloseSurface(FDesktopSurfaceId surfaceId);
    bool SetSurfaceVisible(FDesktopSurfaceId surfaceId, bool visible);
    bool RaiseSurface(FDesktopSurfaceId surfaceId);
    bool MoveSurface(FDesktopSurfaceId surfaceId, Fortress::Core::int32 x, Fortress::Core::int32 y);
    bool ResizeSurface(FDesktopSurfaceId surfaceId, Fortress::Core::int32 width, Fortress::Core::int32 height);
    bool MarkSurfaceDamaged(FDesktopSurfaceId surfaceId, const FDesktopRect &damageRect);
    bool MarkSurfaceDamagedLocal(FDesktopSurfaceId surfaceId, const FDesktopRect &localDamageRect);
    bool PeekSurfaceDirtyRegion(FDesktopSurfaceId surfaceId, FDesktopRect &outDirtyRect) const;
    bool ConsumeSurfaceDirtyRegion(FDesktopSurfaceId surfaceId, FDesktopRect &outDirtyRect);
    bool ConsumeCoalescedDirtyRegion(FDesktopRect &outDirtyRect);
    void ClearSurfaceDirty(FDesktopSurfaceId surfaceId);
    bool IsSurfaceFocusable(FDesktopSurfaceId surfaceId) const;
    bool GetFocusableSurfaceId(FDesktopSurfaceId &outSurfaceId) const;
    bool GetNextFocusableSurfaceId(FDesktopSurfaceId currentSurfaceId, FDesktopSurfaceId &outSurfaceId) const;
    bool GetTopSurfaceAtPoint(Fortress::Core::int32 x, Fortress::Core::int32 y, FDesktopSurfaceId &outSurfaceId) const;
    bool GetSurfaceBounds(FDesktopSurfaceId surfaceId, FDesktopRect &outBounds) const;
    bool GetSurfaceParentId(FDesktopSurfaceId surfaceId, FDesktopSurfaceId &outParentId) const;
    bool GetSurfaceSnapshot(FDesktopSurfaceId surfaceId, FDesktopSurfaceSnapshot &outSnapshot) const;
    bool SurfaceExists(FDesktopSurfaceId surfaceId) const;
    bool IsSurfaceVisible(FDesktopSurfaceId surfaceId) const;
    Fortress::Core::uint32 CountChildSurfaces(FDesktopSurfaceId parentId) const;
    void GetChildSurfaceIds(FDesktopSurfaceId parentId,
                            FDesktopSurfaceId *outSurfaceIds,
                            Fortress::Core::uint32 capacity,
                            Fortress::Core::uint32 &outCount) const;
    void GetSurfacesInZOrder(FDesktopSurfaceId *outSurfaceIds,
                             Fortress::Core::uint32 capacity,
                             Fortress::Core::uint32 &outCount) const;
    void GetActiveSurfaceIds(FDesktopSurfaceId *outSurfaceIds,
                 Fortress::Core::uint32 capacity,
                 Fortress::Core::uint32 &outCount) const;
    void GetActiveSurfaceSnapshots(FDesktopSurfaceSnapshot *outSnapshots,
                     Fortress::Core::uint32 capacity,
                     Fortress::Core::uint32 &outCount) const;
    void SetLastFrameDirtyContributors(const FDesktopDirtyContributor *contributors,
                                       Fortress::Core::uint32 contributorCount);
    void GetLastFrameDirtyContributors(FDesktopDirtyContributor *outContributors,
                                       Fortress::Core::uint32 capacity,
                                       Fortress::Core::uint32 &outCount) const;

    void GetStats(FDesktopCompositorStats &outStats) const;

  private:
    static constexpr Fortress::Core::uint32 MaxSurfaces = 32u;
        static constexpr Fortress::Core::uint32 MaxDirtyContributors = 16u;

    struct FSurfaceNode {
        bool InUse = false;
        bool Visible = true;
        bool Dirty = false;
        FDesktopSurfaceId SurfaceId = DesktopInvalidSurfaceId;
        FDesktopSurfaceId ParentId = DesktopInvalidSurfaceId;
        FDesktopSurfaceId ZPrevSurfaceId = DesktopInvalidSurfaceId;
        FDesktopSurfaceId ZNextSurfaceId = DesktopInvalidSurfaceId;
        Fortress::Core::uint32 ZOrder = 0;
        FDesktopRect Bounds = {};
        FDesktopRect DirtyRect = {};
    };

    Fortress::Core::int32 FindSurfaceIndex(FDesktopSurfaceId surfaceId) const;
    void DetachFromZOrderList(FDesktopSurfaceId surfaceId);
    void InsertIntoZOrderList(FDesktopSurfaceId surfaceId);
    void SyncZOrderValuesFromList();
    void MarkCoalescedDirty(const FDesktopRect &rect);
    Fortress::Core::uint32 ComputeHighestZOrder() const;

    bool Ready = false;
    Fortress::Core::uint32 NextSurfaceId = 1u;
    bool HaveCoalescedDirty = false;
    FDesktopRect CoalescedDirtyRect = {};
    Fortress::Core::uint64 DirtyAcknowledgeCount = 0;
    Fortress::Core::uint64 DirtyAcknowledgePixels = 0;
    FDesktopDirtyContributor LastFrameDirtyContributors[MaxDirtyContributors] = {};
    Fortress::Core::uint32 LastFrameDirtyContributorCount = 0;
    FDesktopSurfaceId RootSurfaceId = DesktopInvalidSurfaceId;
    FDesktopSurfaceId ZOrderHeadSurfaceId = DesktopInvalidSurfaceId;
    FDesktopSurfaceId ZOrderTailSurfaceId = DesktopInvalidSurfaceId;
    FSurfaceNode Surfaces[MaxSurfaces] = {};
};

} // namespace Fortress::Kernel

#endif