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

## Batch 5 (10 items)

1. [x] Add VFS mount snapshot struct for runtime mount introspection.
2. [x] Add VFS API to enumerate active mounts with read-only and driver metadata.
3. [x] Add utility command callback hook for mount listing (`vfsmounts`).
4. [x] Add utility command callback hook for slash-free mount-path resolve (`vfsmresolve <mount> <leaf>`).
5. [x] Add utility command callback hook for slash-free mount-block resolve (`vfsmblk <mount> <index>`).
6. [x] Add utility command callback hook for block digest probe (`vfsblkdigest <mount> <index>`).
7. [x] Add utility command callback hook for mount-targeted log persistence (`logsavemount <mount> <start> <count>`).
8. [x] Implement command-console mount-token path builder with token validation.
9. [x] Implement command-console block digest log output (`bytes/sum/xor/first-byte`) for quick storage validation.
10. [x] Update utility help text and command registry wiring for the new storage workflow commands.

## Batch 6 (10 items)

1. [x] Add spaced alias for VFS stats (`vfs stat`) alongside `vfsstat`.
2. [x] Add spaced alias for mount listing (`vfs mounts`) alongside `vfsmounts`.
3. [x] Add spaced alias for absolute resolve (`vfs resolve /abs/path`) alongside `vfsresolve`.
4. [x] Add spaced alias for boot root resolve (`vfs boot0`) alongside `vfsboot0`.
5. [x] Add spaced alias for boot block resolve (`vfs bootblk <index>`) alongside `vfsbootblk`.
6. [x] Add spaced alias for boot leaf resolve (`vfs boot <name>`) alongside `vfsboot`.
7. [x] Add spaced alias for mount token resolve (`vfs mresolve <mount> <leaf>`) alongside `vfsmresolve`.
8. [x] Add spaced alias for mount block resolve (`vfs mblk <mount> <index>`) alongside `vfsmblk`.
9. [x] Add spaced alias for mount block digest (`vfs blkdigest <mount> <index>`) alongside `vfsblkdigest`.
10. [x] Add spaced alias for mount log persistence (`log savemount <mount> <start> <count>`) alongside `logsavemount` and update help text.

## Batch 7 (10 items)

1. [x] Extend `vfs resolve` to continue supporting absolute paths unchanged.
2. [x] Add `vfs resolve bootblk` alias to resolve `/boot/blk`.
3. [x] Add `vfs resolve bootblk <index>` alias to resolve `/boot/blk/<index>`.
4. [x] Add `vfs resolve boot0` alias to resolve `/boot/blk/0`.
5. [x] Add `vfs resolve boot <name>` alias to resolve `/boot/<name>`.
6. [x] Add `vfs resolve mblk <mount> <index>` alias for mount-token block resolve.
7. [x] Add `vfs resolve mresolve <mount> <leaf>` alias for mount-token path resolve.
8. [x] Add targeted argument-count diagnostics for new `vfs resolve` alias forms.
9. [x] Keep `vfs resolve` invalid-token path explicit with `VFS RESOLVE ARG INVALID`.
10. [x] Update help text with all new `vfs resolve` alias forms.

## Batch 8 (10 items)

1. [x] Add `vfs resolve bootblk/INDEX` shorthand alias.
2. [x] Add `vfs resolve bootblk:INDEX` shorthand alias.
3. [x] Add `vfs resolve boot/NAME` shorthand alias.
4. [x] Add compact `vfs resolve mblk MOUNT:INDEX` form.
5. [x] Add compact `vfs resolve mresolve MOUNT:LEAF` form.
6. [x] Keep explicit argument validation for compact mount forms.
7. [x] Add spaced top-level alias `log saveboot START COUNT`.
8. [x] Add spaced top-level alias `log save /MOUNT START COUNT`.
9. [x] Extend help text for new resolve shorthand aliases.
10. [x] Extend help text for spaced top-level log aliases.

## Batch 9 (10 items)

1. [x] Add parser helper for packed numeric range tokens with `.`, `:`, or `/` delimiters.
2. [x] Extend `logsave` to accept `/MOUNT START.COUNT` compact form.
3. [x] Extend `logsave` to accept `/MOUNT START:COUNT` compact form.
4. [x] Extend `logsave` to accept `/MOUNT START/COUNT` compact form.
5. [x] Extend `log save` alias path to inherit compact range support via `RunLogSave`.
6. [x] Extend `logsaveboot` to accept `START.COUNT` compact form.
7. [x] Extend `log saveboot` alias path to inherit compact range support via `RunLogSaveBoot`.
8. [x] Extend `logsavemount` to accept `MOUNT START.COUNT` compact form.
9. [x] Extend `log savemount` to accept `MOUNT START.COUNT` compact form.
10. [x] Update help text for compact-range log-save aliases.

## Batch 10 (10 items)

1. [x] Extend `vfsmresolve` to accept compact `MOUNT:LEAF` form.
2. [x] Extend `vfs mresolve` to accept compact `MOUNT:LEAF` form.
3. [x] Extend `vfsmblk` to accept compact `MOUNT:INDEX` form.
4. [x] Extend `vfs mblk` to accept compact `MOUNT:INDEX` form.
5. [x] Extend `vfsblkdigest` to accept compact `MOUNT:INDEX` form.
6. [x] Extend `vfs blkdigest` to accept compact `MOUNT:INDEX` form.
7. [x] Add compact `vfsbootblk/INDEX` and `vfsbootblk:INDEX` forms.
8. [x] Add compact `vfs bootblk/INDEX` and `vfs bootblk:INDEX` forms.
9. [x] Preserve explicit usage/validation diagnostics for all compact forms.
10. [x] Update help text with direct-command compact-token aliases.

## Batch 11 (10 items)

1. [x] Add utility callback hook for multi-block digest by mount token.
2. [x] Add `vfsdigestrange <mount> <start> <count>` command.
3. [x] Add `vfs digestrange <mount> <start> <count>` alias.
4. [x] Add packed range token support (`start.count`, `start:count`, `start/count`) for digest-range commands.
5. [x] Add digest-range count validation (`1..16`) to keep runtime logs bounded.
6. [x] Implement per-block digest output (`SUM/XOR/B0`) for each block in range.
7. [x] Implement digest-range summary output with aggregate bytes/sum/xor.
8. [x] Add explicit read-fail diagnostics with failing block index.
9. [x] Wire digest-range callback through command-console utility context.
10. [x] Update help text for digest-range commands and packed-range alias forms.

## Batch 12 (10 items)

1. [x] Add utility callback hook for block digest comparison by mount token.
2. [x] Add `vfsdigestcmp <mount> <left> <right>` command.
3. [x] Add `vfs digestcmp <mount> <left> <right>` alias.
4. [x] Add packed compare pair support (`left.right`, `left:right`, `left/right`).
5. [x] Implement mount-token path mapping for left and right block targets.
6. [x] Add explicit read-fail diagnostics for left and right block reads.
7. [x] Emit compare summary line with equality bit and left/right byte counts.
8. [x] Emit compare detail line with left/right `SUM/XOR` plus differing-byte count.
9. [x] Emit first-difference indicator (byte offset or `SIZE` mismatch marker).
10. [x] Update help text and utility context wiring for digest-compare commands.

## Batch 13 (10 items)

1. [x] Add utility callback hook for block digest scan by mount token.
2. [x] Add `vfsdigestscan <mount> <start> <count>` command.
3. [x] Add `vfs digestscan <mount> <start> <count>` alias.
4. [x] Add packed scan range support (`start.count`, `start:count`, `start/count`).
5. [x] Add digest-scan count validation (`1..64`) for bounded runtime output.
6. [x] Implement scan header line with mount/start/count context.
7. [x] Implement scan read-fail diagnostics that include failing block index.
8. [x] Implement scan summary with non-zero block count (`NZ`).
9. [x] Implement scan summary with first non-zero block index (`FIRSTNZ` or `NONE`).
10. [x] Update help text and utility context wiring for digest-scan commands.

## Batch 14 (10 items)

1. [x] Add utility callback hook for non-zero digest locator by mount token.
2. [x] Add `vfsdigestnz <mount> <start> <count>` command.
3. [x] Add `vfs digestnz <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-nonzero commands.
5. [x] Add digest-nonzero count validation (`1..128`) for bounded scans.
6. [x] Implement digest-nonzero header line with mount/start/count context.
7. [x] Implement digest-nonzero read-fail diagnostics with failing block index.
8. [x] Emit per-block digest lines only for non-zero blocks (`SUM/XOR/B0`).
9. [x] Emit digest-nonzero summary with `NZ`, emitted-line count, and suppressed-line count.
10. [x] Update help text and utility context wiring for digest-nonzero commands.

## Batch 15 (10 items)

1. [x] Add utility callback hook for first non-zero digest locator by mount token.
2. [x] Add `vfsdigestfirst <mount> <start> <count>` command.
3. [x] Add `vfs digestfirst <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-first commands.
5. [x] Add digest-first count validation (`1..256`) for bounded scans.
6. [x] Implement digest-first header line with mount/start/count context.
7. [x] Implement digest-first read-fail diagnostics with failing block index.
8. [x] Emit first-hit line with block index and digest details (`SUM/XOR/B0`).
9. [x] Emit explicit `NONE` result when no non-zero block exists in range.
10. [x] Update help text and utility context wiring for digest-first commands.

## Batch 16 (10 items)

1. [x] Add utility callback hook for last non-zero digest locator by mount token.
2. [x] Add `vfsdigestlast <mount> <start> <count>` command.
3. [x] Add `vfs digestlast <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-last commands.
5. [x] Add digest-last count validation (`1..512`) for bounded scans.
6. [x] Implement digest-last header line with mount/start/count context.
7. [x] Implement digest-last read-fail diagnostics with failing block index.
8. [x] Emit last-hit line with block index and digest details (`SUM/XOR/B0`).
9. [x] Emit explicit `NONE` result when no non-zero block exists in range.
10. [x] Update help text and utility context wiring for digest-last commands.

## Batch 17 (10 items)

1. [x] Add utility callback hook for non-zero span digest by mount token.
2. [x] Add `vfsdigestspan <mount> <start> <count>` command.
3. [x] Add `vfs digestspan <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-span commands.
5. [x] Add digest-span count validation (`1..1024`) for bounded scans.
6. [x] Implement digest-span header line with mount/start/count context.
7. [x] Implement digest-span read-fail diagnostics with failing block index.
8. [x] Emit digest-span summary with non-zero count and first/last hit block indices.
9. [x] Emit digest-span first and last digest detail lines (`BYTES/SUM/XOR/B0`).
10. [x] Update help text and utility context wiring for digest-span commands.

## Batch 18 (10 items)

1. [x] Add utility callback hook for digest-window summary by mount token.
2. [x] Add `vfsdigestwindow <mount> <start> <count>` command.
3. [x] Add `vfs digestwindow <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-window commands.
5. [x] Add digest-window count validation (`1..2048`) for bounded scans.
6. [x] Implement digest-window header line with mount/start/count context.
7. [x] Implement digest-window read-fail diagnostics with failing block index.
8. [x] Emit digest-window summary with non-zero count and first/last non-zero block indices.
9. [x] Emit digest-window aggregate totals (`BYTES/SUM/XOR`) across the scanned range.
10. [x] Update help text and utility context wiring for digest-window commands.

## Batch 19 (10 items)

1. [x] Add utility callback hook for digest-run analysis by mount token.
2. [x] Add `vfsdigestruns <mount> <start> <count>` command.
3. [x] Add `vfs digestruns <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-runs commands.
5. [x] Add digest-runs count validation (`1..4096`) for bounded scans.
6. [x] Implement digest-runs header line with mount/start/count context.
7. [x] Implement digest-runs read-fail diagnostics with failing block index.
8. [x] Emit digest-runs summary with non-zero and zero block counts.
9. [x] Emit digest-runs span and streak metrics (`FIRSTNZ/LASTNZ/MAXNZRUN/MAXZRUN`).
10. [x] Update help text and utility context wiring for digest-runs commands.

## Batch 20 (10 items)

1. [x] Add utility callback hook for digest-transition analysis by mount token.
2. [x] Add `vfsdigesttrans <mount> <start> <count>` command.
3. [x] Add `vfs digesttrans <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-transition commands.
5. [x] Add digest-transition count validation (`1..8192`) for bounded scans.
6. [x] Implement digest-transition header line with mount/start/count context.
7. [x] Implement digest-transition read-fail diagnostics with failing block index.
8. [x] Emit digest-transition summary with non-zero and zero block counts.
9. [x] Emit transition/run metrics (`TRANS/NZRUNS/ZRUNS`) across the range.
10. [x] Update help text and utility context wiring for digest-transition commands.

## Batch 21 (10 items)

1. [x] Add utility callback hook for digest-density analysis by mount token.
2. [x] Add `vfsdigestdensity <mount> <start> <count>` command.
3. [x] Add `vfs digestdensity <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-density commands.
5. [x] Add digest-density count validation (`1..16384`) for bounded scans.
6. [x] Implement digest-density header line with mount/start/count context.
7. [x] Implement digest-density read-fail diagnostics with failing block index.
8. [x] Emit digest-density summary with non-zero and zero block counts.
9. [x] Emit digest-density percentage and permille metrics (`DENSITYPCT/DENSITYPM`).
10. [x] Update help text and utility context wiring for digest-density commands.

## Batch 22 (10 items)

1. [x] Add utility callback hook for digest-ratio analysis by mount token.
2. [x] Add `vfsdigestratio <mount> <start> <count>` command.
3. [x] Add `vfs digestratio <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-ratio commands.
5. [x] Add digest-ratio count validation (`1..32768`) for bounded scans.
6. [x] Implement digest-ratio header line with mount/start/count context.
7. [x] Implement digest-ratio read-fail diagnostics with failing block index.
8. [x] Emit digest-ratio summary with non-zero and zero block counts.
9. [x] Emit ratio/bias metrics (`NZPM/ZPM/DOM/DOMDELTA`) across the range.
10. [x] Update help text and utility context wiring for digest-ratio commands.

## Batch 23 (10 items)

1. [x] Add utility callback hook for digest-balance analysis by mount token.
2. [x] Add `vfsdigestbalance <mount> <start> <count>` command.
3. [x] Add `vfs digestbalance <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-balance commands.
5. [x] Add digest-balance count validation (`1..65536`) for bounded scans.
6. [x] Implement digest-balance header line with mount/start/count context.
7. [x] Implement digest-balance read-fail diagnostics with failing block index.
8. [x] Emit digest-balance summary with non-zero and zero block counts.
9. [x] Emit balance metrics (`NZPCT/ZPCT/DOM/BALPM`) across the range.
10. [x] Update help text and utility context wiring for digest-balance commands.

## Batch 24 (10 items)

1. [x] Add utility callback hook for digest-skew analysis by mount token.
2. [x] Add `vfsdigestskew <mount> <start> <count>` command.
3. [x] Add `vfs digestskew <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-skew commands.
5. [x] Add digest-skew count validation (`1..131072`) for bounded scans.
6. [x] Implement digest-skew header line with mount/start/count context.
7. [x] Implement digest-skew read-fail diagnostics with failing block index.
8. [x] Emit digest-skew summary with non-zero and zero block counts.
9. [x] Emit skew metrics (`NZPM/ZPM/DOM/SKEWPM`) across the range.
10. [x] Update help text and utility context wiring for digest-skew commands.

## Batch 25 (10 items)

1. [x] Add utility callback hook for digest-tilt analysis by mount token.
2. [x] Add `vfsdigesttilt <mount> <start> <count>` command.
3. [x] Add `vfs digesttilt <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-tilt commands.
5. [x] Add digest-tilt count validation (`1..262144`) for bounded scans.
6. [x] Implement digest-tilt header line with mount/start/count context.
7. [x] Implement digest-tilt read-fail diagnostics with failing block index.
8. [x] Emit digest-tilt summary with non-zero and zero block counts.
9. [x] Emit tilt metrics (`DOM/DOMPCT/TILTPM`) across the range.
10. [x] Update help text and utility context wiring for digest-tilt commands.

## Batch 26 (10 items)

1. [x] Add utility callback hook for digest-bias analysis by mount token.
2. [x] Add `vfsdigestbias <mount> <start> <count>` command.
3. [x] Add `vfs digestbias <mount> <start> <count>` alias.
4. [x] Add packed range support (`start.count`, `start:count`, `start/count`) for digest-bias commands.
5. [x] Add digest-bias count validation (`1..524288`) for bounded scans.
6. [x] Implement digest-bias header line with mount/start/count context.
7. [x] Implement digest-bias read-fail diagnostics with failing block index.
8. [x] Emit digest-bias summary with non-zero and zero block counts.
9. [x] Emit bias metrics (`DOM/BIASBLK/BIASPM`) across the range.
10. [x] Update help text and utility context wiring for digest-bias commands.

## Batch 27 (10 items)

1. [x] Add subsystem baseline architecture document and ownership map for non-VFS domains.
2. [x] Add kernel subsystem phase enum and runtime state tracker (Boot, Init, Ready, Degraded).
3. [x] Add subsystem health snapshot struct for scheduler, event manager, services, network, security, desktop, and window stacks.
4. [x] Add diagnostics API to publish subsystem health snapshots through runtime diagnostics.
5. [x] Add scheduler health metrics bridge (ready depth, preemption count, starvation counter, tick drift).
6. [x] Add event manager health metrics bridge (queue depth, fan-out latency, dropped events, handler faults).
7. [x] Add service subsystem health metrics bridge (service count, failed starts, restart attempts, dependency violations).
8. [x] Add security subsystem baseline health metrics (denied capability checks, policy load status, audit queue pressure).
9. [x] Add network subsystem baseline health metrics stubs (interface count, RX/TX counters, drop counters, link state).
10. [x] Add AI execution monitoring subsystem scaffold with telemetry ingress interface and no-op policy evaluator hook.

## Batch 28 (10 items)

1. [x] Add command utility callback hooks for subsystem-health and AI-monitor introspection commands.
2. [x] Add `subsyshealth` command to print published subsystem health snapshot metrics.
3. [x] Add `subsys health` alias command form for subsystem health introspection.
4. [x] Add `aimon` command to print AI monitor telemetry and policy/action state.
5. [x] Add `aimon eval` command to force policy evaluation of current telemetry.
6. [x] Add `ai monitor` and `ai monitor eval` alias command forms.
7. [x] Add static runtime diagnostics accessor for latest published subsystem health snapshot.
8. [x] Add AI monitor accessor APIs for policy-installed state and last action value.
9. [x] Install explicit no-op AI policy evaluator hook during kernel bootstrap.
10. [x] Update utility help text with subsystem/AI observability command family.

## Batch 29 (10 items)

1. [x] Add built-in AI policy mode enum for `NOOP/ALERT/THROTTLE/ISOLATE/RESTART` profiles.
2. [x] Implement built-in AI policy evaluation path when no custom evaluator is installed.
3. [x] Add AI monitor API to set/query built-in policy mode and mode-name string.
4. [x] Add utility callback hook for AI monitor policy command parsing and dispatch.
5. [x] Add `aimon policy` command to query current built-in policy mode.
6. [x] Add `aimon policy <mode>` command to set built-in policy mode.
7. [x] Add `ai monitor policy` alias command family.
8. [x] Extend `aimon` output with custom-policy installed flag and active built-in policy mode.
9. [x] Switch bootstrap AI monitor initialization to explicit built-in policy mode setup.
10. [x] Update utility help text with AI policy command usage.

## Batch 30 (10 items)

1. [x] Add AI policy threshold struct for built-in policy tuning (alert/throttle/isolate/restart).
2. [x] Add AI monitor APIs to get/set/reset built-in policy thresholds.
3. [x] Route built-in policy evaluation through configurable thresholds instead of hard-coded constants.
4. [x] Add utility callback hook for AI threshold command handling.
5. [x] Add `aimon threshold` command to print current threshold values.
6. [x] Add `aimon threshold reset` command to restore default threshold values.
7. [x] Add `aimon threshold set <key> <value>` command with validation and key map.
8. [x] Add `ai monitor threshold` alias command family.
9. [x] Extend `aimon` output with threshold summaries for live policy tuning context.
10. [x] Update utility help text with AI threshold command usage and supported keys.

## Batch 31 (10 items)

1. [x] Add runtime diagnostics API to force an immediate subsystem health snapshot publish.
2. [x] Add runtime diagnostics API to get/set/reset subsystem health publish interval ticks.
3. [x] Update runtime diagnostics publish scheduler to honor force-publish requests and configurable interval.
4. [x] Add utility callback hooks for subsystem health `brief`, `force`, and `interval` controls.
5. [x] Add `subsyshealth brief` command to emit compact phase/tick/ready/event/security summary.
6. [x] Add `subsyshealth force` command to request immediate subsystem snapshot publication.
7. [x] Add `subsyshealth interval [show|reset|set <ticks>|<ticks>]` command family.
8. [x] Add `aimon threshold export` command to emit replay-ready policy/threshold summaries.
9. [x] Add `aimon threshold profile [default|latency|strict]` presets for operational tuning.
10. [x] Update utility help text and command parsing/wiring for all new subsystem and AI controls.

## Batch 32 (10 items)

1. [x] Add AI monitor policy-state snapshot struct (mode + thresholds) for atomic replay.
2. [x] Add AI monitor APIs to get/set/reset full policy-state snapshots.
3. [x] Add utility callback hook and parser wiring for `aimon state` command family.
4. [x] Add `aimon state` / `aimon state show` command to print current mode + thresholds.
5. [x] Add `aimon state reset` command to restore no-op mode with default thresholds.
6. [x] Add `aimon state export` command to emit replay-ready import lines.
7. [x] Add `aimon state import mode <mode> <key value...>` command with key/value validation.
8. [x] Add `ai monitor state` alias command family.
9. [x] Add policy-mode parse helpers to unify mode-token handling across state import flow.
10. [x] Update utility help text for AI policy-state show/export/import/reset operations.

## Batch 33 (10 items)

1. [x] Add terminal-window state tracking (enabled flag + compositor surface ID).
2. [x] Add compositor-backed terminal window creation helper with default bounds.
3. [x] Add terminal window status command path with ID and bounds reporting.
4. [x] Add `terminal window on` command to open/focus/raise terminal window and enable terminal mode.
5. [x] Add `terminal window off` command to hide terminal window and disable terminal mode.
6. [x] Add `terminal window toggle` command for quick open/close workflow.
7. [x] Extend utility command context/validation with terminal-window callbacks.
8. [x] Extend utility parser with `terminal window [on|off|toggle|status]` command family.
9. [x] Render terminal panel inside compositor surface bounds when terminal window is enabled.
10. [x] Update help text and shutdown/hidelog behavior so terminal-off states close the window cleanly.

## Batch 34 (10 items)

1. [x] Add system-log channel model for runtime/subsystem/desktop/warn/error/audit routing.
2. [x] Add terminal filter state with preset route masks (`quiet`, `normal`, `full`).
3. [x] Add utility callback hooks for terminal filter query/set operations.
4. [x] Add command parsing for `terminal filter [quiet|normal|full|status]`.
5. [x] Add terminal status output to include current filter preset.
6. [x] Route `PushSystemLog` through channel classification instead of unconditional terminal-mode suppression.
7. [x] Preserve command-response logs as direct output (unfiltered by system channel routing).
8. [x] Update help text to document terminal filter controls.
9. [x] Reset terminal filter state to default (`normal`) on console initialize.
10. [x] Keep terminal window open/close flows compatible with new routing behavior.
