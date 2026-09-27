#include "Fortress/Kernel/FDesktopSurfaceOverlay.hpp"

#include "Fortress/Video/FColor.hpp"

namespace Fortress::Kernel {

static void DrawFilledRect(const Fortress::Video::FVideoSurfaceView &surface,
                           Fortress::Core::int32 x,
                           Fortress::Core::int32 y,
                           Fortress::Core::int32 width,
                           Fortress::Core::int32 height,
                           Fortress::Video::FColor color) {
    if (!surface.IsValid() || width <= 0 || height <= 0 ||
        surface.Desc.PixelFormat != Fortress::Video::EPixelFormat::Masked32) {
        return;
    }

    const Fortress::Core::uint32 packed = Fortress::Video::FVideoSurfaceOps::PackMasked32(color, surface.Desc.PixelMask);
    for (Fortress::Core::int32 py = 0; py < height; py++) {
        for (Fortress::Core::int32 px = 0; px < width; px++) {
            Fortress::Video::FVideoSurfaceOps::DrawPixel32(surface, x + px, y + py, packed);
        }
    }
}

static void DrawDesktopSurfaceFrameDirtyAware(const Fortress::Video::FVideoSurfaceView &surface,
                                              const FDesktopRect &bounds,
                                              const FDesktopRect &repaintRect,
                                              const Fortress::Video::FColor &color) {
    const Fortress::Core::uint32 packed = Fortress::Video::FVideoSurfaceOps::PackMasked32(color, surface.Desc.PixelMask);

    const Fortress::Core::int32 left = bounds.X;
    const Fortress::Core::int32 top = bounds.Y;
    const Fortress::Core::int32 right = bounds.X + bounds.Width - 1;
    const Fortress::Core::int32 bottom = bounds.Y + bounds.Height - 1;

    const Fortress::Core::int32 repaintLeft = repaintRect.X;
    const Fortress::Core::int32 repaintTop = repaintRect.Y;
    const Fortress::Core::int32 repaintRight = repaintRect.X + repaintRect.Width - 1;
    const Fortress::Core::int32 repaintBottom = repaintRect.Y + repaintRect.Height - 1;

    if (repaintTop <= top && top <= repaintBottom) {
        Fortress::Core::int32 x0 = left;
        Fortress::Core::int32 x1 = right;
        if (x0 < repaintLeft) {
            x0 = repaintLeft;
        }
        if (x1 > repaintRight) {
            x1 = repaintRight;
        }
        if (x0 <= x1) {
            Fortress::Video::FVideoSurfaceOps::DrawLine32(surface, x0, top, x1, top, packed);
        }
    }

    if (repaintTop <= bottom && bottom <= repaintBottom) {
        Fortress::Core::int32 x0 = left;
        Fortress::Core::int32 x1 = right;
        if (x0 < repaintLeft) {
            x0 = repaintLeft;
        }
        if (x1 > repaintRight) {
            x1 = repaintRight;
        }
        if (x0 <= x1) {
            Fortress::Video::FVideoSurfaceOps::DrawLine32(surface, x0, bottom, x1, bottom, packed);
        }
    }

    if (repaintLeft <= left && left <= repaintRight) {
        Fortress::Core::int32 y0 = top;
        Fortress::Core::int32 y1 = bottom;
        if (y0 < repaintTop) {
            y0 = repaintTop;
        }
        if (y1 > repaintBottom) {
            y1 = repaintBottom;
        }
        if (y0 <= y1) {
            Fortress::Video::FVideoSurfaceOps::DrawLine32(surface, left, y0, left, y1, packed);
        }
    }

    if (repaintLeft <= right && right <= repaintRight) {
        Fortress::Core::int32 y0 = top;
        Fortress::Core::int32 y1 = bottom;
        if (y0 < repaintTop) {
            y0 = repaintTop;
        }
        if (y1 > repaintBottom) {
            y1 = repaintBottom;
        }
        if (y0 <= y1) {
            Fortress::Video::FVideoSurfaceOps::DrawLine32(surface, right, y0, right, y1, packed);
        }
    }
}

static Fortress::Video::FColor BuildSurfaceColor(FDesktopSurfaceId surfaceId,
                                                 FDesktopSurfaceId focusSurfaceId,
                                                 FDesktopSurfaceId captureSurfaceId) {
    if (surfaceId == captureSurfaceId) {
        return Fortress::Video::FColor::RGB(255, 190, 80);
    }
    if (surfaceId == focusSurfaceId) {
        return Fortress::Video::FColor::RGB(120, 235, 140);
    }

    const Fortress::Core::uint8 seed = static_cast<Fortress::Core::uint8>((surfaceId * 41u) & 0x7Fu);
    return Fortress::Video::FColor::RGB(static_cast<Fortress::Core::uint8>(90u + (seed % 50u)),
                                        static_cast<Fortress::Core::uint8>(110u + ((seed / 2u) % 60u)),
                                        static_cast<Fortress::Core::uint8>(170u + ((seed / 3u) % 70u)));
}

static bool ClipRectToSurface(const Fortress::Video::FVideoSurfaceView &surface,
                              const FDesktopRect &input,
                              FDesktopRect &output) {
    output = FDesktopRect{};
    if (!surface.IsValid() || input.Width <= 0 || input.Height <= 0) {
        return false;
    }

    Fortress::Core::int32 left = input.X;
    Fortress::Core::int32 top = input.Y;
    Fortress::Core::int32 right = input.X + input.Width;
    Fortress::Core::int32 bottom = input.Y + input.Height;

    const Fortress::Core::int32 maxX = static_cast<Fortress::Core::int32>(surface.Desc.Width);
    const Fortress::Core::int32 maxY = static_cast<Fortress::Core::int32>(surface.Desc.Height);
    if (left < 0) {
        left = 0;
    }
    if (top < 0) {
        top = 0;
    }
    if (right > maxX) {
        right = maxX;
    }
    if (bottom > maxY) {
        bottom = maxY;
    }

    if (right <= left || bottom <= top) {
        return false;
    }

    output = FDesktopRect{.X = left, .Y = top, .Width = right - left, .Height = bottom - top};
    return true;
}

static bool IntersectRects(const FDesktopRect &a, const FDesktopRect &b, FDesktopRect &outRect) {
    outRect = FDesktopRect{};
    if (a.Width <= 0 || a.Height <= 0 || b.Width <= 0 || b.Height <= 0) {
        return false;
    }

    Fortress::Core::int32 left = a.X;
    if (b.X > left) {
        left = b.X;
    }

    Fortress::Core::int32 top = a.Y;
    if (b.Y > top) {
        top = b.Y;
    }

    Fortress::Core::int32 right = a.X + a.Width;
    const Fortress::Core::int32 bRight = b.X + b.Width;
    if (bRight < right) {
        right = bRight;
    }

    Fortress::Core::int32 bottom = a.Y + a.Height;
    const Fortress::Core::int32 bBottom = b.Y + b.Height;
    if (bBottom < bottom) {
        bottom = bBottom;
    }

    if (right <= left || bottom <= top) {
        return false;
    }

    outRect = FDesktopRect{.X = left, .Y = top, .Width = right - left, .Height = bottom - top};
    return true;
}

static void SubtractRect(const FDesktopRect &subject,
                         const FDesktopRect &occluder,
                         FDesktopRect *outPieces,
                         Fortress::Core::uint32 outCapacity,
                         Fortress::Core::uint32 &outCount) {
    outCount = 0u;
    if (outPieces == nullptr || outCapacity == 0u) {
        return;
    }

    FDesktopRect intersection{};
    if (!IntersectRects(subject, occluder, intersection)) {
        outPieces[outCount++] = subject;
        return;
    }

    const Fortress::Core::int32 subjectLeft = subject.X;
    const Fortress::Core::int32 subjectTop = subject.Y;
    const Fortress::Core::int32 subjectRight = subject.X + subject.Width;
    const Fortress::Core::int32 subjectBottom = subject.Y + subject.Height;

    const Fortress::Core::int32 interLeft = intersection.X;
    const Fortress::Core::int32 interTop = intersection.Y;
    const Fortress::Core::int32 interRight = intersection.X + intersection.Width;
    const Fortress::Core::int32 interBottom = intersection.Y + intersection.Height;

    auto TryPush = [&](const FDesktopRect &piece) {
        if (piece.Width <= 0 || piece.Height <= 0 || outCount >= outCapacity) {
            return;
        }
        outPieces[outCount++] = piece;
    };

    TryPush(FDesktopRect{.X = subjectLeft,
                         .Y = subjectTop,
                         .Width = subjectRight - subjectLeft,
                         .Height = interTop - subjectTop});

    TryPush(FDesktopRect{.X = subjectLeft,
                         .Y = interBottom,
                         .Width = subjectRight - subjectLeft,
                         .Height = subjectBottom - interBottom});

    TryPush(FDesktopRect{.X = subjectLeft,
                         .Y = interTop,
                         .Width = interLeft - subjectLeft,
                         .Height = interBottom - interTop});

    TryPush(FDesktopRect{.X = interRight,
                         .Y = interTop,
                         .Width = subjectRight - interRight,
                         .Height = interBottom - interTop});
}

static Fortress::Core::uint64 RectArea(const FDesktopRect &rect) {
    if (rect.Width <= 0 || rect.Height <= 0) {
        return 0;
    }

    return static_cast<Fortress::Core::uint64>(rect.Width) * static_cast<Fortress::Core::uint64>(rect.Height);
}

static void SortSnapshotsByZOrder(FDesktopSurfaceSnapshot *snapshots, Fortress::Core::uint32 count) {
    if (snapshots == nullptr || count < 2u) {
        return;
    }

    for (Fortress::Core::uint32 i = 0; i + 1u < count; i++) {
        Fortress::Core::uint32 minIndex = i;
        for (Fortress::Core::uint32 j = i + 1u; j < count; j++) {
            if (snapshots[j].ZOrder < snapshots[minIndex].ZOrder) {
                minIndex = j;
            }
        }

        if (minIndex != i) {
            const FDesktopSurfaceSnapshot temp = snapshots[i];
            snapshots[i] = snapshots[minIndex];
            snapshots[minIndex] = temp;
        }
    }
}

static Fortress::Core::uint32 SelectTopIndexes(const Fortress::Core::uint64 *values,
                                                Fortress::Core::uint32 valueCount,
                                                Fortress::Core::uint32 maxTop,
                                                Fortress::Core::uint32 *outIndexes) {
    if (values == nullptr || outIndexes == nullptr || valueCount == 0u || maxTop == 0u) {
        return 0u;
    }

    bool selected[16] = {};
    Fortress::Core::uint32 outCount = 0u;
    for (Fortress::Core::uint32 slot = 0; slot < maxTop; slot++) {
        Fortress::Core::uint32 bestIndex = 0u;
        Fortress::Core::uint64 bestValue = 0u;
        bool found = false;
        for (Fortress::Core::uint32 i = 0; i < valueCount; i++) {
            if (selected[i]) {
                continue;
            }

            if (!found || values[i] > bestValue) {
                found = true;
                bestIndex = i;
                bestValue = values[i];
            }
        }

        if (!found || bestValue == 0u) {
            break;
        }

        selected[bestIndex] = true;
        outIndexes[outCount++] = bestIndex;
    }

    return outCount;
}

void FDesktopSurfaceOverlay::Bind(FDesktopCompositor *compositor, FDesktopInputRouter *inputRouter) {
    Compositor = compositor;
    InputRouter = inputRouter;
    FrameStats = FDesktopOverlayFrameStats{};
}

bool FDesktopSurfaceOverlay::IsReady() const {
    return Compositor != nullptr && InputRouter != nullptr && Compositor->IsReady() && InputRouter->IsReady();
}

void FDesktopSurfaceOverlay::Render(const Fortress::Video::FVideoSurfaceView &surface) {
    FrameStats = FDesktopOverlayFrameStats{};
    if (!IsReady() || !surface.IsValid()) {
        return;
    }

    FDesktopSurfaceSnapshot snapshots[16] = {};
    Fortress::Core::uint32 count = 0u;
    Compositor->GetActiveSurfaceSnapshots(snapshots, 16u, count);
    SortSnapshotsByZOrder(snapshots, count);

    FDesktopInputRouterStats inputStats{};
    InputRouter->GetStats(inputStats);

    Fortress::Core::uint32 dirtyContributorIds[16] = {};
    Fortress::Core::uint64 dirtyContributorPixels[16] = {};
    Fortress::Core::uint32 dirtyContributorCount = 0u;

    Fortress::Core::uint32 splitContributorIds[16] = {};
    Fortress::Core::uint64 splitContributorCounts[16] = {};
    Fortress::Core::uint32 splitContributorCount = 0u;

    for (Fortress::Core::uint32 i = 0u; i < count; i++) {
        if (!snapshots[i].Visible) {
            continue;
        }

        FDesktopRect clippedBounds{};
        if (!ClipRectToSurface(surface, snapshots[i].Bounds, clippedBounds)) {
            continue;
        }

        FDesktopRect dirtyRect{};
        FDesktopRect clippedDirty{};
        const bool hadDirty = Compositor->ConsumeSurfaceDirtyRegion(snapshots[i].SurfaceId, dirtyRect);
        const bool haveClippedDirty = hadDirty && ClipRectToSurface(surface, dirtyRect, clippedDirty);
        if (hadDirty) {
            const Fortress::Core::uint64 dirtyPixels = RectArea(dirtyRect);
            FrameStats.AckCount++;
            FrameStats.AckPixels += dirtyPixels;
            if (dirtyContributorCount < 16u) {
                dirtyContributorIds[dirtyContributorCount] = snapshots[i].SurfaceId;
                dirtyContributorPixels[dirtyContributorCount] = dirtyPixels;
                dirtyContributorCount++;
            }
        }

        const Fortress::Video::FColor color =
            BuildSurfaceColor(snapshots[i].SurfaceId, inputStats.FocusSurfaceId, inputStats.CaptureSurfaceId);

        FDesktopRect repaintSeed = clippedBounds;
        if (haveClippedDirty) {
            repaintSeed = clippedDirty;
        } else {
            FrameStats.FallbackCount++;
            FrameStats.FallbackPixels += RectArea(clippedBounds);
        }

        FDesktopRect fragments[64] = {};
        Fortress::Core::uint32 fragmentCount = 1u;
        fragments[0] = repaintSeed;

        Fortress::Core::uint64 surfaceSplitCount = 0u;
        for (Fortress::Core::uint32 j = i + 1u; j < count && fragmentCount > 0u; j++) {
            if (!snapshots[j].Visible) {
                continue;
            }

            FDesktopRect occluderBounds{};
            if (!ClipRectToSurface(surface, snapshots[j].Bounds, occluderBounds)) {
                continue;
            }

            FDesktopRect nextFragments[64] = {};
            Fortress::Core::uint32 nextCount = 0u;
            for (Fortress::Core::uint32 k = 0u; k < fragmentCount; k++) {
                FDesktopRect pieces[4] = {};
                Fortress::Core::uint32 pieceCount = 0u;
                FrameStats.SplitAttempts++;
                surfaceSplitCount++;
                SubtractRect(fragments[k], occluderBounds, pieces, 4u, pieceCount);
                FrameStats.SplitFragmentsGenerated += pieceCount;

                for (Fortress::Core::uint32 p = 0u; p < pieceCount; p++) {
                    if (nextCount < 64u) {
                        nextFragments[nextCount++] = pieces[p];
                    } else {
                        FrameStats.SplitFragmentsDropped++;
                    }
                }
            }

            fragmentCount = nextCount;
            for (Fortress::Core::uint32 k = 0u; k < fragmentCount; k++) {
                fragments[k] = nextFragments[k];
            }
        }

        if (splitContributorCount < 16u) {
            splitContributorIds[splitContributorCount] = snapshots[i].SurfaceId;
            splitContributorCounts[splitContributorCount] = surfaceSplitCount;
            splitContributorCount++;
        }

        Fortress::Core::uint64 drawnPixels = 0u;
        for (Fortress::Core::uint32 k = 0u; k < fragmentCount; k++) {
            const FDesktopRect &repaintRect = fragments[k];
            DrawFilledRect(surface,
                           repaintRect.X,
                           repaintRect.Y,
                           repaintRect.Width,
                           repaintRect.Height,
                           Fortress::Video::FColor::RGB(10, 18, 26));

            DrawDesktopSurfaceFrameDirtyAware(surface, clippedBounds, repaintRect, color);
            drawnPixels += RectArea(repaintRect);
        }

        if (hadDirty && dirtyContributorCount > 0u) {
            dirtyContributorPixels[dirtyContributorCount - 1u] = drawnPixels;
        }
    }

    Fortress::Core::uint32 topIndexes[3] = {};
    FrameStats.TopDirtyCount = SelectTopIndexes(dirtyContributorPixels, dirtyContributorCount, 3u, topIndexes);
    for (Fortress::Core::uint32 i = 0u; i < FrameStats.TopDirtyCount; i++) {
        const Fortress::Core::uint32 index = topIndexes[i];
        FrameStats.TopDirtySurfaceIds[i] = dirtyContributorIds[index];
        FrameStats.TopDirtyPixels[i] = dirtyContributorPixels[index];
    }

    FrameStats.TopSplitCount = SelectTopIndexes(splitContributorCounts, splitContributorCount, 3u, topIndexes);
    for (Fortress::Core::uint32 i = 0u; i < FrameStats.TopSplitCount; i++) {
        const Fortress::Core::uint32 index = topIndexes[i];
        FrameStats.TopSplitSurfaceIds[i] = splitContributorIds[index];
        FrameStats.TopSplitCounts[i] = splitContributorCounts[index];
    }

    FDesktopDirtyContributor contributors[16] = {};
    for (Fortress::Core::uint32 i = 0u; i < dirtyContributorCount; i++) {
        contributors[i] = FDesktopDirtyContributor{
            .SurfaceId = dirtyContributorIds[i],
            .DirtyPixels = dirtyContributorPixels[i],
        };
    }
    Compositor->SetLastFrameDirtyContributors(contributors, dirtyContributorCount);
}

void FDesktopSurfaceOverlay::GetFrameStats(FDesktopOverlayFrameStats &outStats) const {
    outStats = FrameStats;
}

} // namespace Fortress::Kernel
