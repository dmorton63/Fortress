#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
    parallel_probe_smoke.sh [output_log] [--dispatch-callbacks] [--arm-dispatch-callbacks] [--arm-canary-enqueue] [--capture1] [--micro-canary] [--require-pending-drain] [--drain-window N]

Builds AP-probe experimental image, replays deterministic parallel commands over serial,
and validates expected probe/drain markers.
EOF
}

output_log="build/qemu-serial.log"
capture_log=""
dispatch_callbacks=0
arm_dispatch_callbacks=0
arm_canary_enqueue=0
capture1=0
autorun_micro_canary=0
require_pending_drain=false
drain_probe_window=3
parallel_smoke_timeout="${PARALLEL_SMOKE_TIMEOUT:-40}"
qemu_smp="${QEMU_SMP:-4}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dispatch-callbacks)
            dispatch_callbacks=1
            shift
            ;;
        --arm-dispatch-callbacks)
            arm_dispatch_callbacks=1
            shift
            ;;
        --arm-canary-enqueue)
            arm_canary_enqueue=1
            shift
            ;;
        --capture1)
            capture1=1
            shift
            ;;
        --micro-canary)
            autorun_micro_canary=1
            shift
            ;;
        --require-pending-drain)
            require_pending_drain=true
            shift
            ;;
        --drain-window)
            if [[ $# -lt 2 ]]; then
                echo "Missing value for --drain-window"
                exit 1
            fi
            drain_probe_window="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            if [[ "$output_log" == "build/qemu-serial.log" ]]; then
                output_log="$1"
                shift
            else
                echo "Unexpected argument: $1"
                usage
                exit 1
            fi
            ;;
    esac
done

if [[ "$output_log" == *.log ]]; then
    capture_log="${output_log%.log}.capture.log"
else
    capture_log="${output_log}.capture.log"
fi

if [[ ! "$drain_probe_window" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid --drain-window value: $drain_probe_window"
    exit 1
fi

if [[ ! "$parallel_smoke_timeout" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid PARALLEL_SMOKE_TIMEOUT value: $parallel_smoke_timeout"
    exit 1
fi

if [[ ! "$qemu_smp" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid QEMU_SMP value: $qemu_smp"
    exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

mkdir -p build

make clean >/dev/null
make iso AP_DRAIN_EXPERIMENTAL=1 AP_DISPATCH_CALLBACKS_EXPERIMENTAL="$dispatch_callbacks" AP_DISPATCH_CALLBACKS_ARMED="$arm_dispatch_callbacks" AP_DISPATCH_CANARY_ENQUEUE_EXPERIMENTAL="$arm_canary_enqueue" AP_DISPATCH_CAPTURE1="$capture1" PARALLEL_PROBE_AUTORUN=1 PARALLEL_PROBE_AUTORUN_MICRO_CANARY="$autorun_micro_canary" >/dev/null

rm -f "$output_log"
rm -f "$capture_log"

qemu_capture_args=()
if [[ "$capture1" -eq 1 ]]; then
    qemu_capture_args+=( -debugcon "file:$capture_log" -global isa-debugcon.iobase=0xe9 )
fi

echo "Capturing parallel probe log: $output_log"
set +e
timeout "${parallel_smoke_timeout}s" \
    qemu-system-x86_64 -M q35 -m 256M -smp "$qemu_smp" \
    -cdrom build/fortress.iso \
    -serial "file:$output_log" \
    "${qemu_capture_args[@]}" \
    -no-reboot \
    -device qemu-xhci,id=xhci \
    -device usb-tablet,bus=xhci.0 \
    >/dev/null 2>&1
qemu_status=$?
set -e

if [[ ! -s "$output_log" ]]; then
    echo "No serial log produced"
    exit 1
fi

if [[ $qemu_status -ne 0 && $qemu_status -ne 124 ]]; then
    echo "QEMU exited with status: $qemu_status"
fi

check_args=("$output_log")
if [[ "$autorun_micro_canary" -eq 0 ]]; then
    check_args+=(--skip-canary-check --require-shutdown)
fi
if [[ "$dispatch_callbacks" -eq 1 ]]; then
    if [[ "$arm_dispatch_callbacks" -eq 1 ]]; then
        check_args+=(--expect-mode drain --allow-zero-dpt)
    else
        check_args+=(--expect-mode probe)
    fi
    if [[ "$require_pending_drain" == false ]] || [[ "$arm_dispatch_callbacks" -eq 0 ]]; then
        check_args+=(--allow-canary-blocked)
    fi
else
    check_args+=(--expect-mode probe)
    if [[ "$autorun_micro_canary" -eq 1 ]]; then
        check_args+=(--allow-probe-canary-blocked)
    fi
fi
if [[ "$require_pending_drain" == true ]]; then
    check_args+=(--require-pending-drain --drain-window "$drain_probe_window")
fi
bash "$script_dir/parallel_probe_smoke_check.sh" "${check_args[@]}"

echo "Parallel probe smoke harness pass."
if [[ -s "$capture_log" ]]; then
    echo "Dispatch capture log: $capture_log"
fi
