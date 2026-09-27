# Windowing Terminology Glossary

Status: Active
Scope: User-facing command language and internal architecture terms for Fortress desktop rendering and input routing.

## Terms

- Window (user-facing): the UI object users think about and operate on.
- Surface (internal): the compositor-managed render/input unit with bounds, z-order, visibility, dirty state, and IDs.
- Compositor: the engine that tracks surfaces and renders them with dirty-region and occlusion rules.
- Window Manager (policy): logic that decides focus, raise, capture, and interaction policy.
- Shell: high-level UX/control behavior layered on top of compositor and policy.
- Overlay HUD: diagnostic control surface rendered last for deterministic visibility.

## Naming Rule

- User-facing commands may use WINDOW* aliases for clarity.
- Internal contracts and telemetry remain surface-based for determinism and compatibility.
- Existing DESKTOP* and DSKSURF* commands remain valid during migration.

## Command Alias Intent

- WINDOWCREATE maps to DSKSURFCREATE semantics.
- WINDOWMOVE, WINDOWRESIZE, WINDOWSHOW, WINDOWHIDE, WINDOWDAMAGE, WINDOWCLOSE map to corresponding surface lifecycle commands.
- WINDOWFOCUS, WINDOWCAPTURE, WINDOWLIST, WINDOWINSPECT, WINDOWDIRTY, WINDOWRAISE, WINDOWSTAT, WINDOWOVERLAY map to existing desktop/surface control behavior.
