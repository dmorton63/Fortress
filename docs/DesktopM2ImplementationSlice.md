# Desktop M2 Implementation Slice (10 Tasks)

Status: Proposed
Scope: Move from validated surface infrastructure (M1) to a usable desktop shell policy and UX layer without breaking deterministic telemetry.

## Exit Criteria

- Keyboard and pointer users can create, focus, move, resize, and close desktop surfaces through deterministic control paths.
- Desktop policy behavior is explicit (focus, raise, capture, visibility transitions).
- Surface chrome rendering exists (active/inactive titlebar, border, grip hints) and preserves partial redraw accounting.
- Existing checks remain green:
  - make run-log-check
  - make dsksurf-contract-occlusion-strict-check
  - make dsksurf-contract-fallback-strict-check

## Task 1: Shell Policy Module Extraction

- Goal: Separate desktop policy decisions from compositor mechanics.
- Primary modules:
  - src/Kernel/FDesktopRuntime.cpp
  - include/Fortress/Kernel/FDesktopRuntime.hpp
- New modules:
  - include/Fortress/Kernel/FDesktopShellPolicy.hpp
  - src/Kernel/FDesktopShellPolicy.cpp
- Deliverable:
  - Policy interface for focus-on-click, raise-on-focus, capture release rules.
- Validation:
  - make -j4
  - make run-log-check

## Task 2: Focus and Raise Contract Wiring

- Goal: Enforce explicit focus/raise behavior via policy callbacks.
- Primary modules:
  - src/Kernel/FDesktopInputRouter.cpp
  - src/Kernel/FDesktopCompositor.cpp
  - src/Kernel/FDesktopRuntime.cpp
- Deliverable:
  - On pointer focus action: focus changes are deterministic, optional raise happens through policy.
- Validation:
  - make dsksurf-contract-check
  - make dsksurf-contract-token-check

## Task 3: Move/Resize Interaction State Machine

- Goal: Add deterministic drag state for move/resize interactions.
- Primary modules:
  - include/Fortress/Kernel/FDesktopInputRouter.hpp
  - src/Kernel/FDesktopInputRouter.cpp
- Deliverable:
  - BeginDrag/UpdateDrag/EndDrag states with resize edge selection and cancel path.
- Validation:
  - make run-log-check
  - Manual command replay with desktopmove/desktopresize and deterministic logs.

## Task 4: Desktop Surface Chrome Renderer

- Goal: Render shell chrome for active/inactive surfaces (title bar, border, grip hints).
- Primary modules:
  - src/Kernel/FDesktopSurfaceOverlay.cpp
  - include/Fortress/Kernel/FDesktopSurfaceOverlay.hpp
- Deliverable:
  - Visual distinction for focused/captured states without regressing dirty-first behavior.
- Validation:
  - make dsksurf-contract-occlusion-check
  - make dsksurf-contract-fallback-strict-check

## Task 5: Surface Role Metadata (Client, Shell, HUD)

- Goal: Add typed roles so policy and rendering can branch safely.
- Primary modules:
  - include/Fortress/Kernel/FDesktopCompositor.hpp
  - src/Kernel/FDesktopCompositor.cpp
- Deliverable:
  - Role enum on snapshots; role-aware list/inspect output.
- Validation:
  - make dsksurf-contract-check
  - Verify DSKSURFLIST and DSKSURFINSPECT include role fields.

## Task 6: Desktop Command UX v2

- Goal: Improve command ergonomics for real shell operations.
- Primary modules:
  - src/Kernel/FKernelCommandConsole.cpp
- Deliverable:
  - Human-readable success/failure lines include ID, coordinates, dimensions, and reason tokens.
  - Deterministic one-line failure policy preserved.
- Validation:
  - make dsksurf-contract-token-check
  - make run-log-check

## Task 7: Contract Matrix Expansion (M2 Rows)

- Goal: Add policy/interaction checks to the matrix.
- Primary docs:
  - docs/DesktopSurfaceControlValidationMatrix.md
- Deliverable:
  - New rows for drag transitions, raise-on-focus policy, role-specific behavior, and chrome redraw expectations.
- Validation:
  - Matrix rows map to existing or new checker flags/targets.

## Task 8: Checker Profile Expansion for Policy Signals

- Goal: Add strict profile(s) for focus/raise/capture transitions.
- Primary modules:
  - tools/desktop_surface_contract_smoke_check.sh
  - Makefile
- Deliverable:
  - New geometry/policy profile (for example boot-policy-v1) that asserts focus/capture counters and target IDs in summary lines.
- Validation:
  - make dsksurf-contract-check
  - New strict target passes on fresh log.

## Task 9: Deterministic Replay Script for M2

- Goal: Create scripted command burst that exercises M2 behavior repeatably.
- Primary modules:
  - tools/desktop_surface_contract_smoke.sh
  - optional new helper in tools/
- Deliverable:
  - Two-pass replay producing stable signatures for policy + rendering telemetry.
- Validation:
  - Existing compare signature mode passes across both runs.

## Task 10: M2 Freeze Gate and Handoff

- Goal: Define merge gate before deeper desktop/app work.
- Primary docs:
  - docs/DesktopM2ImplementationSlice.md
  - docs/DesktopSurfaceControlContract.md
- Deliverable:
  - Checklist: all strict profiles green, no unknown fail tokens, and stable contributor ordering in deterministic run.
- Validation:
  - make run-log-check
  - make dsksurf-contract-token-check
  - make dsksurf-contract-occlusion-strict-check
  - make dsksurf-contract-fallback-strict-check

## Suggested Execution Order

1. Task 1
2. Task 2
3. Task 3
4. Task 4
5. Task 5
6. Task 6
7. Task 7
8. Task 8
9. Task 9
10. Task 10

## Risk Notes

- Main risk: mixing shell policy logic into compositor internals.
- Mitigation: keep policy decisions in a separate module and keep compositor as state + geometry engine.
- Main regression risk: chrome rendering inflates fallback/dirty counters.
- Mitigation: preserve dirty-first repaint and hold strict profile gates for occlusion/fallback.
