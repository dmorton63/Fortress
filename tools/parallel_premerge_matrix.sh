#!/usr/bin/env bash
set -euo pipefail

smps_raw="${PARALLEL_MATRIX_SMPS:-2 4 8}"
burn_runs="${PARALLEL_CI_BURN_RUNS:-10}"
parallel_smoke_timeout="${PARALLEL_SMOKE_TIMEOUT:-80}"

if [[ ! "$burn_runs" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid PARALLEL_CI_BURN_RUNS value: $burn_runs"
    exit 1
fi

if [[ ! "$parallel_smoke_timeout" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid PARALLEL_SMOKE_TIMEOUT value: $parallel_smoke_timeout"
    exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

matrix_id="${PARALLEL_PREMERGE_MATRIX_ID:-$(date +%Y%m%d-%H%M%S)}"
summary_file="artifacts/parallel-premerge-matrix-${matrix_id}.summary.txt"
mkdir -p artifacts

{
    echo "parallel-premerge-matrix"
    echo "id: $matrix_id"
    echo "burn_runs: $burn_runs"
    echo "timeout: $parallel_smoke_timeout"
    echo "smps: $smps_raw"
} > "$summary_file"

echo "[matrix] id=$matrix_id smps=[$smps_raw] burn_runs=$burn_runs timeout=${parallel_smoke_timeout}s"

for smp in $smps_raw; do
    if [[ ! "$smp" =~ ^[1-9][0-9]*$ ]]; then
        echo "[matrix] invalid SMP entry: $smp"
        exit 1
    fi

    ci_id="matrix-${matrix_id}-smp${smp}"
    echo "[matrix] running premerge gate for QEMU_SMP=$smp ci_id=$ci_id"

    PARALLEL_PROBE_CI_ID="$ci_id" \
    QEMU_SMP="$smp" \
    PARALLEL_CI_BURN_RUNS="$burn_runs" \
    PARALLEL_SMOKE_TIMEOUT="$parallel_smoke_timeout" \
    make parallel-premerge-gate

    echo "smp${smp}: PASS artifacts/parallel-probe-ci-${ci_id}" >> "$summary_file"
done

bash "$script_dir/parallel_premerge_matrix_check.sh" "$matrix_id" "$smps_raw" "$burn_runs"
echo "report: artifacts/parallel-premerge-matrix-${matrix_id}.report.txt" >> "$summary_file"

echo "[matrix] all SMP variants passed"
echo "[matrix] summary: $summary_file"
