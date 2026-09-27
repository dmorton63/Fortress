# Fortress OS (Minimal Limine Bootstrap)

This is a minimal bare-metal OS bootstrap using Limine. It boots a tiny x86_64 kernel and renders a rotating, software-rasterized 3D cube with a fixed-timestep loop.

It now includes Unreal-style C++ module scaffolding for memory and video subsystems,
including a software 3D rasterizer.

## Prerequisites

- `clang`
- `ld.lld`
- `make`
- `git`
- `xorriso`
- `qemu-system-x86_64`

On Debian/Ubuntu:

```bash
sudo apt install clang lld make git xorriso qemu-system-x86
```

## Build

```bash
make
```

Select a renderer backend at compile time (default: software):

```bash
make RENDERER_BACKEND=software
```

Placeholder backend selection path (expected to initialize-fail at boot until implemented):

```bash
make RENDERER_BACKEND=null
```

## Build Bootable ISO

```bash
make iso
```

## Build Bootable USB Image (BIOS + UEFI)

```bash
make usb-image
```

This produces:

- `build/fortress-usb.iso`

Flash it to a USB device (replace `/dev/sdX`):

```bash
sudo dd if=build/fortress-usb.iso of=/dev/sdX bs=4M status=progress conv=fsync
sync
```

## Repository Backup Snapshot

Create a timestamped backup snapshot commit (if needed), tag it, and push to your configured remote:

```bash
make backup-snapshot
```

Optional overrides:

- `BACKUP_REMOTE=origin` (default remote to push)
- `BACKUP_TAG_PREFIX=backup` (tag name prefix, e.g. `backup-20260927-012345`)

Example:

```bash
make backup-snapshot BACKUP_REMOTE=origin BACKUP_TAG_PREFIX=fortress-backup
```

## Run In QEMU

```bash
make run
```

To mirror serial output to the terminal and a log file:

```bash
make run-log
```

By default this writes to `build/qemu-serial.log`.
You can override the path:

```bash
make run-log QEMU_LOG=build/my-session.log
```

Validate desktop surface contract smoke markers against an existing log:

```bash
make dsksurf-contract-check
```

Validate occlusion-specific counters (requires an overlap/split-producing log):

```bash
make dsksurf-contract-occlusion-check
```

Run strict O1 geometry profile assertions for the boot overlap scenario:

```bash
make dsksurf-contract-occlusion-strict-check
```

Run strict fallback-profile assertions for the same boot scenario:

```bash
make dsksurf-contract-fallback-strict-check
```

Validate fail-token mapping compatibility (legacy fail lines + ERR_DSKSURF_*):

```bash
make dsksurf-contract-token-check
```

Run the full contract smoke harness (two captures + deterministic summary compare):

```bash
make dsksurf-contract-smoke
```

## Parallel SMP Validation

Run the combined parallel gate (safe lane + strict AP_DRAIN lane):

```bash
make parallel-probe-gate
```

Run strict AP_DRAIN burn validation (default 15 runs):

```bash
make parallel-probe-dispatch-drain-burn PARALLEL_DISPATCH_BURN_RUNS=15 PARALLEL_SMOKE_TIMEOUT=80
```

Run the CI-style parallel wrapper (gate + burn) and collect timestamped artifacts:

```bash
make parallel-probe-ci PARALLEL_CI_BURN_RUNS=15 PARALLEL_SMOKE_TIMEOUT=80
```

Canonical pre-merge gate for parallel runtime changes:

```bash
make parallel-premerge-gate
```

Fast local pre-merge lane (lower burn count for iteration speed):

```bash
make parallel-premerge-fast
```

Run the same pre-merge gate across SMP variants (default `2 4 8`):

```bash
make parallel-premerge-matrix
```

Fast local SMP matrix lane (default `2 4` with reduced burn count):

```bash
make parallel-premerge-fast-matrix
```

Optional matrix controls:

- `PARALLEL_MATRIX_SMPS="2 4 8"`
- `PARALLEL_CI_BURN_RUNS=15`
- `PARALLEL_SMOKE_TIMEOUT=80`

Fast-lane defaults:

- `PARALLEL_FAST_CI_BURN_RUNS=3`
- `PARALLEL_FAST_MATRIX_SMPS="2 4"`

Matrix runs also emit an automatic verification report:

- `artifacts/parallel-premerge-matrix-<timestamp>.report.txt`

Defaults used by this gate:

- `PARALLEL_CI_BURN_RUNS=15`
- `PARALLEL_SMOKE_TIMEOUT=80`

Artifacts:

- Gate and summary logs are written to `artifacts/parallel-probe-ci-<timestamp>/`.
- Burn logs are written to `artifacts/parallel-dispatch-burn-<id>/` and copied into the CI artifact folder.

Post-merge real-hardware soak archive helper:

```bash
make parallel-hw-soak-archive \
	HW_SOAK_LOG=/path/to/hardware-serial.log \
	HW_SOAK_CAPTURE_LOG=/path/to/hardware-capture.log \
	HW_SOAK_ID=amd-20260926-run1
```

This writes:

- `artifacts/hardware-soak-<id>/summary.txt`
- `artifacts/hardware-soak-<id>.tar.gz`

Developer run modes:

- `make run-dev` runs with probe autorun disabled.
- `make run-probe` runs with probe autorun and strict dispatch experimental flags enabled.

The default run command attaches a virtual USB tablet to the XHCI controller,
so `xhcienum` and `xhciaddrdev` have a connected port to enumerate.
When HID interrupt-IN polling is running, absolute tablet reports can drive
an on-screen software cursor crosshair.
With cursor overlay enabled, moving the cursor over the test box near the
upper-right and left-clicking toggles its state color.

## Notes

- The first `make iso` clones and builds Limine in `limine/`.
- `make usb-image` builds a hybrid BIOS+UEFI image suitable for writing directly to USB.
- The kernel entrypoint is in `src/kernel.cpp`.
- Naming rules are documented in `Standards.md`.

## Subsystems

- `Memory`: `FMemoryArena` bump allocator with stats for kernel-owned arena memory.
- `Memory`: `FPhysicalMemoryManager` page allocator seeded from Limine memory map.
- `Memory`: `FPhysicalMemoryManager` includes constrained allocation (`AllocatePagesBelow`) for low-address DMA requirements.
- `Memory`: `FVirtualMemoryManager` x86_64 page-table manager (map/unmap/translate) layered on Limine's initial CR3 + HHDM.
- `Memory`: `FKernelHeap` growable page-backed heap using PMM + VMM with allocation/free support.
- `Memory`: `FDmaMemoryManager` contiguous DMA buffer allocator with optional below-4GiB guarantees.
- `Memory`: `FPinnedMappingManager` tracked physical range pin/map API for MMIO/controller structures.
- `Kernel`: `FKernelCommandConsole` command parsing, PMM/VMM command actions, and on-screen command log state.
- `Kernel`: `FKernelBootstrap` central subsystem bring-up and runtime context creation.
- `Kernel`: `FKernelCubeScene` fixed-step cube scene simulation and render submission.
- `Kernel`: `FKernelHudOverlay` HUD text rendering and formatting.
- `Kernel`: `FKernelConfig` centralized kernel constants for addresses/sizes.
- `Video`: `FVideoDevice` (double-buffered framebuffer output), `FVideoConsole` (text output), `FSoftwareRenderer3D` (triangle rasterizer with depth buffer and face lighting).
- `Video`: renderer now includes near-plane clipping and optional wireframe overlay.
- `Platform`: `FTimerX86` PIT-based frame timing plus `FKeyboardX86` PS/2 keyboard input.
- `Platform`: `FPciConfigX86` legacy PCI config-space accessor (`0xCF8/0xCFC`) for early hardware discovery.
- `Platform`: `FXhciMmioRegisters` typed xHCI capability/operational/doorbell/runtime register access abstraction.
- `Platform`: `FXhciPciDiscovery` scans PCI class/subclass/program-interface and resolves the first xHCI MMIO BAR.
- `Cpu`: `FInterruptsX64` minimal IDT setup.
- `Cpu`: `FPanicScreen` exception visualization for panic state.

## Runtime Commands

Type commands in the on-screen prompt and press Enter:

- `help`
- `stats`
- `wire` (toggle)
- `wire on`
- `wire off`
- `pause`
- `resume`
- `cursor`
- `cursor on`
- `cursor off`
- `cursor invertx [on|off]`
- `cursor inverty [on|off]`
- `cursor sens N` (range `25..400`)
- `palloc`
- `palloc N`
- `pfree`
- `preserve low`
- `vmmap VA PA [N] [rw|rwnx|rx|dev]`
- `vmalloc N [rw|rwnx|rx|dev]`
- `vmfree`
- `vmfree VA`
- `vmunmap VA [N]`
- `vmtranslate VA`
- `dmaalloc BYTES [ALIGN] [low4g]`
- `dmafree`
- `dmafree VA`
- `mmpin PA BYTES [rw|rwnx|rx|dev]`
- `mmunpin`
- `mmunpin VA`
- `xhciregs [CAPVA|PABASE]`
- `xhciprobe`
- `xhciinit`
- `xhciringtest`
- `xhcienum`
- `xhciaddrdev`
- `xhcigetdesc`
- `xhcigetcfg`
- `xhcisetcfg`
- `xhciepconf`
- `xhciintrin`
- `xhciintrinloop`
- `xhcihid [compact|core|verbose]`
- `kalloc BYTES`
- `kfree VA`
- `memtest`
- `xhcimemtest`
- `mmiotest`
