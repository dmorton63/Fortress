# TODO_MAIN

Batch 1 focuses on subsystem progress outside USB/xHCI, aligned to Desktop Composition (Phase 5) and Platform Services/Security (Phase 6).

## Batch 1 (10 items)

1. [x] Add desktop surface IDs and parent-child linking API in Desktop Surface Manager.
2. [x] Implement stable z-order list maintenance with insert/move/remove operations.
3. [x] Add compositor damage-region tracking for per-surface invalidation and frame coalescing.
4. [x] Implement focus manager with single active surface and explicit focus transfer rules.
5. [x] Add pointer hit-testing against surface bounds with z-order precedence.
6. [x] Route keyboard input only to focused surface through event dispatch contracts.
7. [x] Introduce Desktop Shell component registration for pluggable surface producers.
8. [x] Add service registry adapter skeleton for Citadel-compatible service metadata.
9. [x] Implement initial Port Manager default-deny table with open/close lease primitives.
10. [x] Add audit log records for denied port access and lease lifecycle events.

## Validation targets

- Build remains green with make.
- Existing desktop smoke flows continue to pass.
- New logs added for focus changes and denied port operations.

## Batch 2 (10 items)

1. [x] Expose compositor z-ordered surface enumeration API for deterministic diagnostics.
2. [x] Add compositor helper to count child surfaces by parent for tree validation.
3. [x] Add desktop stats logging for coalesced dirty-region area.
4. [x] Add desktop input stats logging for focus-reject and capture state drift.
5. [x] Add input-router stale capture cleanup when captured surface becomes non-focusable.
6. [x] Extend service registry database adapter with runtime diagnostics counters.
7. [x] Add service registry adapter health log line in runtime diagnostics.
8. [x] Add port manager helpers to query latest audit and latest denied audit events.
9. [x] Add port manager policy/audit log line in runtime diagnostics.
10. [x] Add bootstrap default-deny policy probes for multiple services/ports.

## Batch 3 (10 items)

1. [x] Add utility command for service registry database adapter stats (`srdbstat`).
2. [x] Add utility command to find service-db record by service ID (`srdbfind <id>`).
3. [x] Add utility command for port policy summary stats (`portstat`).
4. [x] Add utility command for latest port audit entry (`portaudit last`).
5. [x] Add utility command for latest denied port audit entry (`portaudit denied`).
6. [x] Add utility command for explicit access-check probe (`portcheck <port> <service>`).
7. [x] Add utility command for desktop z-order listing (`dskzlist`).
8. [x] Add utility command for desktop child-surface listing (`dskchildren <parent>`).
9. [x] Wire all new utility command callbacks through command console context validation.
10. [x] Update built-in help text to document new subsystem commands.

## Batch 4 (10 items)

1. [x] Add port-manager API to enumerate registered ports with lease snapshots.
2. [x] Add port-manager API to query port name and active lease by port ID.
3. [x] Add utility command to list ports and lease state (`portlist`).
4. [x] Add utility command to open command-console lease (`portopen <port>`).
5. [x] Add utility command to close command-console lease (`portclose <port>`).
6. [x] Add utility command to inspect single-port lease (`portlease <port>`).
7. [x] Add command-console local lease cache for safe close/retry flow.
8. [x] Enforce port-policy gate for `vfsresolve` on `/boot` path.
9. [x] Enforce port-policy gate for `logsave` on `/boot` mount path.
10. [x] Add explicit deny guidance log message with remediation hint (`PORTOPEN <id>`).
