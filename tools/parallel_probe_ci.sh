#!/usr/bin/env bash
set -euo pipefail

burn_runs="${1:-${PARALLEL_CI_BURN_RUNS:-10}}"
parallel_smoke_timeout="${PARALLEL_SMOKE_TIMEOUT:-80}"
qemu_smp="${QEMU_SMP:-4}"
ci_gate_attempts="${PARALLEL_CI_GATE_ATTEMPTS:-2}"

if [[ ! "$burn_runs" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid burn run count: $burn_runs"
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

if [[ ! "$ci_gate_attempts" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid PARALLEL_CI_GATE_ATTEMPTS value: $ci_gate_attempts"
    exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

run_id="${PARALLEL_PROBE_CI_ID:-$(date +%Y%m%d-%H%M%S)}"
ci_dir="artifacts/parallel-probe-ci-${run_id}"
mkdir -p "$ci_dir"

echo "[ci] artifact directory: $ci_dir"
echo "[ci] timeout=${parallel_smoke_timeout}s smp=${qemu_smp} burn_runs=${burn_runs} gate_attempts=${ci_gate_attempts}"

safe_log="$ci_dir/qemu-serial.parallel-safe.log"
strict_log="$ci_dir/qemu-serial.parallel-strict.log"

run_gate_with_retry() {
    local label="$1"
    local target="$2"
    local log_path="$3"

    local attempt
    for ((attempt = 1; attempt <= ci_gate_attempts; attempt++)); do
        echo "[ci] running ${label} gate (attempt ${attempt}/${ci_gate_attempts})"
        if PARALLEL_SMOKE_TIMEOUT="$parallel_smoke_timeout" QEMU_SMP="$qemu_smp" \
            make "$target" QEMU_LOG="$log_path"; then
            return 0
        fi

        if (( attempt < ci_gate_attempts )); then
            echo "[ci] ${label} gate failed on attempt ${attempt}, retrying"
        fi
    done

    echo "[ci] ${label} gate failed after ${ci_gate_attempts} attempts"
    return 1
}

run_gate_with_retry "safe" "parallel-probe-smoke" "$safe_log"

run_gate_with_retry "strict" "parallel-probe-dispatch-drain-smoke" "$strict_log"

burn_id="ci-${run_id}"
echo "[ci] running strict burn id=${burn_id}"
PARALLEL_SMOKE_TIMEOUT="$parallel_smoke_timeout" QEMU_SMP="$qemu_smp" \
    PARALLEL_DISPATCH_BURN_ID="$burn_id" \
    make parallel-probe-dispatch-drain-burn PARALLEL_DISPATCH_BURN_RUNS="$burn_runs"

burn_dir="artifacts/parallel-dispatch-burn-${burn_id}"
if [[ -d "$burn_dir" ]]; then
    cp -a "$burn_dir" "$ci_dir/"
fi

summary_file="$ci_dir/summary.txt"
{
    echo "parallel-probe-ci PASS"
    echo "id: $run_id"
    echo "safe_log: $safe_log"
    echo "strict_log: $strict_log"
    echo "burn_runs: $burn_runs"
    echo "burn_dir: $burn_dir"
} > "$summary_file"

echo "[ci] complete"
echo "[ci] summary: $summary_file"
