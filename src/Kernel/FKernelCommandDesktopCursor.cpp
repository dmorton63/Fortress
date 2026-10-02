#include "Fortress/Kernel/FKernelCommandDesktopCursor.hpp"
#include "Fortress/Kernel/FKernelCommandConsole.hpp"

namespace Fortress::Kernel {

static void ClearCommandInput(FKernelDesktopCursorCommandContext &context) {
    if (context.CommandLength != nullptr) {
        *context.CommandLength = 0;
    }
    if (context.CommandBuffer != nullptr) {
        context.CommandBuffer[0] = '\0';
    }
}

static bool RequireCompositor(FKernelDesktopCursorCommandContext &context) {
    if (context.DesktopCompositor != nullptr) {
        return true;
    }

    context.PushLogFn("DESKTOP COMPOSITOR UNBOUND");
    ClearCommandInput(context);
    return false;
}

static bool RequireInputRouter(FKernelDesktopCursorCommandContext &context) {
    if (context.DesktopInputRouter != nullptr) {
        return true;
    }

    context.PushLogFn("DESKTOP INPUT ROUTER UNBOUND");
    ClearCommandInput(context);
    return false;
}

static bool RequireContentHost(FKernelDesktopCursorCommandContext &context) {
    if (context.DesktopSurfaceContentHost != nullptr) {
        return true;
    }

    context.PushLogFn("DESKTOP CONTENT HOST UNBOUND");
    ClearCommandInput(context);
    return false;
}

bool TryProcessDesktopSurfaceCommand(FKernelDesktopCursorCommandContext &context) {
    if (context.CommandBuffer == nullptr || context.CommandLength == nullptr || context.PushLogFn == nullptr ||
        context.AppendCharFn == nullptr || context.AppendStringFn == nullptr || context.AppendUIntFn == nullptr ||
        context.StrEqFn == nullptr || context.StartsWithFn == nullptr || context.ParseUIntFn == nullptr ||
        context.ReadTokenFn == nullptr || context.MatchAnyExactFn == nullptr || context.MatchAnyPrefixFn == nullptr ||
        context.AliasArgAfterPrefixFn == nullptr) {
        return false;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopstat", "dsksurfstat", "windowstat")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        FDesktopCompositorStats stats{};
        context.DesktopCompositor->GetStats(stats);
        char line[128] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP S ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.SurfaceCount);
        context.AppendStringFn(line, sizeof(line), pos, " D ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.DirtySurfaceCount);
        context.AppendStringFn(line, sizeof(line), pos, " DA ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.DirtyPixelArea);
        context.AppendStringFn(line, sizeof(line), pos, " ACK ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.DirtyAcknowledgeCount);
        context.AppendStringFn(line, sizeof(line), pos, " APX ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.DirtyAcknowledgePixels);
        context.AppendStringFn(line, sizeof(line), pos, " Z ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.HighestZOrder);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopcontrols", "dsksurfcontrols", "windowcontrols") ||
        context.MatchAnyPrefixFn(context.CommandBuffer, "desktopcontrols ", "dsksurfcontrols ", "windowcontrols ")) {
        if (!RequireContentHost(context)) {
            return true;
        }

        uint64_t parsedSurfaceId = 0u;
        FDesktopSurfaceId surfaceId = DesktopInvalidSurfaceId;
        if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopcontrols ", "dsksurfcontrols ", "windowcontrols ")) {
            const char *arg = context.AliasArgAfterPrefixFn(
                context.CommandBuffer, "desktopcontrols ", 16, "dsksurfcontrols ", 16, "windowcontrols ", 15);
            if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u) {
                context.PushLogFn("DESKTOPCONTROLS ARG INVALID");
                ClearCommandInput(context);
                return true;
            }
            surfaceId = static_cast<FDesktopSurfaceId>(parsedSurfaceId);
        } else {
            if (!RequireInputRouter(context)) {
                return true;
            }
            FDesktopInputRouterStats stats{};
            context.DesktopInputRouter->GetStats(stats);
            surfaceId = stats.FocusSurfaceId;
            if (surfaceId == DesktopInvalidSurfaceId) {
                context.PushLogFn("DESKTOPCONTROLS NO FOCUS");
                ClearCommandInput(context);
                return true;
            }
        }

        Fortress::Core::uint32 controlCount = 0u;
        Fortress::Core::uint32 focusedControlId = 0u;
        if (!context.DesktopSurfaceContentHost->GetSurfaceControlSummary(surfaceId, controlCount, focusedControlId)) {
            context.PushLogFn("DESKTOPCONTROLS NONE");
            return true;
        }

        char line[196] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP CONTROLS S ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(surfaceId));
        context.AppendStringFn(line, sizeof(line), pos, " N ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(controlCount));
        context.AppendStringFn(line, sizeof(line), pos, " F ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(focusedControlId));
        context.PushLogFn(line);

        for (Fortress::Core::uint32 i = 0u; i < controlCount; i++) {
            FDesktopControlNode node{};
            if (!context.DesktopSurfaceContentHost->GetSurfaceControlNode(surfaceId, i, node)) {
                continue;
            }

            pos = 0;
            line[0] = '\0';
            context.AppendStringFn(line, sizeof(line), pos, "CTRL ");
            context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(node.ControlId));
            context.AppendStringFn(line, sizeof(line), pos, " T ");
            context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(node.Type == EDesktopControlType::Button ? 1u : 0u));
            context.AppendStringFn(line, sizeof(line), pos, " X ");
            context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(node.LocalBounds.X));
            context.AppendStringFn(line, sizeof(line), pos, " Y ");
            context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(node.LocalBounds.Y));
            context.AppendStringFn(line, sizeof(line), pos, " W ");
            context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(node.LocalBounds.Width));
            context.AppendStringFn(line, sizeof(line), pos, " H ");
            context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(node.LocalBounds.Height));
            context.AppendStringFn(line, sizeof(line), pos, " F ");
            context.AppendUIntFn(line, sizeof(line), pos, node.Focused ? 1u : 0u);
            context.AppendStringFn(line, sizeof(line), pos, " P ");
            context.AppendUIntFn(line, sizeof(line), pos, node.Pressed ? 1u : 0u);
            context.PushLogFn(line);
        }
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopraise", "dsksurfraise", "windowraise") ||
        context.MatchAnyPrefixFn(context.CommandBuffer, "desktopraise ", "dsksurfraise ", "windowraise ")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        FDesktopSurfaceId surfaceId = 0u;
        if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopraise ", "dsksurfraise ", "windowraise ")) {
            const char *arg = context.AliasArgAfterPrefixFn(
                context.CommandBuffer, "desktopraise ", 13, "dsksurfraise ", 12, "windowraise ", 12);
            uint64_t parsedSurfaceId = 0;
            if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u) {
                context.PushLogFn("DESKTOPRAISE ARG INVALID");
                ClearCommandInput(context);
                return true;
            }
            surfaceId = static_cast<FDesktopSurfaceId>(parsedSurfaceId);
        } else if (!context.DesktopCompositor->GetFocusableSurfaceId(surfaceId)) {
            context.PushLogFn("DESKTOPRAISE NO SURFACE");
            ClearCommandInput(context);
            return true;
        }

        if (!context.DesktopCompositor->RaiseSurface(surfaceId)) {
            context.PushLogFn("DESKTOPRAISE FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP SURFACE RAISE ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(surfaceId));
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktophide ", "dsksurfhide ", "windowhide ")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        const char *arg = context.AliasArgAfterPrefixFn(
            context.CommandBuffer, "desktophide ", 12, "dsksurfhide ", 12, "windowhide ", 11);
        uint64_t parsedSurfaceId = 0;
        if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u) {
            context.PushLogFn("DESKTOPHIDE ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (!context.DesktopCompositor->SetSurfaceVisible(static_cast<FDesktopSurfaceId>(parsedSurfaceId), false)) {
            context.PushLogFn("DESKTOPHIDE FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP SURFACE HIDE ");
        context.AppendUIntFn(line, sizeof(line), pos, parsedSurfaceId);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopshow ", "dsksurfshow ", "windowshow ")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        const char *arg = context.AliasArgAfterPrefixFn(
            context.CommandBuffer, "desktopshow ", 12, "dsksurfshow ", 12, "windowshow ", 11);
        uint64_t parsedSurfaceId = 0;
        if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u) {
            context.PushLogFn("DESKTOPSHOW ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (!context.DesktopCompositor->SetSurfaceVisible(static_cast<FDesktopSurfaceId>(parsedSurfaceId), true)) {
            context.PushLogFn("DESKTOPSHOW FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP SURFACE SHOW ");
        context.AppendUIntFn(line, sizeof(line), pos, parsedSurfaceId);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopdamage ", "dsksurfdamage ", "windowdamage ")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        const char *arg = context.AliasArgAfterPrefixFn(
            context.CommandBuffer, "desktopdamage ", 14, "dsksurfdamage ", 14, "windowdamage ", 13);
        uint64_t parsedSurfaceId = 0;
        if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u) {
            context.PushLogFn("DESKTOPDAMAGE ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        FDesktopRect bounds{};
        if (!context.DesktopCompositor->GetSurfaceBounds(static_cast<FDesktopSurfaceId>(parsedSurfaceId), bounds) ||
            !context.DesktopCompositor->MarkSurfaceDamaged(static_cast<FDesktopSurfaceId>(parsedSurfaceId), bounds)) {
            context.PushLogFn("DESKTOPDAMAGE FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP SURFACE DAMAGE ");
        context.AppendUIntFn(line, sizeof(line), pos, parsedSurfaceId);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopcreate", "dsksurfcreate", "windowcreate") ||
        context.MatchAnyPrefixFn(context.CommandBuffer, "desktopcreate ", "dsksurfcreate ", "windowcreate ")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        const char *cursor =
            context.AliasArgAfterPrefixFn(context.CommandBuffer, "desktopcreate", 13, "dsksurfcreate", 13, "windowcreate", 12);
        char token[16] = {};
        uint64_t x = 0;
        uint64_t y = 0;
        uint64_t w = 0;
        uint64_t h = 0;
        uint64_t z = 0;

        if (!context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, x) ||
            !context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, y) ||
            !context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, w) ||
            !context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, h) ||
            !context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, z)) {
            context.PushLogFn("DSKSURFCREATE USAGE X Y W H Z");
            ClearCommandInput(context);
            return true;
        }

        FDesktopSurfaceId surfaceId = DesktopInvalidSurfaceId;
        if (!context.DesktopCompositor->CreateSurface(1u,
                                                      FDesktopRect{.X = static_cast<int32_t>(x),
                                                                   .Y = static_cast<int32_t>(y),
                                                                   .Width = static_cast<int32_t>(w),
                                                                   .Height = static_cast<int32_t>(h)},
                                                      static_cast<uint32_t>(z),
                                                      surfaceId)) {
            context.PushLogFn("DSKSURFCREATE FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DSKSURF CREATE ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(surfaceId));
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopclose ", "dsksurfclose ", "windowclose ")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        const char *arg = context.AliasArgAfterPrefixFn(
            context.CommandBuffer, "desktopclose ", 13, "dsksurfclose ", 13, "windowclose ", 12);
        uint64_t parsedSurfaceId = 0;
        if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u ||
            !context.DesktopCompositor->CloseSurface(static_cast<FDesktopSurfaceId>(parsedSurfaceId))) {
            context.PushLogFn("DSKSURFCLOSE FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DSKSURF CLOSE ");
        context.AppendUIntFn(line, sizeof(line), pos, parsedSurfaceId);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopmove ", "dsksurfmove ", "windowmove ")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        const char *cursor =
            context.AliasArgAfterPrefixFn(context.CommandBuffer, "desktopmove ", 12, "dsksurfmove ", 12, "windowmove ", 11);
        char token[16] = {};
        uint64_t id = 0;
        uint64_t x = 0;
        uint64_t y = 0;
        if (!context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, id) ||
            !context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, x) ||
            !context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, y) || id == 0u) {
            context.PushLogFn("DSKSURFMOVE USAGE ID X Y");
            ClearCommandInput(context);
            return true;
        }

        if (!context.DesktopCompositor->MoveSurface(static_cast<FDesktopSurfaceId>(id),
                                                    static_cast<int32_t>(x),
                                                    static_cast<int32_t>(y))) {
            context.PushLogFn("DSKSURFMOVE FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DSKSURF MOVE ");
        context.AppendUIntFn(line, sizeof(line), pos, id);
        context.AppendStringFn(line, sizeof(line), pos, " X ");
        context.AppendUIntFn(line, sizeof(line), pos, x);
        context.AppendStringFn(line, sizeof(line), pos, " Y ");
        context.AppendUIntFn(line, sizeof(line), pos, y);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopresize ", "dsksurfresize ", "windowresize ")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        const char *cursor = context.AliasArgAfterPrefixFn(
            context.CommandBuffer, "desktopresize ", 14, "dsksurfresize ", 14, "windowresize ", 13);
        char token[16] = {};
        uint64_t id = 0;
        uint64_t w = 0;
        uint64_t h = 0;
        if (!context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, id) ||
            !context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, w) ||
            !context.ReadTokenFn(cursor, token, sizeof(token)) || !context.ParseUIntFn(token, h) || id == 0u || w == 0u ||
            h == 0u) {
            context.PushLogFn("DSKSURFRESIZE USAGE ID W H");
            ClearCommandInput(context);
            return true;
        }

        if (!context.DesktopCompositor->ResizeSurface(static_cast<FDesktopSurfaceId>(id),
                                                      static_cast<int32_t>(w),
                                                      static_cast<int32_t>(h))) {
            context.PushLogFn("DSKSURFRESIZE FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DSKSURF RESIZE ");
        context.AppendUIntFn(line, sizeof(line), pos, id);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopfocus", "dsksurffocus", "windowfocus")) {
        if (!RequireInputRouter(context)) {
            return true;
        }

        FDesktopInputRouterStats stats{};
        context.DesktopInputRouter->GetStats(stats);
        char line[96] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP FOCUS ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(stats.FocusSurfaceId));
        context.AppendStringFn(line, sizeof(line), pos, " CAP ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(stats.CaptureSurfaceId));
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopfocus next", "dsksurffocus next", "windowfocus next")) {
        if (!RequireInputRouter(context)) {
            return true;
        }

        if (!context.DesktopInputRouter->FocusNext()) {
            context.PushLogFn("DESKTOPFOCUS NEXT FAIL");
            ClearCommandInput(context);
            return true;
        }

        FDesktopInputRouterStats stats{};
        context.DesktopInputRouter->GetStats(stats);
        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP FOCUS ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(stats.FocusSurfaceId));
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopfocus ", "dsksurffocus ", "windowfocus ")) {
        if (!RequireInputRouter(context)) {
            return true;
        }

        const char *arg = context.AliasArgAfterPrefixFn(
            context.CommandBuffer, "desktopfocus ", 13, "dsksurffocus ", 13, "windowfocus ", 12);
        uint64_t parsedSurfaceId = 0;
        if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u) {
            context.PushLogFn("DESKTOPFOCUS ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (!context.DesktopInputRouter->SetFocus(static_cast<FDesktopSurfaceId>(parsedSurfaceId))) {
            context.PushLogFn("DESKTOPFOCUS FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP FOCUS ");
        context.AppendUIntFn(line, sizeof(line), pos, parsedSurfaceId);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopcapture", "dsksurfcapture", "windowcapture")) {
        if (!RequireInputRouter(context)) {
            return true;
        }

        FDesktopInputRouterStats stats{};
        context.DesktopInputRouter->GetStats(stats);
        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP CAPTURE ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(stats.CaptureSurfaceId));
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopcapture off", "dsksurfcapture off", "windowcapture off")) {
        if (!RequireInputRouter(context)) {
            return true;
        }

        context.DesktopInputRouter->ReleaseCapture();
        context.PushLogFn("DESKTOP CAPTURE 0");
        return true;
    }

    if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopcapture ", "dsksurfcapture ", "windowcapture ")) {
        if (!RequireInputRouter(context)) {
            return true;
        }

        const char *arg = context.AliasArgAfterPrefixFn(
            context.CommandBuffer, "desktopcapture ", 15, "dsksurfcapture ", 15, "windowcapture ", 14);
        uint64_t parsedSurfaceId = 0;
        if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u) {
            context.PushLogFn("DESKTOPCAPTURE ARG INVALID");
            ClearCommandInput(context);
            return true;
        }

        if (!context.DesktopInputRouter->SetCapture(static_cast<FDesktopSurfaceId>(parsedSurfaceId))) {
            context.PushLogFn("DESKTOPCAPTURE FAIL");
            ClearCommandInput(context);
            return true;
        }

        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP CAPTURE ");
        context.AppendUIntFn(line, sizeof(line), pos, parsedSurfaceId);
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopinput", "dsksurfinput", "windowinput")) {
        if (!RequireInputRouter(context)) {
            return true;
        }

        FDesktopInputRouterStats stats{};
        context.DesktopInputRouter->GetStats(stats);
        char line[96] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP INPUT F ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(stats.FocusSurfaceId));
        context.AppendStringFn(line, sizeof(line), pos, " CAP ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(stats.CaptureSurfaceId));
        context.AppendStringFn(line, sizeof(line), pos, " RX ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.RoutedKeyCount);
        context.AppendStringFn(line, sizeof(line), pos, " DROP ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.DroppedKeyCount);
        context.AppendStringFn(line, sizeof(line), pos, " PTR ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.PointerSampleCount);
        context.AppendStringFn(line, sizeof(line), pos, " PE ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.PointerPressEdgeCount);
        context.AppendStringFn(line, sizeof(line), pos, " HIT ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.PointerHitSurfaceCount);
        context.AppendStringFn(line, sizeof(line), pos, " CLK ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.PointerFocusClickCount);
        context.AppendStringFn(line, sizeof(line), pos, " FFAIL ");
        context.AppendUIntFn(line, sizeof(line), pos, stats.PointerFocusFailCount);

        Fortress::Core::uint32 xhciEp = 0u;
        Fortress::Core::uint32 xhciKind = 0u;
        Fortress::Core::uint32 xhciCc = 0u;
        Fortress::Core::uint32 xhciNoXferStreak = 0u;
        FKernelCommandConsole::GetXhciBackgroundInputTelemetry(xhciEp, xhciKind, xhciCc, xhciNoXferStreak);
        context.AppendStringFn(line, sizeof(line), pos, " XEP ");
        context.AppendUIntFn(line, sizeof(line), pos, xhciEp);
        context.AppendStringFn(line, sizeof(line), pos, " XK ");
        context.AppendUIntFn(line, sizeof(line), pos, xhciKind);
        context.AppendStringFn(line, sizeof(line), pos, " XCC ");
        context.AppendUIntFn(line, sizeof(line), pos, xhciCc);
        context.AppendStringFn(line, sizeof(line), pos, " XNFX ");
        context.AppendUIntFn(line, sizeof(line), pos, xhciNoXferStreak);

        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktoplist", "dsksurflist", "windowlist")) {
        if (!RequireCompositor(context)) {
            return true;
        }

        FDesktopSurfaceId ids[16] = {};
        Fortress::Core::uint32 count = 0;
        context.DesktopCompositor->GetActiveSurfaceIds(ids, 16u, count);

        char line[96] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DSKSURF LIST N ");
        context.AppendUIntFn(line, sizeof(line), pos, count);
        if (count == 0u) {
            context.PushLogFn(line);
        } else {
            context.AppendStringFn(line, sizeof(line), pos, " IDS");
            for (Fortress::Core::uint32 i = 0; i < count; i++) {
                context.AppendCharFn(line, sizeof(line), pos, ' ');
                context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(ids[i]));
            }
            context.PushLogFn(line);
        }
        return true;
    }

    const bool isDirtyExact = context.MatchAnyExactFn(context.CommandBuffer, "desktopdirty", "dsksurfdirty", "windowdirty") ||
                             context.StrEqFn(context.CommandBuffer, "desksurfdirty");
    const bool isDirtyPrefix = context.MatchAnyPrefixFn(context.CommandBuffer, "desktopdirty ", "dsksurfdirty ", "windowdirty ") ||
                              context.StartsWithFn(context.CommandBuffer, "desksurfdirty ");
    if (isDirtyExact || isDirtyPrefix) {
        if (!RequireCompositor(context)) {
            return true;
        }

        uint64_t requestedCount = 3u;
        if (isDirtyPrefix) {
            const char *arg = nullptr;
            if (context.StartsWithFn(context.CommandBuffer, "desksurfdirty ")) {
                arg = context.CommandBuffer + 14;
            } else {
                arg = context.AliasArgAfterPrefixFn(context.CommandBuffer,
                                                    "desktopdirty ",
                                                    13,
                                                    "dsksurfdirty ",
                                                    13,
                                                    "windowdirty ",
                                                    12);
            }
            if (context.StrEqFn(arg, "all")) {
                requestedCount = 16u;
            } else {
                if (!context.ParseUIntFn(arg, requestedCount) || requestedCount == 0u || requestedCount > 8u) {
                    context.PushLogFn("DSKSURFDIRTY RANGE 1..8 OR ALL");
                    ClearCommandInput(context);
                    return true;
                }
            }
        }

        FDesktopDirtyContributor contributors[16] = {};
        Fortress::Core::uint32 contributorCount = 0;
        context.DesktopCompositor->GetLastFrameDirtyContributors(contributors, 16u, contributorCount);

        Fortress::Core::uint32 emitCount = static_cast<Fortress::Core::uint32>(requestedCount);
        if (emitCount > contributorCount) {
            emitCount = contributorCount;
        }

        Fortress::Core::uint32 topIndexes[8] = {};
        bool selected[16] = {};
        for (Fortress::Core::uint32 slot = 0; slot < emitCount; slot++) {
            Fortress::Core::uint32 bestIndex = 0u;
            Fortress::Core::uint64 bestPixels = 0u;
            bool found = false;
            for (Fortress::Core::uint32 i = 0; i < contributorCount; i++) {
                if (selected[i]) {
                    continue;
                }
                if (!found || contributors[i].DirtyPixels > bestPixels) {
                    found = true;
                    bestIndex = i;
                    bestPixels = contributors[i].DirtyPixels;
                }
            }

            if (!found) {
                emitCount = slot;
                break;
            }

            selected[bestIndex] = true;
            topIndexes[slot] = bestIndex;
        }

        char line[196] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "DESKTOP DIRTY TOP N ");
        context.AppendUIntFn(line, sizeof(line), pos, emitCount);
        for (Fortress::Core::uint32 slot = 0; slot < emitCount; slot++) {
            const Fortress::Core::uint32 i = topIndexes[slot];
            context.AppendStringFn(line, sizeof(line), pos, " ID ");
            context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(contributors[i].SurfaceId));
            context.AppendStringFn(line, sizeof(line), pos, " PX ");
            context.AppendUIntFn(line, sizeof(line), pos, contributors[i].DirtyPixels);
        }
        context.PushLogFn(line);
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopinspect", "dsksurfinspect", "windowinspect") ||
        context.MatchAnyPrefixFn(context.CommandBuffer, "desktopinspect ", "dsksurfinspect ", "windowinspect ")) {
        if (!RequireInputRouter(context)) {
            return true;
        }

        uint64_t parsedSurfaceId = 0;
        if (context.MatchAnyPrefixFn(context.CommandBuffer, "desktopinspect ", "dsksurfinspect ", "windowinspect ")) {
            const char *arg = context.AliasArgAfterPrefixFn(
                context.CommandBuffer, "desktopinspect ", 15, "dsksurfinspect ", 15, "windowinspect ", 14);
            if (!context.ParseUIntFn(arg, parsedSurfaceId) || parsedSurfaceId == 0u) {
                context.PushLogFn("DSKSURFINSPECT ARG INVALID");
                ClearCommandInput(context);
                return true;
            }
        } else {
            FDesktopInputRouterStats routerStats{};
            context.DesktopInputRouter->GetStats(routerStats);
            parsedSurfaceId = static_cast<uint64_t>(routerStats.FocusSurfaceId);
            if (parsedSurfaceId == 0u) {
                context.PushLogFn("DSKSURFINSPECT NO FOCUS");
                ClearCommandInput(context);
                return true;
            }
        }

        FDesktopSurfaceInputStats surfaceStats{};
        char line[128] = {};
        size_t pos = 0;
        if (context.DesktopInputRouter->GetSurfaceInputStats(static_cast<FDesktopSurfaceId>(parsedSurfaceId), surfaceStats)) {
            context.AppendStringFn(line, sizeof(line), pos, "DSKSURF ");
            context.AppendUIntFn(line, sizeof(line), pos, parsedSurfaceId);
            context.AppendStringFn(line, sizeof(line), pos, " RX ");
            context.AppendUIntFn(line, sizeof(line), pos, surfaceStats.RoutedKeyCount);
            context.AppendStringFn(line, sizeof(line), pos, " CLK ");
            context.AppendUIntFn(line, sizeof(line), pos, surfaceStats.PointerFocusClickCount);
            context.AppendStringFn(line, sizeof(line), pos, " KEY ");
            context.AppendUIntFn(line, sizeof(line), pos, surfaceStats.LastRoutedKeyAscii);
            context.PushLogFn(line);
            return true;
        }

        if (!RequireCompositor(context)) {
            return true;
        }

        FDesktopSurfaceSnapshot snapshot{};
        if (!context.DesktopCompositor->GetSurfaceSnapshot(static_cast<FDesktopSurfaceId>(parsedSurfaceId), snapshot)) {
            context.PushLogFn("DSKSURFINSPECT NO DATA");
            ClearCommandInput(context);
            return true;
        }

        context.AppendStringFn(line, sizeof(line), pos, "DSKSURF ");
        context.AppendUIntFn(line, sizeof(line), pos, parsedSurfaceId);
        context.AppendStringFn(line, sizeof(line), pos, " Z ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(snapshot.ZOrder));
        context.AppendStringFn(line, sizeof(line), pos, " V ");
        context.AppendUIntFn(line, sizeof(line), pos, snapshot.Visible ? 1u : 0u);
        context.AppendStringFn(line, sizeof(line), pos, " X ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(snapshot.Bounds.X));
        context.AppendStringFn(line, sizeof(line), pos, " Y ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(snapshot.Bounds.Y));
        context.AppendStringFn(line, sizeof(line), pos, " W ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(snapshot.Bounds.Width));
        context.AppendStringFn(line, sizeof(line), pos, " H ");
        context.AppendUIntFn(line, sizeof(line), pos, static_cast<uint64_t>(snapshot.Bounds.Height));
        context.AppendStringFn(line, sizeof(line), pos, " RX 0 CLK 0 KEY 0");
        context.PushLogFn(line);
        return true;
    }

    return false;
}

bool TryProcessCursorCommand(FKernelDesktopCursorCommandContext &context) {
    if (context.CommandBuffer == nullptr || context.CommandLength == nullptr || context.PushLogFn == nullptr ||
        context.AppendCharFn == nullptr || context.AppendStringFn == nullptr || context.AppendUIntFn == nullptr ||
        context.StrEqFn == nullptr || context.StartsWithFn == nullptr || context.ParseUIntFn == nullptr ||
        context.MatchAnyExactFn == nullptr || context.GetCursorLatencyModeNameFn == nullptr ||
        context.GetCursorLatencyModeValueFn == nullptr || context.SetCursorLatencyModeValueFn == nullptr ||
        context.CursorOverlayEnabled == nullptr || context.DesktopSurfaceOverlayEnabled == nullptr ||
        context.CursorInvertX == nullptr || context.CursorInvertY == nullptr || context.CursorSensitivityPercent == nullptr ||
        context.PublishCursorOverlaySetEventFn == nullptr || context.PublishDesktopSurfaceOverlaySetEventFn == nullptr) {
        return false;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor")) {
        char line[96] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "CURSOR ");
        context.AppendStringFn(line, sizeof(line), pos, *context.CursorOverlayEnabled ? "ON" : "OFF");
        context.AppendStringFn(line, sizeof(line), pos, " INVX ");
        context.AppendUIntFn(line, sizeof(line), pos, *context.CursorInvertX ? 1u : 0u);
        context.AppendStringFn(line, sizeof(line), pos, " INVY ");
        context.AppendUIntFn(line, sizeof(line), pos, *context.CursorInvertY ? 1u : 0u);
        context.AppendStringFn(line, sizeof(line), pos, " SENS ");
        context.AppendUIntFn(line, sizeof(line), pos, *context.CursorSensitivityPercent);
        context.AppendCharFn(line, sizeof(line), pos, '%');
        context.AppendStringFn(line, sizeof(line), pos, " LAT ");
        context.AppendStringFn(line, sizeof(line), pos, context.GetCursorLatencyModeNameFn());
        context.PushLogFn(line);
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor on")) {
        *context.CursorOverlayEnabled = true;
        context.PublishCursorOverlaySetEventFn(true);
        context.PushLogFn("CURSOR ON");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor off")) {
        *context.CursorOverlayEnabled = false;
        context.PublishCursorOverlaySetEventFn(false);
        context.PushLogFn("CURSOR OFF");
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopoverlay", "dsksurfoverlay", "windowoverlay")) {
        context.PushLogFn(*context.DesktopSurfaceOverlayEnabled ? "DSKSURF OVERLAY ON" : "DSKSURF OVERLAY OFF");
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopoverlay on", "dsksurfoverlay on", "windowoverlay on")) {
        *context.DesktopSurfaceOverlayEnabled = true;
        context.PublishDesktopSurfaceOverlaySetEventFn(true);
        context.PushLogFn("DSKSURF OVERLAY ON");
        return true;
    }

    if (context.MatchAnyExactFn(context.CommandBuffer, "desktopoverlay off", "dsksurfoverlay off", "windowoverlay off")) {
        *context.DesktopSurfaceOverlayEnabled = false;
        context.PublishDesktopSurfaceOverlaySetEventFn(false);
        context.PushLogFn("DSKSURF OVERLAY OFF");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor invertx")) {
        *context.CursorInvertX = !*context.CursorInvertX;
        context.PushLogFn(*context.CursorInvertX ? "CURSOR INVX ON" : "CURSOR INVX OFF");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor invertx on")) {
        *context.CursorInvertX = true;
        context.PushLogFn("CURSOR INVX ON");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor invertx off")) {
        *context.CursorInvertX = false;
        context.PushLogFn("CURSOR INVX OFF");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor inverty")) {
        *context.CursorInvertY = !*context.CursorInvertY;
        context.PushLogFn(*context.CursorInvertY ? "CURSOR INVY ON" : "CURSOR INVY OFF");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor inverty on")) {
        *context.CursorInvertY = true;
        context.PushLogFn("CURSOR INVY ON");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor inverty off")) {
        *context.CursorInvertY = false;
        context.PushLogFn("CURSOR INVY OFF");
        return true;
    }

    if (context.StartsWithFn(context.CommandBuffer, "cursor sens ")) {
        uint64_t sensitivity = 0;
        if (!context.ParseUIntFn(context.CommandBuffer + 12, sensitivity)) {
            context.PushLogFn("CURSOR SENS ARG INVALID");
        } else if (sensitivity < 25 || sensitivity > 400) {
            context.PushLogFn("CURSOR SENS RANGE 25..400");
        } else {
            *context.CursorSensitivityPercent = static_cast<uint32_t>(sensitivity);
            char line[64] = {};
            size_t pos = 0;
            context.AppendStringFn(line, sizeof(line), pos, "CURSOR SENS ");
            context.AppendUIntFn(line, sizeof(line), pos, *context.CursorSensitivityPercent);
            context.AppendCharFn(line, sizeof(line), pos, '%');
            context.PushLogFn(line);
        }
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor latency")) {
        char line[64] = {};
        size_t pos = 0;
        context.AppendStringFn(line, sizeof(line), pos, "CURSOR LAT ");
        context.AppendStringFn(line, sizeof(line), pos, context.GetCursorLatencyModeNameFn());
        context.PushLogFn(line);
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor latency smooth")) {
        context.SetCursorLatencyModeValueFn(0u);
        context.PushLogFn("CURSOR LAT SMOOTH");
        return true;
    }

    if (context.StrEqFn(context.CommandBuffer, "cursor latency responsive")) {
        context.SetCursorLatencyModeValueFn(1u);
        context.PushLogFn("CURSOR LAT RESP");
        return true;
    }

    return false;
}

} // namespace Fortress::Kernel
