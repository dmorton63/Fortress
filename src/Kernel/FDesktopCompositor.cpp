#include "Fortress/Kernel/FDesktopCompositor.hpp"

namespace Fortress::Kernel {

static bool IsRectValid(const FDesktopRect &rect) {
    return rect.Width > 0 && rect.Height > 0;
}

static Fortress::Core::int32 MinI32(Fortress::Core::int32 a, Fortress::Core::int32 b) {
    return a < b ? a : b;
}

static Fortress::Core::int32 MaxI32(Fortress::Core::int32 a, Fortress::Core::int32 b) {
    return a > b ? a : b;
}

static bool ClipRectToBounds(const FDesktopRect &input, const FDesktopRect &bounds, FDesktopRect &outRect) {
    outRect = FDesktopRect{};
    if (!IsRectValid(input) || !IsRectValid(bounds)) {
        return false;
    }

    const Fortress::Core::int32 left = MaxI32(input.X, bounds.X);
    const Fortress::Core::int32 top = MaxI32(input.Y, bounds.Y);
    const Fortress::Core::int32 right = MinI32(input.X + input.Width, bounds.X + bounds.Width);
    const Fortress::Core::int32 bottom = MinI32(input.Y + input.Height, bounds.Y + bounds.Height);
    if (right <= left || bottom <= top) {
        return false;
    }

    outRect = FDesktopRect{.X = left, .Y = top, .Width = right - left, .Height = bottom - top};
    return true;
}

static FDesktopRect UnionRects(const FDesktopRect &a, const FDesktopRect &b) {
    const Fortress::Core::int32 left = MinI32(a.X, b.X);
    const Fortress::Core::int32 top = MinI32(a.Y, b.Y);
    const Fortress::Core::int32 right = MaxI32(a.X + a.Width, b.X + b.Width);
    const Fortress::Core::int32 bottom = MaxI32(a.Y + a.Height, b.Y + b.Height);
    return FDesktopRect{.X = left, .Y = top, .Width = right - left, .Height = bottom - top};
}

Fortress::Core::int32 FDesktopCompositor::FindSurfaceIndex(FDesktopSurfaceId surfaceId) const {
    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        if (Surfaces[i].InUse && Surfaces[i].SurfaceId == surfaceId) {
            return static_cast<Fortress::Core::int32>(i);
        }
    }

    return -1;
}

Fortress::Core::uint32 FDesktopCompositor::ComputeHighestZOrder() const {
    Fortress::Core::uint32 highest = 0;
    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        if (!Surfaces[i].InUse) {
            continue;
        }

        if (Surfaces[i].ZOrder > highest) {
            highest = Surfaces[i].ZOrder;
        }
    }

    return highest;
}

bool FDesktopCompositor::Initialize(Fortress::Core::uint32 desktopWidth, Fortress::Core::uint32 desktopHeight) {
    if (desktopWidth == 0 || desktopHeight == 0) {
        return false;
    }

    Ready = false;
    NextSurfaceId = 1u;
    DirtyAcknowledgeCount = 0;
    DirtyAcknowledgePixels = 0;
    LastFrameDirtyContributorCount = 0;
    for (Fortress::Core::uint32 i = 0; i < MaxDirtyContributors; i++) {
        LastFrameDirtyContributors[i] = FDesktopDirtyContributor{};
    }
    RootSurfaceId = DesktopInvalidSurfaceId;
    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        Surfaces[i] = FSurfaceNode{};
    }

    FDesktopSurfaceId rootId = DesktopInvalidSurfaceId;
    if (!CreateSurface(DesktopInvalidSurfaceId,
                       FDesktopRect{.X = 0,
                                    .Y = 0,
                                    .Width = static_cast<Fortress::Core::int32>(desktopWidth),
                                    .Height = static_cast<Fortress::Core::int32>(desktopHeight)},
                       0u,
                       rootId)) {
        return false;
    }

    RootSurfaceId = rootId;
    Ready = true;
    return true;
}

bool FDesktopCompositor::IsReady() const {
    return Ready;
}

bool FDesktopCompositor::CreateSurface(FDesktopSurfaceId parentId,
                                       const FDesktopRect &bounds,
                                       Fortress::Core::uint32 zOrder,
                                       FDesktopSurfaceId &outSurfaceId) {
    outSurfaceId = DesktopInvalidSurfaceId;
    if (!IsRectValid(bounds)) {
        return false;
    }

    if (parentId != DesktopInvalidSurfaceId && FindSurfaceIndex(parentId) < 0) {
        return false;
    }

    Fortress::Core::int32 freeIndex = -1;
    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        if (!Surfaces[i].InUse) {
            freeIndex = static_cast<Fortress::Core::int32>(i);
            break;
        }
    }

    if (freeIndex < 0) {
        return false;
    }

    const FDesktopSurfaceId surfaceId = NextSurfaceId++;
    Surfaces[freeIndex] = FSurfaceNode{
        .InUse = true,
        .Visible = true,
        .Dirty = true,
        .SurfaceId = surfaceId,
        .ParentId = parentId,
        .ZOrder = zOrder,
        .Bounds = bounds,
        .DirtyRect = bounds,
    };

    outSurfaceId = surfaceId;
    return true;
}

bool FDesktopCompositor::CloseSurface(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || surfaceId == RootSurfaceId) {
        return false;
    }

    Surfaces[index] = FSurfaceNode{};
    return true;
}

bool FDesktopCompositor::SetSurfaceVisible(FDesktopSurfaceId surfaceId, bool visible) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    Surfaces[index].Visible = visible;
    Surfaces[index].Dirty = true;
    Surfaces[index].DirtyRect = Surfaces[index].Bounds;
    return true;
}

bool FDesktopCompositor::RaiseSurface(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    Surfaces[index].ZOrder = ComputeHighestZOrder() + 1u;
    Surfaces[index].Dirty = true;
    Surfaces[index].DirtyRect = Surfaces[index].Bounds;
    return true;
}

bool FDesktopCompositor::MoveSurface(FDesktopSurfaceId surfaceId, Fortress::Core::int32 x, Fortress::Core::int32 y) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || surfaceId == RootSurfaceId) {
        return false;
    }

    Surfaces[index].Bounds.X = x;
    Surfaces[index].Bounds.Y = y;
    Surfaces[index].Dirty = true;
    Surfaces[index].DirtyRect = Surfaces[index].Bounds;
    return true;
}

bool FDesktopCompositor::ResizeSurface(FDesktopSurfaceId surfaceId,
                                       Fortress::Core::int32 width,
                                       Fortress::Core::int32 height) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || surfaceId == RootSurfaceId || width <= 0 || height <= 0) {
        return false;
    }

    Surfaces[index].Bounds.Width = width;
    Surfaces[index].Bounds.Height = height;
    Surfaces[index].Dirty = true;
    Surfaces[index].DirtyRect = Surfaces[index].Bounds;
    return true;
}

bool FDesktopCompositor::MarkSurfaceDamaged(FDesktopSurfaceId surfaceId, const FDesktopRect &damageRect) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || !IsRectValid(damageRect)) {
        return false;
    }

    FDesktopRect clippedDamage = {};
    if (!ClipRectToBounds(damageRect, Surfaces[index].Bounds, clippedDamage)) {
        return false;
    }

    if (Surfaces[index].Dirty) {
        Surfaces[index].DirtyRect = UnionRects(Surfaces[index].DirtyRect, clippedDamage);
    } else {
        Surfaces[index].Dirty = true;
        Surfaces[index].DirtyRect = clippedDamage;
    }
    return true;
}

bool FDesktopCompositor::PeekSurfaceDirtyRegion(FDesktopSurfaceId surfaceId, FDesktopRect &outDirtyRect) const {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || !Surfaces[index].Dirty || !IsRectValid(Surfaces[index].DirtyRect)) {
        outDirtyRect = FDesktopRect{};
        return false;
    }

    outDirtyRect = Surfaces[index].DirtyRect;
    return true;
}

bool FDesktopCompositor::ConsumeSurfaceDirtyRegion(FDesktopSurfaceId surfaceId, FDesktopRect &outDirtyRect) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || !Surfaces[index].Dirty || !IsRectValid(Surfaces[index].DirtyRect)) {
        outDirtyRect = FDesktopRect{};
        return false;
    }

    outDirtyRect = Surfaces[index].DirtyRect;
    Surfaces[index].Dirty = false;
    Surfaces[index].DirtyRect = FDesktopRect{};
    DirtyAcknowledgeCount++;
    DirtyAcknowledgePixels += static_cast<Fortress::Core::uint64>(outDirtyRect.Width) *
                              static_cast<Fortress::Core::uint64>(outDirtyRect.Height);
    return true;
}

void FDesktopCompositor::ClearSurfaceDirty(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return;
    }

    Surfaces[index].Dirty = false;
    Surfaces[index].DirtyRect = FDesktopRect{};
}

bool FDesktopCompositor::GetFocusableSurfaceId(FDesktopSurfaceId &outSurfaceId) const {
    outSurfaceId = DesktopInvalidSurfaceId;
    bool haveCandidate = false;
    Fortress::Core::uint32 candidateZ = 0;

    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        const FSurfaceNode &node = Surfaces[i];
        if (!node.InUse || !node.Visible || node.SurfaceId == RootSurfaceId) {
            continue;
        }

        if (!haveCandidate || node.ZOrder >= candidateZ) {
            outSurfaceId = node.SurfaceId;
            candidateZ = node.ZOrder;
            haveCandidate = true;
        }
    }

    return haveCandidate;
}

bool FDesktopCompositor::GetNextFocusableSurfaceId(FDesktopSurfaceId currentSurfaceId,
                                                   FDesktopSurfaceId &outSurfaceId) const {
    outSurfaceId = DesktopInvalidSurfaceId;

    Fortress::Core::uint32 currentZ = 0;
    bool haveCurrent = false;
    if (currentSurfaceId != DesktopInvalidSurfaceId) {
        const Fortress::Core::int32 currentIndex = FindSurfaceIndex(currentSurfaceId);
        if (currentIndex >= 0) {
            currentZ = Surfaces[currentIndex].ZOrder;
            haveCurrent = true;
        }
    }

    bool haveCandidateAbove = false;
    Fortress::Core::uint32 candidateAboveZ = 0;
    FDesktopSurfaceId candidateAboveId = DesktopInvalidSurfaceId;

    bool haveCandidateFloor = false;
    Fortress::Core::uint32 candidateFloorZ = 0;
    FDesktopSurfaceId candidateFloorId = DesktopInvalidSurfaceId;

    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        const FSurfaceNode &node = Surfaces[i];
        if (!node.InUse || !node.Visible || node.SurfaceId == RootSurfaceId) {
            continue;
        }

        if (haveCurrent && node.ZOrder > currentZ) {
            if (!haveCandidateAbove || node.ZOrder < candidateAboveZ) {
                haveCandidateAbove = true;
                candidateAboveZ = node.ZOrder;
                candidateAboveId = node.SurfaceId;
            }
        }

        if (!haveCandidateFloor || node.ZOrder < candidateFloorZ) {
            haveCandidateFloor = true;
            candidateFloorZ = node.ZOrder;
            candidateFloorId = node.SurfaceId;
        }
    }

    if (haveCandidateAbove) {
        outSurfaceId = candidateAboveId;
        return true;
    }

    if (haveCandidateFloor) {
        outSurfaceId = candidateFloorId;
        return true;
    }

    return false;
}

bool FDesktopCompositor::GetTopSurfaceAtPoint(Fortress::Core::int32 x,
                                              Fortress::Core::int32 y,
                                              FDesktopSurfaceId &outSurfaceId) const {
    outSurfaceId = DesktopInvalidSurfaceId;
    Fortress::Core::uint32 topZ = 0;
    bool found = false;

    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        const FSurfaceNode &node = Surfaces[i];
        if (!node.InUse || !node.Visible || node.SurfaceId == RootSurfaceId) {
            continue;
        }

        const bool contains = (x >= node.Bounds.X) &&
                              (y >= node.Bounds.Y) &&
                              (x < (node.Bounds.X + node.Bounds.Width)) &&
                              (y < (node.Bounds.Y + node.Bounds.Height));
        if (!contains) {
            continue;
        }

        if (!found || node.ZOrder >= topZ) {
            found = true;
            topZ = node.ZOrder;
            outSurfaceId = node.SurfaceId;
        }
    }

    return found;
}

bool FDesktopCompositor::GetSurfaceBounds(FDesktopSurfaceId surfaceId, FDesktopRect &outBounds) const {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    outBounds = Surfaces[index].Bounds;
    return true;
}

bool FDesktopCompositor::GetSurfaceSnapshot(FDesktopSurfaceId surfaceId, FDesktopSurfaceSnapshot &outSnapshot) const {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    const FSurfaceNode &node = Surfaces[index];
    outSnapshot = FDesktopSurfaceSnapshot{
        .SurfaceId = node.SurfaceId,
        .Bounds = node.Bounds,
        .ZOrder = node.ZOrder,
        .Visible = node.Visible,
        .Dirty = node.Dirty,
    };
    return true;
}

bool FDesktopCompositor::SurfaceExists(FDesktopSurfaceId surfaceId) const {
    return FindSurfaceIndex(surfaceId) >= 0;
}

void FDesktopCompositor::GetActiveSurfaceIds(FDesktopSurfaceId *outSurfaceIds,
                                             Fortress::Core::uint32 capacity,
                                             Fortress::Core::uint32 &outCount) const {
    outCount = 0;
    if (outSurfaceIds == nullptr || capacity == 0u) {
        return;
    }

    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        const FSurfaceNode &surface = Surfaces[i];
        if (!surface.InUse || surface.SurfaceId == RootSurfaceId) {
            continue;
        }

        if (outCount >= capacity) {
            break;
        }

        outSurfaceIds[outCount++] = surface.SurfaceId;
    }
}

void FDesktopCompositor::GetActiveSurfaceSnapshots(FDesktopSurfaceSnapshot *outSnapshots,
                                                   Fortress::Core::uint32 capacity,
                                                   Fortress::Core::uint32 &outCount) const {
    outCount = 0;
    if (outSnapshots == nullptr || capacity == 0u) {
        return;
    }

    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        const FSurfaceNode &surface = Surfaces[i];
        if (!surface.InUse || surface.SurfaceId == RootSurfaceId) {
            continue;
        }

        if (outCount >= capacity) {
            break;
        }

        outSnapshots[outCount++] = FDesktopSurfaceSnapshot{
            .SurfaceId = surface.SurfaceId,
            .Bounds = surface.Bounds,
            .ZOrder = surface.ZOrder,
            .Visible = surface.Visible,
            .Dirty = surface.Dirty,
        };
    }
}

void FDesktopCompositor::SetLastFrameDirtyContributors(const FDesktopDirtyContributor *contributors,
                                                       Fortress::Core::uint32 contributorCount) {
    LastFrameDirtyContributorCount = 0u;
    for (Fortress::Core::uint32 i = 0; i < MaxDirtyContributors; i++) {
        LastFrameDirtyContributors[i] = FDesktopDirtyContributor{};
    }

    if (contributors == nullptr || contributorCount == 0u) {
        return;
    }

    const Fortress::Core::uint32 copyCount = contributorCount < MaxDirtyContributors ? contributorCount : MaxDirtyContributors;
    for (Fortress::Core::uint32 i = 0; i < copyCount; i++) {
        LastFrameDirtyContributors[i] = contributors[i];
    }
    LastFrameDirtyContributorCount = copyCount;
}

void FDesktopCompositor::GetLastFrameDirtyContributors(FDesktopDirtyContributor *outContributors,
                                                       Fortress::Core::uint32 capacity,
                                                       Fortress::Core::uint32 &outCount) const {
    outCount = 0u;
    if (outContributors == nullptr || capacity == 0u || LastFrameDirtyContributorCount == 0u) {
        return;
    }

    const Fortress::Core::uint32 copyCount = (LastFrameDirtyContributorCount < capacity)
                                                  ? LastFrameDirtyContributorCount
                                                  : capacity;
    for (Fortress::Core::uint32 i = 0; i < copyCount; i++) {
        outContributors[i] = LastFrameDirtyContributors[i];
    }
    outCount = copyCount;
}

void FDesktopCompositor::GetStats(FDesktopCompositorStats &outStats) const {
    Fortress::Core::uint32 surfaceCount = 0;
    Fortress::Core::uint32 dirtyCount = 0;
    Fortress::Core::uint64 dirtyPixelArea = 0;
    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        if (!Surfaces[i].InUse) {
            continue;
        }
        surfaceCount++;
        if (Surfaces[i].Dirty) {
            dirtyCount++;
            if (IsRectValid(Surfaces[i].DirtyRect)) {
                dirtyPixelArea += static_cast<Fortress::Core::uint64>(Surfaces[i].DirtyRect.Width) *
                                  static_cast<Fortress::Core::uint64>(Surfaces[i].DirtyRect.Height);
            }
        }
    }

    outStats = FDesktopCompositorStats{
        .SurfaceCount = surfaceCount,
        .DirtySurfaceCount = dirtyCount,
        .DirtyPixelArea = dirtyPixelArea,
        .DirtyAcknowledgeCount = DirtyAcknowledgeCount,
        .DirtyAcknowledgePixels = DirtyAcknowledgePixels,
        .HighestZOrder = ComputeHighestZOrder(),
    };
}

} // namespace Fortress::Kernel