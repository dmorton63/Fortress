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

void FDesktopCompositor::DetachFromZOrderList(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return;
    }

    FSurfaceNode &node = Surfaces[index];
    if (node.ZPrevSurfaceId != DesktopInvalidSurfaceId) {
        const Fortress::Core::int32 prevIndex = FindSurfaceIndex(node.ZPrevSurfaceId);
        if (prevIndex >= 0) {
            Surfaces[prevIndex].ZNextSurfaceId = node.ZNextSurfaceId;
        }
    } else {
        ZOrderHeadSurfaceId = node.ZNextSurfaceId;
    }

    if (node.ZNextSurfaceId != DesktopInvalidSurfaceId) {
        const Fortress::Core::int32 nextIndex = FindSurfaceIndex(node.ZNextSurfaceId);
        if (nextIndex >= 0) {
            Surfaces[nextIndex].ZPrevSurfaceId = node.ZPrevSurfaceId;
        }
    } else {
        ZOrderTailSurfaceId = node.ZPrevSurfaceId;
    }

    node.ZPrevSurfaceId = DesktopInvalidSurfaceId;
    node.ZNextSurfaceId = DesktopInvalidSurfaceId;
}

void FDesktopCompositor::InsertIntoZOrderList(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return;
    }

    FSurfaceNode &insertNode = Surfaces[index];
    insertNode.ZPrevSurfaceId = DesktopInvalidSurfaceId;
    insertNode.ZNextSurfaceId = DesktopInvalidSurfaceId;

    if (ZOrderHeadSurfaceId == DesktopInvalidSurfaceId) {
        ZOrderHeadSurfaceId = surfaceId;
        ZOrderTailSurfaceId = surfaceId;
        return;
    }

    FDesktopSurfaceId scanId = ZOrderHeadSurfaceId;
    FDesktopSurfaceId prevId = DesktopInvalidSurfaceId;
    while (scanId != DesktopInvalidSurfaceId) {
        const Fortress::Core::int32 scanIndex = FindSurfaceIndex(scanId);
        if (scanIndex < 0) {
            break;
        }

        const FSurfaceNode &scanNode = Surfaces[scanIndex];
        if (scanNode.ZOrder > insertNode.ZOrder) {
            break;
        }

        prevId = scanId;
        scanId = scanNode.ZNextSurfaceId;
    }

    if (prevId == DesktopInvalidSurfaceId) {
        const Fortress::Core::int32 oldHeadIndex = FindSurfaceIndex(ZOrderHeadSurfaceId);
        insertNode.ZNextSurfaceId = ZOrderHeadSurfaceId;
        if (oldHeadIndex >= 0) {
            Surfaces[oldHeadIndex].ZPrevSurfaceId = surfaceId;
        }
        ZOrderHeadSurfaceId = surfaceId;
        return;
    }

    const Fortress::Core::int32 prevIndex = FindSurfaceIndex(prevId);
    if (prevIndex < 0) {
        return;
    }

    insertNode.ZPrevSurfaceId = prevId;
    insertNode.ZNextSurfaceId = scanId;
    Surfaces[prevIndex].ZNextSurfaceId = surfaceId;

    if (scanId != DesktopInvalidSurfaceId) {
        const Fortress::Core::int32 scanIndex = FindSurfaceIndex(scanId);
        if (scanIndex >= 0) {
            Surfaces[scanIndex].ZPrevSurfaceId = surfaceId;
        }
    } else {
        ZOrderTailSurfaceId = surfaceId;
    }
}

void FDesktopCompositor::SyncZOrderValuesFromList() {
    Fortress::Core::uint32 order = 0u;
    FDesktopSurfaceId scanId = ZOrderHeadSurfaceId;
    while (scanId != DesktopInvalidSurfaceId) {
        const Fortress::Core::int32 scanIndex = FindSurfaceIndex(scanId);
        if (scanIndex < 0) {
            break;
        }

        Surfaces[scanIndex].ZOrder = order;
        order++;
        scanId = Surfaces[scanIndex].ZNextSurfaceId;
    }
}

void FDesktopCompositor::MarkCoalescedDirty(const FDesktopRect &rect) {
    if (!IsRectValid(rect)) {
        return;
    }

    if (HaveCoalescedDirty) {
        CoalescedDirtyRect = UnionRects(CoalescedDirtyRect, rect);
    } else {
        HaveCoalescedDirty = true;
        CoalescedDirtyRect = rect;
    }
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
    HaveCoalescedDirty = false;
    CoalescedDirtyRect = FDesktopRect{};
    DirtyAcknowledgeCount = 0;
    DirtyAcknowledgePixels = 0;
    LastFrameDirtyContributorCount = 0;
    for (Fortress::Core::uint32 i = 0; i < MaxDirtyContributors; i++) {
        LastFrameDirtyContributors[i] = FDesktopDirtyContributor{};
    }
    RootSurfaceId = DesktopInvalidSurfaceId;
    ZOrderHeadSurfaceId = DesktopInvalidSurfaceId;
    ZOrderTailSurfaceId = DesktopInvalidSurfaceId;
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
        .ZPrevSurfaceId = DesktopInvalidSurfaceId,
        .ZNextSurfaceId = DesktopInvalidSurfaceId,
        .ZOrder = zOrder,
        .Bounds = bounds,
        .DirtyRect = bounds,
    };

    InsertIntoZOrderList(surfaceId);
    SyncZOrderValuesFromList();
    MarkCoalescedDirty(bounds);

    outSurfaceId = surfaceId;
    return true;
}

bool FDesktopCompositor::CloseSurface(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || surfaceId == RootSurfaceId) {
        return false;
    }

    const FDesktopSurfaceId parentId = Surfaces[index].ParentId;
    const FDesktopRect oldBounds = Surfaces[index].Bounds;

    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        if (!Surfaces[i].InUse || Surfaces[i].SurfaceId == surfaceId) {
            continue;
        }

        if (Surfaces[i].ParentId == surfaceId) {
            Surfaces[i].ParentId = parentId;
        }
    }

    DetachFromZOrderList(surfaceId);
    SyncZOrderValuesFromList();
    MarkCoalescedDirty(oldBounds);

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
    MarkCoalescedDirty(Surfaces[index].Bounds);
    return true;
}

bool FDesktopCompositor::RaiseSurface(FDesktopSurfaceId surfaceId) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    DetachFromZOrderList(surfaceId);
    Surfaces[index].ZOrder = ComputeHighestZOrder() + 1u;
    InsertIntoZOrderList(surfaceId);
    SyncZOrderValuesFromList();
    Surfaces[index].Dirty = true;
    Surfaces[index].DirtyRect = Surfaces[index].Bounds;
    MarkCoalescedDirty(Surfaces[index].Bounds);
    return true;
}

bool FDesktopCompositor::MoveSurface(FDesktopSurfaceId surfaceId, Fortress::Core::int32 x, Fortress::Core::int32 y) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || surfaceId == RootSurfaceId) {
        return false;
    }

    const FDesktopRect oldBounds = Surfaces[index].Bounds;
    Surfaces[index].Bounds.X = x;
    Surfaces[index].Bounds.Y = y;
    Surfaces[index].Dirty = true;
    Surfaces[index].DirtyRect = UnionRects(oldBounds, Surfaces[index].Bounds);
    MarkCoalescedDirty(Surfaces[index].DirtyRect);
    return true;
}

bool FDesktopCompositor::ResizeSurface(FDesktopSurfaceId surfaceId,
                                       Fortress::Core::int32 width,
                                       Fortress::Core::int32 height) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || surfaceId == RootSurfaceId || width <= 0 || height <= 0) {
        return false;
    }

    const FDesktopRect oldBounds = Surfaces[index].Bounds;
    Surfaces[index].Bounds.Width = width;
    Surfaces[index].Bounds.Height = height;
    Surfaces[index].Dirty = true;
    Surfaces[index].DirtyRect = UnionRects(oldBounds, Surfaces[index].Bounds);
    MarkCoalescedDirty(Surfaces[index].DirtyRect);
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
    MarkCoalescedDirty(clippedDamage);
    return true;
}

bool FDesktopCompositor::MarkSurfaceDamagedLocal(FDesktopSurfaceId surfaceId, const FDesktopRect &localDamageRect) {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0 || !IsRectValid(localDamageRect)) {
        return false;
    }

    const FDesktopRect globalDamage{
        .X = Surfaces[index].Bounds.X + localDamageRect.X,
        .Y = Surfaces[index].Bounds.Y + localDamageRect.Y,
        .Width = localDamageRect.Width,
        .Height = localDamageRect.Height,
    };
    return MarkSurfaceDamaged(surfaceId, globalDamage);
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

bool FDesktopCompositor::ConsumeCoalescedDirtyRegion(FDesktopRect &outDirtyRect) {
    if (!HaveCoalescedDirty || !IsRectValid(CoalescedDirtyRect)) {
        outDirtyRect = FDesktopRect{};
        return false;
    }

    outDirtyRect = CoalescedDirtyRect;
    HaveCoalescedDirty = false;
    CoalescedDirtyRect = FDesktopRect{};
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

bool FDesktopCompositor::IsSurfaceFocusable(FDesktopSurfaceId surfaceId) const {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    const FSurfaceNode &node = Surfaces[index];
    return node.InUse && node.Visible && node.SurfaceId != RootSurfaceId;
}

bool FDesktopCompositor::GetFocusableSurfaceId(FDesktopSurfaceId &outSurfaceId) const {
    outSurfaceId = DesktopInvalidSurfaceId;
    FDesktopSurfaceId scanId = ZOrderTailSurfaceId;
    while (scanId != DesktopInvalidSurfaceId) {
        if (IsSurfaceFocusable(scanId)) {
            outSurfaceId = scanId;
            return true;
        }

        const Fortress::Core::int32 scanIndex = FindSurfaceIndex(scanId);
        if (scanIndex < 0) {
            break;
        }
        scanId = Surfaces[scanIndex].ZPrevSurfaceId;
    }

    return false;
}

bool FDesktopCompositor::GetNextFocusableSurfaceId(FDesktopSurfaceId currentSurfaceId,
                                                   FDesktopSurfaceId &outSurfaceId) const {
    outSurfaceId = DesktopInvalidSurfaceId;
    if (ZOrderTailSurfaceId == DesktopInvalidSurfaceId) {
        return false;
    }

    if (!IsSurfaceFocusable(currentSurfaceId)) {
        return GetFocusableSurfaceId(outSurfaceId);
    }

    const Fortress::Core::int32 currentIndex = FindSurfaceIndex(currentSurfaceId);
    if (currentIndex < 0) {
        return GetFocusableSurfaceId(outSurfaceId);
    }

    FDesktopSurfaceId scanId = Surfaces[currentIndex].ZPrevSurfaceId;
    if (scanId == DesktopInvalidSurfaceId) {
        scanId = ZOrderTailSurfaceId;
    }

    while (scanId != DesktopInvalidSurfaceId && scanId != currentSurfaceId) {
        if (IsSurfaceFocusable(scanId)) {
            outSurfaceId = scanId;
            return true;
        }

        const Fortress::Core::int32 scanIndex = FindSurfaceIndex(scanId);
        if (scanIndex < 0) {
            break;
        }
        scanId = Surfaces[scanIndex].ZPrevSurfaceId;
        if (scanId == DesktopInvalidSurfaceId) {
            scanId = ZOrderTailSurfaceId;
        }
    }

    if (IsSurfaceFocusable(currentSurfaceId)) {
        outSurfaceId = currentSurfaceId;
        return true;
    }

    return false;
}

bool FDesktopCompositor::GetTopSurfaceAtPoint(Fortress::Core::int32 x,
                                              Fortress::Core::int32 y,
                                              FDesktopSurfaceId &outSurfaceId) const {
    outSurfaceId = DesktopInvalidSurfaceId;
    FDesktopSurfaceId scanId = ZOrderTailSurfaceId;
    while (scanId != DesktopInvalidSurfaceId) {
        const Fortress::Core::int32 index = FindSurfaceIndex(scanId);
        if (index < 0) {
            break;
        }

        const FSurfaceNode &node = Surfaces[index];
        if (!node.InUse || !node.Visible || node.SurfaceId == RootSurfaceId) {
            scanId = node.ZPrevSurfaceId;
            continue;
        }

        const bool contains = (x >= node.Bounds.X) &&
                              (y >= node.Bounds.Y) &&
                              (x < (node.Bounds.X + node.Bounds.Width)) &&
                              (y < (node.Bounds.Y + node.Bounds.Height));
        if (contains) {
            outSurfaceId = node.SurfaceId;
            return true;
        }

        scanId = node.ZPrevSurfaceId;
    }

    return false;
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
        .ParentSurfaceId = node.ParentId,
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

bool FDesktopCompositor::IsSurfaceVisible(FDesktopSurfaceId surfaceId) const {
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    return Surfaces[index].Visible;
}

Fortress::Core::uint32 FDesktopCompositor::CountChildSurfaces(FDesktopSurfaceId parentId) const {
    Fortress::Core::uint32 count = 0u;
    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        if (!Surfaces[i].InUse) {
            continue;
        }

        if (Surfaces[i].ParentId == parentId) {
            count++;
        }
    }

    return count;
}

bool FDesktopCompositor::GetSurfaceParentId(FDesktopSurfaceId surfaceId, FDesktopSurfaceId &outParentId) const {
    outParentId = DesktopInvalidSurfaceId;
    const Fortress::Core::int32 index = FindSurfaceIndex(surfaceId);
    if (index < 0) {
        return false;
    }

    outParentId = Surfaces[index].ParentId;
    return true;
}

void FDesktopCompositor::GetChildSurfaceIds(FDesktopSurfaceId parentId,
                                            FDesktopSurfaceId *outSurfaceIds,
                                            Fortress::Core::uint32 capacity,
                                            Fortress::Core::uint32 &outCount) const {
    outCount = 0u;
    if (outSurfaceIds == nullptr || capacity == 0u) {
        return;
    }

    for (Fortress::Core::uint32 i = 0; i < MaxSurfaces; i++) {
        const FSurfaceNode &surface = Surfaces[i];
        if (!surface.InUse || surface.ParentId != parentId) {
            continue;
        }

        if (outCount >= capacity) {
            break;
        }

        outSurfaceIds[outCount++] = surface.SurfaceId;
    }
}

void FDesktopCompositor::GetSurfacesInZOrder(FDesktopSurfaceId *outSurfaceIds,
                                             Fortress::Core::uint32 capacity,
                                             Fortress::Core::uint32 &outCount) const {
    outCount = 0u;
    if (outSurfaceIds == nullptr || capacity == 0u) {
        return;
    }

    FDesktopSurfaceId scanId = ZOrderHeadSurfaceId;
    while (scanId != DesktopInvalidSurfaceId && outCount < capacity) {
        const Fortress::Core::int32 scanIndex = FindSurfaceIndex(scanId);
        if (scanIndex < 0) {
            break;
        }

        const FSurfaceNode &surface = Surfaces[scanIndex];
        if (surface.InUse && surface.SurfaceId != RootSurfaceId) {
            outSurfaceIds[outCount++] = surface.SurfaceId;
        }

        scanId = surface.ZNextSurfaceId;
    }
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
            .ParentSurfaceId = surface.ParentId,
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
        .CoalescedDirtyPixelArea = static_cast<Fortress::Core::uint64>(CoalescedDirtyRect.Width > 0 ? CoalescedDirtyRect.Width : 0) *
                                  static_cast<Fortress::Core::uint64>(CoalescedDirtyRect.Height > 0 ? CoalescedDirtyRect.Height : 0),
        .DirtyAcknowledgeCount = DirtyAcknowledgeCount,
        .DirtyAcknowledgePixels = DirtyAcknowledgePixels,
        .HighestZOrder = ComputeHighestZOrder(),
    };
}

} // namespace Fortress::Kernel