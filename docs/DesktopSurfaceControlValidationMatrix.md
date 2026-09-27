# Desktop Surface Control Validation Matrix (M1)

Status: Ready for implementation
Companion to: docs/DesktopSurfaceControlContract.md
Purpose: Convert contract requirements into deterministic spec-to-check rows for command, state, and telemetry validation.

## 1) Surface Lifecycle and Invariants

| ID | Area | Spec | Test Procedure | Assertions | Telemetry Checks | Failure Token |
| --- | --- | --- | --- | --- | --- | --- |
| L1 | Create | DSKSURFCREATE with valid rect, parent, Z creates a visible dirty surface. | Run create command with valid X,Y,W,H,Z. | SurfaceExists(id)=true. Snapshot matches bounds, Z, visible=true, dirty=true. | DESKTOP S: SurfaceCount +1; Dirty count increments; Dirty area increases by clipped area. | INVALID_SIZE, CAPACITY_EXCEEDED |
| L2 | Close | DSKSURFCLOSE on existing non-root removes the surface. | Run close command on non-root id. | SurfaceExists(id)=false. Future inspect/snapshot for id fails. | DESKTOP S: SurfaceCount -1. | ERR_DSKSURF_CLOSE_ROOT for root close; NOT_FOUND for missing ID. |
| L3 | Invariants | Invalid/unknown IDs cause no side effects. | Run close/move/resize/show/hide on unknown id. | Operation fails; no snapshot deltas on existing surfaces. | DESKTOP S unchanged except deterministic failure line count. | INVALID_ID or NOT_FOUND |

## 2) Geometry and Visibility Commands

| ID | Area | Spec | Test Procedure | Assertions | Telemetry Checks | Failure Token |
| --- | --- | --- | --- | --- | --- | --- |
| G1 | Move | DSKSURFMOVE updates bounds X/Y and marks dirty. | Move target to known X,Y. | Snapshot bounds updated. Dirty=true. DirtyRect covers old/new exposure region (or full bounds minimum). | Dirty area increases as expected for movement scenario. | INVALID_ID, NOT_FOUND |
| G2 | Resize | DSKSURFRESIZE with positive dimensions updates bounds and marks dirty. | Resize target to W,H > 0. | Snapshot width/height match. Dirty=true. | Dirty area increases. | ERR_DSKSURF_RESIZE_INVALID (or INVALID_SIZE), INVALID_ID, NOT_FOUND |
| G3 | Show | DSKSURFSHOW sets visible=true and marks dirty. | Hide then show same id. | Visible toggles to true. Dirty=true. DirtyRect intersects bounds. | Dirty count/area reflect repaint request. | INVALID_ID, NOT_FOUND |
| G4 | Hide | DSKSURFHIDE sets visible=false and marks uncovered region dirty. | Show then hide same id. | Visible toggles to false. Dirty=true for exposed background/underlap. | Dirty count/area reflect exposure redraw. | INVALID_ID, NOT_FOUND |

## 3) Z-Order and Focus

| ID | Area | Spec | Test Procedure | Assertions | Telemetry Checks | Failure Token |
| --- | --- | --- | --- | --- | --- | --- |
| Z1 | Raise | DSKSURFRAISE moves target to top Z and marks dirty. | Raise target under known stack. | Snapshot ZOrder is greater than all peers. | DESKTOP S highest Z updated; dirty area may increase. | INVALID_ID, NOT_FOUND |
| Z2 | Focus | DSKSURFFOCUS sets focus to target surface. | Focus on specific id, then inspect focus id. | GetFocusableSurfaceId()==id. | Optional focus marker appears if enabled; focus change counter increments only on change. | INVALID_ID, NOT_FOUND, NO_FOCUSABLE_SURFACE |

## 4) Damage and Dirty Semantics

| ID | Area | Spec | Test Procedure | Assertions | Telemetry Checks | Failure Token |
| --- | --- | --- | --- | --- | --- | --- |
| D1 | Damage | DSKSURFDAMAGE clips to bounds and accumulates dirty union. | Issue repeated damage events with overlapping/out-of-bounds regions. | Peek dirty region equals clipped union. Dirty=true. | Dirty area equals expected clipped union area; contributor entry exists for surface. | INVALID_ID, NOT_FOUND |
| D2 | Dirty peek/consume | Peek reports current dirty; consume clears and increments acknowledge counters. | Run DSKSURFDIRTY inspect, then render/consume path once. | After consume: Dirty=false for consumed region. | DirtyAcknowledgeCount +1 and DirtyAcknowledgePixels += consumed area; DESKTOP S reflects updated cumulative counters. | INVALID_ARG_RANGE |
| D3 | Fallback repaint | If no dirty region exists, repaint falls back to clipped bounds. | Force frame with surface visible but no dirty mark. | Frame still repaints safely; no stale pixels. | Frame fallback counters increment. | N/A |

## 5) Occlusion Splitting and Overdraw Reduction

| ID | Area | Spec | Test Procedure | Assertions | Telemetry Checks | Failure Token |
| --- | --- | --- | --- | --- | --- | --- |
| O1 | Overlap subtraction | Lower-Z dirty regions are split by higher-Z visible bounds; out-of-desktop fragments dropped. | Build deterministic 3-surface overlap scenario and trigger redraw. | Split attempts > 0. Fragment count equals expected geometry. Clipped-away fragment count equals expected out-of-bounds portions. | DESKTOP S counters: RSPLIT, RFRAG, RCLIP match expected counts. | N/A |
| O2 | Contributor ranking | Top contributors reflect post-occlusion redraw cost. | Run deterministic move/resize/damage burst with fixed seed/order. | GetLastFrameDirtyContributors rank order matches expected hot surfaces. | Fallback pixels lower than pre-occlusion baseline run. | N/A |

## 6) List, Inspect, Overlay

| ID | Area | Spec | Test Procedure | Assertions | Telemetry Checks | Failure Token |
| --- | --- | --- | --- | --- | --- | --- |
| C1 | List | DSKSURFLIST returns all active non-root surfaces. | Create known set, run list command. | Listed IDs equal GetActiveSurfaceIds minus root. | Optional count in list output matches DESKTOP S surface count relationship. | N/A |
| C2 | Inspect | DSKSURFINSPECT prints snapshot fields. | Run inspect for known id. | Output fields match GetSurfaceSnapshot (bounds, Z, visible, dirty). | N/A | INVALID_ID, NOT_FOUND, NO_FOCUS |
| C3 | Overlay | DSKSURFOVERLAY ON/OFF toggles HUD visibility/participation. | Toggle ON then OFF around a frame. | HUD surface visible state toggles as expected. | Overlay-related render counters change by mode; dirty behavior remains deterministic. | INVALID_ARG |

## 7) Freeze Acceptance Mapping (M1)

| ID | Checklist Item | Spec-to-Check Row(s) | Deterministic Pass Criteria |
| --- | --- | --- | --- |
| F1 | No root surface close allowed | L2 | Close root fails with ERR_DSKSURF_CLOSE_ROOT and root snapshot unchanged. |
| F2 | All controls parse deterministically | L1-L3, G1-G4, Z1-Z2, D1-D3, C1-C3 | Same command corpus yields identical success/fail outcomes across two runs. |
| F3 | All fail reasons mapped and covered | All rows with failure token | Every expected failure token appears exactly once per failed control invocation. |
| F4 | Invariants hold under stress bursts | L3, D1, O1, O2 | Randomized burst does not violate bounds/ID/focus/capture invariants. |
| F5 | Run-log marker checks pass after stress | O1, O2, D2, D3 | DESKTOP S and contributor markers satisfy existing run-log-check requirements. |
| F6 | Deterministic telemetry | O2 plus full command replay | Two identical stress runs produce identical DESKTOP S metrics and contributor sets. |

## 8) Harness Notes

- Use one deterministic script for baseline and one for stress.
- Fix random seed, command order, and frame count.
- Capture serial logs to build/qemu-serial.log and assert counters via run-log-check plus explicit metric parsing.
- Keep one failure log line per failed control invocation.
- Prefer contract token names exactly; if legacy tokens differ, add explicit alias mapping in test parser.
