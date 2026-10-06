CXX := clang++
LD := ld.lld
CXXFLAGS := -std=c++20 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -mcmodel=kernel -mno-red-zone -fno-exceptions -fno-rtti -Wall -Wextra -Iinclude
LDFLAGS := -nostdlib -static -T linker.ld
RENDERER_BACKEND ?= software
AP_DRAIN_EXPERIMENTAL ?= 0
AP_DISPATCH_CALLBACKS_EXPERIMENTAL ?= 0
AP_DISPATCH_CALLBACKS_ARMED ?= 0
AP_DISPATCH_CANARY_ENQUEUE_EXPERIMENTAL ?= 0
AP_DISPATCH_CAPTURE1 ?= 0
PARALLEL_PROBE_AUTORUN ?= 0
PARALLEL_PROBE_AUTORUN_MICRO_CANARY ?= 0
DISPLAY_LATENCY_EXPERIMENTAL ?= 0
KEYBOARD_FONT_PROFILE_EXPERIMENTAL ?= 0
NO_REBOOT ?= 0

ifeq ($(AP_DRAIN_EXPERIMENTAL),1)
CXXFLAGS += -DFORTRESS_EXPERIMENTAL_AP_DRAIN_ENABLE
endif

ifeq ($(AP_DISPATCH_CALLBACKS_EXPERIMENTAL),1)
CXXFLAGS += -DFORTRESS_EXPERIMENTAL_AP_DISPATCH_CALLBACKS
endif

ifeq ($(AP_DISPATCH_CALLBACKS_ARMED),1)
CXXFLAGS += -DFORTRESS_EXPERIMENTAL_AP_DISPATCH_CALLBACKS_ARMED
endif

ifeq ($(AP_DISPATCH_CANARY_ENQUEUE_EXPERIMENTAL),1)
CXXFLAGS += -DFORTRESS_EXPERIMENTAL_AP_DISPATCH_CANARY_ENQUEUE
endif

ifeq ($(AP_DISPATCH_CAPTURE1),1)
CXXFLAGS += -DFORTRESS_EXPERIMENTAL_DISPATCH_CAPTURE1
endif

ifeq ($(PARALLEL_PROBE_AUTORUN),1)
CXXFLAGS += -DFORTRESS_PARALLEL_PROBE_AUTORUN
endif

ifeq ($(PARALLEL_PROBE_AUTORUN_MICRO_CANARY),1)
CXXFLAGS += -DFORTRESS_PARALLEL_PROBE_AUTORUN_MICRO_CANARY
endif

ifeq ($(DISPLAY_LATENCY_EXPERIMENTAL),1)
CXXFLAGS += -DFORTRESS_EXPERIMENTAL_DISPLAY_LATENCY
endif

ifeq ($(KEYBOARD_FONT_PROFILE_EXPERIMENTAL),1)
CXXFLAGS += -DFORTRESS_EXPERIMENTAL_KEYBOARD_FONT_PROFILE
endif

ifeq ($(NO_REBOOT),1)
CXXFLAGS += -DFORTRESS_NO_REBOOT
endif

ifeq ($(RENDERER_BACKEND),software)
CXXFLAGS += -DFORTRESS_RENDERER_BACKEND_SOFTWARE
else ifeq ($(RENDERER_BACKEND),null)
CXXFLAGS += -DFORTRESS_RENDERER_BACKEND_NULL
else
$(error Unsupported RENDERER_BACKEND '$(RENDERER_BACKEND)'; expected 'software' or 'null')
endif

BUILD_DIR := build
GENERATED_AERO_HEADER := include/Fortress/Kernel/Generated/FAeroTheme.generated.hpp
ISO_DIR := $(BUILD_DIR)/iso_root
LIMINE_DIR := limine
KERNEL := $(BUILD_DIR)/kernel.elf
ISO := $(BUILD_DIR)/fortress.iso
USB_IMAGE := $(BUILD_DIR)/fortress-usb.iso
QEMU_LOG ?= $(BUILD_DIR)/qemu-serial.log
QEMU_SMP ?= 4
HOST_SYSTEM_DIR ?=
HOST_SHARED_DIR ?=
HOST_SYSTEM_DIR_DEFAULT := $(CURDIR)/System
HOST_SHARED_DIR_DEFAULT := $(CURDIR)/shared
QEMU_EXTRA_ARGS ?=

ifneq ($(strip $(HOST_SYSTEM_DIR)),)
QEMU_EXTRA_ARGS += -virtfs local,path=$(HOST_SYSTEM_DIR),mount_tag=host_system,security_model=none,readonly=on
endif

ifneq ($(strip $(HOST_SHARED_DIR)),)
QEMU_EXTRA_ARGS += -virtfs local,path=$(HOST_SHARED_DIR),mount_tag=host_shared,security_model=none
endif

CONTRACT_SMOKE_TIMEOUT ?= 40
PARALLEL_SMOKE_TIMEOUT ?= 40
PARALLEL_DISPATCH_BURN_RUNS ?= 15
PARALLEL_CI_BURN_RUNS ?= 15
PARALLEL_MATRIX_SMPS ?= 2 4 8
PARALLEL_FAST_CI_BURN_RUNS ?= 3
PARALLEL_FAST_MATRIX_SMPS ?= 2 4
HW_SOAK_LOG ?=
HW_SOAK_CAPTURE_LOG ?=
BACKUP_REMOTE ?= origin
BACKUP_TAG_PREFIX ?= backup
SOURCES := $(shell find src -name '*.cpp')
OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean limine iso usb-image run run-dev run-probe run-log run-log-check run-shares run-log-shares host-shares-setup parallel-probe-check parallel-probe-drain-check parallel-probe-smoke parallel-probe-drain-smoke parallel-probe-dispatch-containment-smoke parallel-probe-dispatch-drain-smoke parallel-probe-gate parallel-probe-dispatch-drain-burn parallel-probe-ci parallel-premerge-fast parallel-premerge-gate parallel-premerge-fast-matrix parallel-premerge-matrix parallel-hw-soak-archive dsksurf-contract-check dsksurf-contract-occlusion-check dsksurf-contract-occlusion-strict-check dsksurf-contract-fallback-strict-check dsksurf-contract-token-check dsksurf-contract-smoke backup-snapshot

all: $(KERNEL)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(GENERATED_AERO_HEADER): aero.json tools/generate_aero_theme_header.py
	@mkdir -p $(dir $@)
	python3 tools/generate_aero_theme_header.py aero.json $(GENERATED_AERO_HEADER)

$(BUILD_DIR)/Kernel/FKernelFramePipeline.o: $(GENERATED_AERO_HEADER)

$(BUILD_DIR)/Cpu/%.o: CXXFLAGS += -mgeneral-regs-only

$(KERNEL): $(BUILD_DIR) $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $(KERNEL)

limine:
	@if [ ! -f "$(LIMINE_DIR)/limine-bios.sys" ]; then \
		rm -rf $(LIMINE_DIR); \
		git clone --branch=v12.x-binary --depth=1 https://github.com/limine-bootloader/limine.git $(LIMINE_DIR); \
	fi
	$(MAKE) -C $(LIMINE_DIR)

iso: limine $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/limine
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp limine.conf $(ISO_DIR)/boot/limine/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_DIR)/boot/limine/
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_DIR)/boot/limine/
	xorriso -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--protective-msdos-label \
		$(ISO_DIR) -o $(ISO)

usb-image: limine $(KERNEL)
	mkdir -p $(ISO_DIR)/boot/limine
	mkdir -p $(ISO_DIR)/EFI/BOOT
	cp $(KERNEL) $(ISO_DIR)/boot/kernel.elf
	cp limine.conf $(ISO_DIR)/boot/limine/limine.conf
	cp $(LIMINE_DIR)/limine-bios.sys $(ISO_DIR)/boot/limine/
	cp $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_DIR)/boot/limine/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_DIR)/boot/limine/
	cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_DIR)/EFI/BOOT/BOOTX64.EFI
	cp $(LIMINE_DIR)/BOOTIA32.EFI $(ISO_DIR)/EFI/BOOT/BOOTIA32.EFI
	xorriso -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		$(ISO_DIR) -o $(USB_IMAGE)

run: iso
	qemu-system-x86_64 -M q35 -m 256M -smp $(QEMU_SMP) -cdrom $(ISO) -serial stdio -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 $(QEMU_EXTRA_ARGS)

run-dev:
	@$(MAKE) clean
	@$(MAKE) run PARALLEL_PROBE_AUTORUN=0 PARALLEL_PROBE_AUTORUN_MICRO_CANARY=0

run-probe:
	@$(MAKE) clean
	@$(MAKE) run \
		PARALLEL_PROBE_AUTORUN=1 \
		PARALLEL_PROBE_AUTORUN_MICRO_CANARY=1 \
		AP_DRAIN_EXPERIMENTAL=1 \
		AP_DISPATCH_CALLBACKS_EXPERIMENTAL=1 \
		AP_DISPATCH_CALLBACKS_ARMED=1 \
		AP_DISPATCH_CANARY_ENQUEUE_EXPERIMENTAL=1

run-log: iso
	@mkdir -p $(BUILD_DIR)
	@echo "Logging serial output to $(QEMU_LOG)"
	qemu-system-x86_64 -M q35 -m 256M -smp $(QEMU_SMP) -cdrom $(ISO) -serial stdio -device qemu-xhci,id=xhci -device usb-tablet,bus=xhci.0 $(QEMU_EXTRA_ARGS) 2>&1 | tee $(QEMU_LOG)

host-shares-setup:
	@mkdir -p "$(HOST_SYSTEM_DIR_DEFAULT)" "$(HOST_SHARED_DIR_DEFAULT)"
	@echo "Host share folders ready:"
	@echo "  $(HOST_SYSTEM_DIR_DEFAULT)"
	@echo "  $(HOST_SHARED_DIR_DEFAULT)"

run-shares: host-shares-setup
	@$(MAKE) run HOST_SYSTEM_DIR="$(HOST_SYSTEM_DIR_DEFAULT)" HOST_SHARED_DIR="$(HOST_SHARED_DIR_DEFAULT)"

run-log-shares: host-shares-setup
	@$(MAKE) run-log HOST_SYSTEM_DIR="$(HOST_SYSTEM_DIR_DEFAULT)" HOST_SHARED_DIR="$(HOST_SHARED_DIR_DEFAULT)"

run-log-check:
	@if [ ! -s "$(QEMU_LOG)" ]; then \
		echo "Missing or empty $(QEMU_LOG)."; \
		echo "Generate it first, e.g. timeout 40s make run-log"; \
		exit 1; \
	fi
	@echo "--- marker checks ---"
	@set -e; \
	for m in "DSKSURF SMOKE PASS" "DSKSURF OVERLAY ON" "DESKTOP S " "DESKTOP DIRTY TOP" "DESKTOP SPLIT TOP"; do \
		if grep -Fq "$$m" "$(QEMU_LOG)"; then \
			echo "FOUND: $$m"; \
		else \
			echo "MISSING: $$m"; \
			exit 1; \
		fi; \
	done
	@echo "--- DESKTOP sample lines ---"
	@grep -F "DESKTOP" "$(QEMU_LOG)" | head -n 12 || true
	@echo "--- desktop surface contract check ---"
	@./tools/desktop_surface_contract_smoke_check.sh "$(QEMU_LOG)"

parallel-probe-check:
	@bash ./tools/parallel_probe_smoke_check.sh "$(QEMU_LOG)"

parallel-probe-drain-check:
	@bash ./tools/parallel_probe_smoke_check.sh "$(QEMU_LOG)" --require-pending-drain --drain-window 3

parallel-probe-smoke:
	@PARALLEL_SMOKE_TIMEOUT=$(PARALLEL_SMOKE_TIMEOUT) QEMU_SMP=$(QEMU_SMP) bash ./tools/parallel_probe_smoke.sh "$(QEMU_LOG)"

parallel-probe-drain-smoke:
	@PARALLEL_SMOKE_TIMEOUT=$(PARALLEL_SMOKE_TIMEOUT) QEMU_SMP=$(QEMU_SMP) bash ./tools/parallel_probe_smoke.sh "$(QEMU_LOG)" --dispatch-callbacks

parallel-probe-dispatch-containment-smoke:
	@PARALLEL_SMOKE_TIMEOUT=$(PARALLEL_SMOKE_TIMEOUT) QEMU_SMP=$(QEMU_SMP) bash ./tools/parallel_probe_smoke.sh "$(QEMU_LOG)" --dispatch-callbacks --arm-dispatch-callbacks --micro-canary

parallel-probe-dispatch-drain-smoke:
	@PARALLEL_SMOKE_TIMEOUT=$${PARALLEL_SMOKE_TIMEOUT:-80} QEMU_SMP=$(QEMU_SMP) bash ./tools/parallel_probe_smoke.sh "$(QEMU_LOG)" --dispatch-callbacks --arm-dispatch-callbacks --arm-canary-enqueue --capture1 --micro-canary --require-pending-drain --drain-window 3

parallel-probe-gate:
	@$(MAKE) parallel-probe-smoke QEMU_LOG=$(BUILD_DIR)/qemu-serial.parallel-safe.log
	@$(MAKE) parallel-probe-dispatch-drain-smoke QEMU_LOG=$(BUILD_DIR)/qemu-serial.parallel-strict.log

parallel-probe-dispatch-drain-burn:
	@PARALLEL_SMOKE_TIMEOUT=$${PARALLEL_SMOKE_TIMEOUT:-80} QEMU_SMP=$(QEMU_SMP) bash ./tools/parallel_probe_dispatch_drain_burn.sh "$(PARALLEL_DISPATCH_BURN_RUNS)"

parallel-probe-ci:
	@PARALLEL_SMOKE_TIMEOUT=$${PARALLEL_SMOKE_TIMEOUT:-80} QEMU_SMP=$(QEMU_SMP) PARALLEL_CI_BURN_RUNS=$(PARALLEL_CI_BURN_RUNS) bash ./tools/parallel_probe_ci.sh

parallel-premerge-fast:
	@$(MAKE) parallel-probe-ci PARALLEL_CI_BURN_RUNS=$${PARALLEL_CI_BURN_RUNS:-$(PARALLEL_FAST_CI_BURN_RUNS)} PARALLEL_SMOKE_TIMEOUT=$${PARALLEL_SMOKE_TIMEOUT:-80}

parallel-premerge-gate:
	@$(MAKE) parallel-probe-ci PARALLEL_CI_BURN_RUNS=$${PARALLEL_CI_BURN_RUNS:-15} PARALLEL_SMOKE_TIMEOUT=$${PARALLEL_SMOKE_TIMEOUT:-80}

parallel-premerge-fast-matrix:
	@PARALLEL_CI_BURN_RUNS=$${PARALLEL_CI_BURN_RUNS:-$(PARALLEL_FAST_CI_BURN_RUNS)} PARALLEL_SMOKE_TIMEOUT=$${PARALLEL_SMOKE_TIMEOUT:-80} PARALLEL_MATRIX_SMPS="$${PARALLEL_MATRIX_SMPS:-$(PARALLEL_FAST_MATRIX_SMPS)}" bash ./tools/parallel_premerge_matrix.sh

parallel-premerge-matrix:
	@PARALLEL_CI_BURN_RUNS=$${PARALLEL_CI_BURN_RUNS:-15} PARALLEL_SMOKE_TIMEOUT=$${PARALLEL_SMOKE_TIMEOUT:-80} PARALLEL_MATRIX_SMPS="$(PARALLEL_MATRIX_SMPS)" bash ./tools/parallel_premerge_matrix.sh

parallel-hw-soak-archive:
	@if [ -z "$(HW_SOAK_LOG)" ]; then \
		echo "HW_SOAK_LOG is required"; \
		echo "Example: make parallel-hw-soak-archive HW_SOAK_LOG=/path/to/serial.log HW_SOAK_ID=amd-20260926"; \
		exit 1; \
	fi
	@HW_SOAK_LOG="$(HW_SOAK_LOG)" HW_SOAK_CAPTURE_LOG="$(HW_SOAK_CAPTURE_LOG)" HW_SOAK_ID=$${HW_SOAK_ID:-$$(date +%Y%m%d-%H%M%S)} bash ./tools/post_merge_hardware_soak_archive.sh

dsksurf-contract-check:
	@./tools/desktop_surface_contract_smoke_check.sh "$(QEMU_LOG)"

dsksurf-contract-occlusion-check:
	@./tools/desktop_surface_contract_smoke_check.sh "$(QEMU_LOG)" --expect-occlusion

dsksurf-contract-occlusion-strict-check:
	@./tools/desktop_surface_contract_smoke_check.sh "$(QEMU_LOG)" --expect-occlusion --geometry-profile boot-occlusion-v1

dsksurf-contract-fallback-strict-check:
	@./tools/desktop_surface_contract_smoke_check.sh "$(QEMU_LOG)" --expect-occlusion --geometry-profile boot-fallback-v1

dsksurf-contract-token-check:
	@./tools/desktop_surface_contract_smoke_check.sh "$(QEMU_LOG)" --token-audit --strict-fail-token-map

dsksurf-contract-smoke:
	@./tools/desktop_surface_contract_smoke.sh "$(CONTRACT_SMOKE_TIMEOUT)"

backup-snapshot:
	@BACKUP_REMOTE="$(BACKUP_REMOTE)" BACKUP_TAG_PREFIX="$(BACKUP_TAG_PREFIX)" bash ./tools/git_backup_snapshot.sh

clean:
	rm -rf $(BUILD_DIR)
