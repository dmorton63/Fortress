# Subsystem Baseline Architecture

This document establishes the non-VFS subsystem map for a fully functioning Fortress operating system and defines the baseline implementation sequence.

## Core Subsystem Inventory

1. Boot and Hardware Bring-up
- CPU/core discovery and AP startup.
- Interrupt table setup and timer calibration.
- Firmware table parsing, PCI discovery, MMIO mapping, DMA-safe regions.

2. Memory Management
- Physical memory manager and virtual memory manager.
- Kernel heap and allocator policies.
- User address spaces, fault handling, shared memory primitives.

3. Tasking and Execution
- Process and thread lifecycle model.
- Context switching and run-queue management.
- Priority, affinity, preemption, time accounting.

4. IPC, Events, and Messaging
- Event manager topic model and fan-out guarantees.
- Message bus contracts for subsystem decoupling.
- Synchronization primitives and queue backpressure policy.

5. Service Subsystem
- Service registry and dependency graph.
- Startup/shutdown ordering and restart policy.
- Capability checks for service-to-service access.

6. Device Driver Subsystem
- Driver lifecycle (probe, bind, suspend, remove).
- Common contracts for storage, input, display, network, and USB.
- Hotplug and fault-isolation behavior.

7. Storage and Filesystem Stack
- VFS routing and mount table control plane.
- Block cache and writeback policy.
- Filesystem driver contracts, recovery, and integrity hooks.

8. Network Subsystem
- NIC driver model and packet buffer management.
- L2/L3/L4 stack baseline and socket API.
- DNS resolver, routing policy, firewall attachment points.

9. Security Subsystem
- Identity model for users/services.
- Authentication and authorization path.
- Capabilities/ACL, audit trail, cryptographic service hooks.

10. Desktop and Window Stack
- Compositor, surface tree, and damage pipeline.
- Window management and focus/input policy.
- Shell integration and desktop-level service UX.

11. Syscall and ABI Layer
- Stable syscall surface and ABI versioning.
- Handle model and error semantics.
- Compatibility and policy checkpoints.

12. Observability and Diagnostics
- Structured logging, metrics, and tracing.
- Crash capture and postmortem tooling.
- Runtime health summaries and anomaly hooks.

13. Package and Update Subsystem
- Artifact format and signature verification.
- Install, rollback, and migration sequencing.
- Update orchestration and failure recovery.

14. AI Execution Monitoring Subsystem
- Execution telemetry taps (scheduler/syscall/events/memory/network/file IO).
- Behavioral policy profiles by service/process.
- Anomaly detection, response policy (alert/throttle/isolate/restart), and explainable audit trail.

## Non-VFS Ownership Map

This map defines initial code ownership boundaries by subsystem so implementation work can proceed without cross-domain ambiguity.

| Subsystem | Primary Runtime Owner | Core Kernel Modules |
| --- | --- | --- |
| Boot and Hardware Bring-up | Kernel Bootstrap Plane | `FKernelBootstrap`, `FCpuCoreManager`, platform bring-up (`FTimerX86`, `FPciConfigX86`) |
| Memory Management | Memory Plane | `FPhysicalMemoryManager`, `FVirtualMemoryManager`, `FKernelHeap`, `FDmaMemoryManager` |
| Tasking and Execution | Scheduler Plane | `FKernelScheduler`, `FKernelSchedulerEventPlane`, `FKernelRuntimeLoop` |
| IPC, Events, Messaging | Event Fabric Plane | `FMessageBus`, `FEventManager`, `FKernelCommandControlPlane`, `FKernelInputEventPlane` |
| Service Subsystem | Service Control Plane | `FServiceRegistry`, `FServiceRegistryDatabaseAdapter`, `FPortManager` |
| Device Driver Subsystem | Platform Driver Plane | storage/input/display/network/USB drivers under `Platform/` and `Storage/` |
| Network Subsystem | Network Control Plane | (planned) network interface registry, packet queue manager, protocol workers |
| Security Subsystem | Security Policy Plane | `FPortManager` policy path, capability checks, audit sinks |
| Desktop and Window Stack | Desktop Runtime Plane | `FDesktopRuntime`, `FDesktopShell`, `FDesktopCompositor`, overlays/input router |
| Syscall and ABI Layer | Kernel ABI Plane | (planned) syscall dispatcher, handle table, ABI compatibility layer |
| Observability and Diagnostics | Diagnostics Plane | `FKernelRuntimeDiagnostics`, command-console diagnostics, health snapshots |
| Package and Update Subsystem | Lifecycle Plane | (planned) package verifier, installer/rollback controller |
| AI Execution Monitoring | AI Monitor Plane | (planned) telemetry ingestion, anomaly evaluator, response policy hooks |

## Batch 27 Baseline Scope (Non-VFS)

Batch 27 introduces the cross-subsystem architecture baseline and interfaces needed to move beyond VFS-centric expansion.

Target outcomes:
- Baseline subsystem ownership map and interface contracts in-source.
- Control-plane hooks for security, networking, scheduler, and services.
- Initial AI monitoring architecture scaffolding tied to runtime telemetry.
