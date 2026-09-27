#!/usr/bin/env bash
set -euo pipefail

runs="${1:-10}"

if [[ ! "$runs" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid run count: $runs"
    echo "Usage: parallel_probe_dispatch_drain_burn.sh [runs]"
    exit 1
fi

parallel_smoke_timeout="${PARALLEL_SMOKE_TIMEOUT:-80}"
qemu_smp="${QEMU_SMP:-4}"

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

burn_id="${PARALLEL_DISPATCH_BURN_ID:-$(date +%Y%m%d-%H%M%S)}"
burn_dir="artifacts/parallel-dispatch-burn-${burn_id}"
mkdir -p "$burn_dir"
echo "[burn] artifact directory: $burn_dir"

pass_count=0

for ((i = 1; i <= runs; i++)); do
    success=0
    max_attempts=2
    for ((attempt = 1; attempt <= max_attempts; attempt++)); do
        log_path="$burn_dir/qemu-serial.parallel-dispatch-burn-${i}.attempt-${attempt}.log"
        echo "[burn $i/$runs attempt $attempt/$max_attempts] strict drain smoke -> $log_path"

        if PARALLEL_SMOKE_TIMEOUT="$parallel_smoke_timeout" QEMU_SMP="$qemu_smp" \
            bash "$script_dir/parallel_probe_smoke.sh" "$log_path" \
                --dispatch-callbacks \
                --arm-dispatch-callbacks \
                --arm-canary-enqueue \
                --capture1 \
                --micro-canary \
                --require-pending-drain \
                --drain-window 3; then
            success=1
            break
        fi

        if [[ ! -s "$log_path" ]]; then
            echo "[burn] failed on iteration $i attempt $attempt (no log output)"
            continue
        fi

        if ! rg -q "CMD> parallel on" "$log_path"; then
            echo "[burn] iteration $i attempt $attempt: autorun command sequence did not start"
            if (( attempt < max_attempts )); then
                echo "[burn] retrying iteration $i once"
                continue
            fi
        fi

        if ! rg -q "PARALLELCANARY REQ" "$log_path"; then
            echo "[burn] iteration $i attempt $attempt: autorun stalled before canary completion"
            if (( attempt < max_attempts )); then
                echo "[burn] retrying iteration $i once"
                continue
            fi
        fi

        break
    done

    if (( success == 1 )); then
        pass_count=$((pass_count + 1))
        continue
    fi

    echo "[burn] failed on iteration $i"
    echo "[burn] passes before failure: $pass_count/$runs"
    exit 1
done

echo "[burn] all iterations passed: $pass_count/$runs"
echo "[burn] logs kept under: $burn_dir"
