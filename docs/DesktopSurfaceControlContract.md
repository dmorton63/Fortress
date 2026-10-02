# Desktop Surface Control Contract (M1 Freeze)

Status: Draft freeze candidate
Scope: Desktop surface lifecycle, ordering, visibility, focus/capture, damage, diagnostics controls
Naming rule: Use desktop surface terminology; avoid window terminology in internal contracts.
Companion validation matrix: docs/DesktopSurfaceControlValidationMatrix.md

## 1) Surface Model

- Surface ID: uint32, 0 is invalid.
- Root surface: internal parent for top-level surfaces.
- Visibility: visible or hidden; hidden surfaces are excluded from hit testing and rendering.
- Ordering: integer Z order; larger value means visually above smaller value.
- Dirty state: each surface may have a bounded dirty region (clipped to surface bounds).
- Focus: at most one focus surface, optional.
- Capture: at most one capture surface, optional; capture overrides pointer hit-testing target.

## 2) Global Invariants

- IDs are never reused during a boot session.
- Surface bounds width and height are strictly positive.
- Surface operations on invalid or unknown IDs must fail with no side effects.
- Hidden surfaces must not receive pointer focus clicks by hit testing.
- Focus and capture IDs must always be either invalid or existing surfaces.
- Damage is clipped to desktop and surface bounds before accumulation.
- Consume dirty operation clears dirty state for that surface in the same frame.

## 3) Control Contract Table

## Create

- Command shape: DSKSURFCREATE X Y W H Z
- Args: X,Y int32; W,H int32; Z uint32
- Validation:
  - W >= 1, H >= 1
  - surface capacity not exceeded
- State transition:
  - allocates new surface
  - sets bounds and Z
  - visible=true
  - marks initial bounds dirty
- Telemetry deltas:
  - SurfaceCount +1
  - DirtySurfaceCount may +1
  - DirtyPixelArea increases by clipped area
- Fail reasons:
  - INVALID_SIZE
  - CAPACITY_EXCEEDED

## Close

- Command shape: DSKSURFCLOSE ID
- Args: ID uint32
- Validation: surface exists and ID != 0
- State transition:
  - surface removed
  - if focused, focus moves to next focusable or invalid
  - if captured, capture cleared
- Telemetry deltas:
  - SurfaceCount -1
  - Focus/Capture change counters may increment
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND

## Move

- Command shape: DSKSURFMOVE ID X Y
- Args: ID uint32; X,Y int32
- Validation: surface exists
- State transition:
  - bounds X,Y updated
  - old and new bounds contribute to dirty accumulation
- Telemetry deltas:
  - DirtySurfaceCount may +1
  - DirtyPixelArea increases
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND

## Resize

- Command shape: DSKSURFRESIZE ID W H
- Args: ID uint32; W,H int32
- Validation: surface exists; W >= 1; H >= 1
- State transition:
  - bounds width/height updated
  - old and new bounds contribute to dirty accumulation
- Telemetry deltas:
  - DirtySurfaceCount may +1
  - DirtyPixelArea increases
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND
  - INVALID_SIZE

## Show

- Command shape: DSKSURFSHOW ID
- Args: ID uint32
- Validation: surface exists
- State transition:
  - visible=true
  - surface bounds marked dirty
- Telemetry deltas:
  - DirtySurfaceCount may +1
  - DirtyPixelArea increases
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND

## Hide

- Command shape: DSKSURFHIDE ID
- Args: ID uint32
- Validation: surface exists
- State transition:
  - visible=false
  - uncovered region marked dirty
  - if captured by this ID, capture cleared
- Telemetry deltas:
  - DirtySurfaceCount may +1
  - DirtyPixelArea increases
  - CaptureChangeCount may +1
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND

## Raise

- Command shape: DSKSURFRAISE ID
- Args: ID uint32
- Validation: surface exists
- State transition:
  - assigns top Z order
  - old/new coverage considered dirty
- Telemetry deltas:
  - HighestZOrder may increase
  - DirtyPixelArea may increase
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND

## Focus

- Command shape: DSKSURFFOCUS ID | DSKSURFFOCUS NEXT
- Args: ID uint32 or NEXT keyword
- Validation:
  - target exists and focusable when ID path
  - NEXT requires at least one focusable surface
- State transition:
  - FocusSurfaceId updated
  - focus change counter incremented when changed
- Telemetry deltas:
  - FocusChangeCount +1 on change
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND
  - NO_FOCUSABLE_SURFACE

## Capture

- Command shape: DSKSURFCAPTURE ID | DSKSURFCAPTURE OFF
- Args: ID uint32 or OFF keyword
- Validation: ID exists for capture set
- State transition:
  - CaptureSurfaceId set to ID or invalid
  - capture change counter increments on change
- Telemetry deltas:
  - CaptureChangeCount +1 on change
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND

## Damage

- Command shape: DSKSURFDAMAGE ID
- Args: ID uint32
- Validation: surface exists
- State transition:
  - full surface bounds marked dirty
  - dirty accumulation uses union and clipping
- Telemetry deltas:
  - DirtySurfaceCount may +1
  - DirtyPixelArea increases
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND

## Dirty Inspect

- Command shape: DSKSURFDIRTY [N|ALL]
- Args: optional N (1..8) or ALL
- Validation: argument in range or ALL
- State transition: none
- Telemetry deltas: none
- Fail reasons:
  - INVALID_ARG_RANGE

## List

- Command shape: DSKSURFLIST
- Args: none
- Validation: none
- State transition: none
- Telemetry deltas: none

## Inspect

- Command shape: DSKSURFINSPECT [ID]
- Args: optional ID, default focused surface
- Validation: focused surface exists if no ID
- State transition: none
- Telemetry deltas: none
- Fail reasons:
  - INVALID_ID
  - NOT_FOUND
  - NO_FOCUS

## Overlay Toggle

- Command shape: DSKSURFOVERLAY ON|OFF
- Args: ON/OFF
- Validation: valid keyword
- State transition:
  - render pass enable state toggled via control plane
- Telemetry deltas:
  - overlay render counters behavior changes by mode
- Fail reasons:
  - INVALID_ARG

## 4) Render/Dirty Semantics

- Normal path: compositor consumes dirty region per visible surface, repaints clipped dirty fragment(s), then acknowledges consumed dirty.
- Fallback path: when no dirty region is available, repaint falls back to clipped surface bounds for safety.
- Occlusion path: repaint fragments are split by higher Z visible surfaces before drawing.
- Contributor reporting:
  - Top dirty contributors: by drawn dirty pixels in frame.
  - Top split contributors: by split attempts in frame.

## 5) Surface Content Host Contract (Batch 35)

- Registration:
  - Each surface may register one content contract with:
    - Render callback: optional host-owned content draw hook.
    - Input callback: optional control event notification hook.
  - Runtime initializes host after compositor/input router readiness and registers baseline controls for managed desktop surfaces.
- Control model:
  - Fixed-capacity control array per surface.
  - Node fields include: control ID, control type (label/button), local bounds, visible/enabled/focusable flags, focused/pressed state.
- Focus model:
  - Focus traversal is scoped to the currently focused desktop surface.
  - Next traversal key: Tab.
  - Previous traversal key: Ctrl+K (ASCII 11) for deterministic non-layout-dependent reverse traversal.
  - If no focusable control is set when a surface receives focus, host assigns first visible+enabled+focusable control.
- Pointer model:
  - Pointer press resolves top-most enabled+visible control inside focused/hit surface local bounds.
  - Button controls toggle pressed state on press.
  - Control press updates focus when target is focusable.
- Dirty/Redraw model:
  - Control invalidation uses compositor local damage helper:
    - `MarkSurfaceDamagedLocal(surfaceId, localRect)`
  - Local rect is translated into desktop/global coordinates then merged through existing dirty union and clipping.
  - Host emits validation log `CTRL REDRAW` when local damage is accepted.
- Rendering:
  - Overlay calls host per visible snapshot after base surface fill.
  - Label/button visuals are deterministic solid fills + frames.
  - Focused button frame color changes only when owning surface is focused.

## 6) Control Diagnostics and Validation Logs

- Command:
  - `DSKSURFCONTROLS [ID]` (aliases: `DESKTOPCONTROLS`, `WINDOWCONTROLS`)
  - No ID: inspect focused surface.
  - With ID: inspect specified surface.
- Output summary:
  - Surface ID, control count, focused control ID.
- Output rows:
  - Per-control: control ID, type, local bounds, focused flag, pressed flag.
- Validation logs:
  - `CTRL FOCUS`: focus assignment/traversal.
  - `CTRL CLICK`: pointer click routing/hit-test result.
  - `CTRL KEY`: key activation path on focused control.
  - `CTRL REDRAW`: control-local dirty invalidation accepted.

## 5) Telemetry Contract

Periodic desktop diagnostics must include:

- DESKTOP S with:
  - Surface count
  - Dirty count
  - Dirty area
  - Dirty ack count and pixels (cumulative)
  - Frame ack count and pixels
  - Frame fallback count and pixels
  - Frame split attempts, fragments generated, fragments dropped
  - Highest Z
  - Shell component count and ticks
  - Focus and capture IDs
  - Pointer click count
- DESKTOP DIRTY TOP N ... entries
- DESKTOP SPLIT TOP N ... entries
- DESKTOP INPUT RX ... entries

## 6) Failure and Logging Policy

- Every failed control emits a single deterministic failure line with reason token.
- Failure lines must include control name and ID when applicable.
- Success lines include enough state to reconstruct transitions (for example ID and coordinates on move).

## 7) Freeze Acceptance Checklist

- All controls above exist and parse deterministically.
- All fail reasons are mapped and covered in tests/log checks.
- No control violates global invariants under randomized command bursts.
- Run-log marker checks pass after a control stress sequence.
- Contract review signed off before adding new desktop surface feature flags.
