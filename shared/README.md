# Fortress Roadmap

This roadmap translates long-term product goals into implementation phases.

## Phase 1: Kernel Parallelism Foundations

Status: complete

Scope:
- CPU core inventory and bootstrap processor discovery
- Scheduler scaffolding with task states and tick-driven rotation
- Boot diagnostics for core and scheduler state

Deliverables:
- `FCpuCoreManager` initialized from Limine MP response
- `FKernelScheduler` initialized with online core count
- Frame loop calls scheduler tick hook

Exit criteria:
- Boot log reports online cores and scheduler initialization
- Kernel builds and boots in current QEMU flow

## Phase 2: Messaging and Event Fabric

Status: complete

Scope:
- Message envelopes and channel IDs
- Kernel event manager with pub/sub topics
- Service registry metadata and endpoint discovery

Deliverables:
- `FMessageBus` with fixed-size queues
- `FEventManager` with typed event IDs
- Service registration API used by kernel subsystems

Exit criteria:
- Input and desktop subsystems communicate over bus interfaces

## Phase 3: Storage and Driver-Swappable Filesystem Layer

Status: complete

Scope:
- Block device abstraction
- VFS mount table and path dispatch
- Driver swap strategy for hardware-specific storage backends

Deliverables:
- `IBlockDevice` contract
- `FVirtualFileSystem` path router
- At least one boot volume driver binding

Exit criteria:
- System can mount and access files through VFS-only interfaces

## Phase 4: Input, Fonts, and Text Pipeline

Status: complete

Scope:
- Keyboard manager with layout abstraction
- Font manager with glyph cache
- Text shaping and UI-facing text rendering contracts

Deliverables:
- `FKeyboardManager`
- `FFontManager`
- Text draw API that does not depend on low-level glyph bitmaps

Exit criteria:
- Desktop shell can receive text input and render dynamic text cleanly

## Phase 5: Desktop Composition Stack

Status: in progress

Scope:
- Desktop surface manager/compositor
- Desktop manager and layout/designer runtime
- Event routing for focus and input capture

Deliverables:
- Desktop surface tree, z-order, and damage tracking
- Desktop shell process with pluggable components

Exit criteria:
- Multiple Desktop Surface components can render and receive input

## Phase 6: Platform Services and Security Controls

Scope:
- Service base database integration (Citadel-compatible model)
- Port manager default-deny policy and open/close leases
- Capability checks across service boundaries

Deliverables:
- `FServiceRegistryDatabaseAdapter`
- `FPortManager` with audit trail

Exit criteria:
- External ports remain closed until an authorized service acquires a lease

## Phase 7: First-Party IDE Application

Scope:
- Text editor core
- Project/workspace model
- Build/run/debug integration with Fortress services

Deliverables:
- `FortressIDE` executable and package manifest

Exit criteria:
- Edit/build/run cycle available inside Fortress desktop
