#!/usr/bin/env bash
set -euo pipefail

matrix_id="${1:-}"
smps_raw="${2:-}"
expected_burn_runs="${3:-}"

if [[ -z "$matrix_id" || -z "$smps_raw" || -z "$expected_burn_runs" ]]; then
    echo "Usage: parallel_premerge_matrix_check.sh <matrix_id> <smps> <expected_burn_runs>"
    exit 1
fi

if [[ ! "$expected_burn_runs" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid expected burn run count: $expected_burn_runs"
    exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

report_file="artifacts/parallel-premerge-matrix-${matrix_id}.report.txt"

{
    echo "parallel-premerge-matrix-check"
    echo "id: $matrix_id"
    echo "smps: $smps_raw"
    echo "expected_burn_runs: $expected_burn_runs"
} > "$report_file"

failures=0

for smp in $smps_raw; do
    ci_id="matrix-${matrix_id}-smp${smp}"
    ci_dir="artifacts/parallel-probe-ci-${ci_id}"
    lane_fail=0

    safe_log="$ci_dir/qemu-serial.parallel-safe.log"
    strict_log="$ci_dir/qemu-serial.parallel-strict.log"
    strict_capture="$ci_dir/qemu-serial.parallel-strict.capture.log"
    burn_dir="$ci_dir/parallel-dispatch-burn-ci-${ci_id}"

    if [[ ! -s "$safe_log" ]]; then
        echo "smp${smp}: FAIL missing safe log" >> "$report_file"
        lane_fail=1
    fi
    if [[ ! -s "$strict_log" ]]; then
        echo "smp${smp}: FAIL missing strict log" >> "$report_file"
        lane_fail=1
    fi
    if [[ ! -s "$strict_capture" ]]; then
        echo "smp${smp}: FAIL strict capture missing/empty" >> "$report_file"
        lane_fail=1
    fi

    if [[ -s "$safe_log" ]]; then
        if ! rg -q "SHUTDOWN REQUESTED" "$safe_log"; then
            echo "smp${smp}: FAIL safe log missing SHUTDOWN REQUESTED" >> "$report_file"
            lane_fail=1
        fi
    fi

    if [[ -s "$strict_log" ]]; then
        if ! rg -q "PARALLELCANARY REQ" "$strict_log"; then
            echo "smp${smp}: FAIL strict log missing PARALLELCANARY REQ" >> "$report_file"
            lane_fail=1
        fi
        if ! rg -q "PARALLELCANARY EXE DELTA" "$strict_log"; then
            echo "smp${smp}: FAIL strict log missing PARALLELCANARY EXE DELTA" >> "$report_file"
            lane_fail=1
        fi
    fi

    if [[ ! -d "$burn_dir" ]]; then
        echo "smp${smp}: FAIL burn directory missing" >> "$report_file"
        lane_fail=1
    else
        for ((i = 1; i <= expected_burn_runs; i++)); do
            found_pass_for_iteration=0
            shopt -s nullglob
            for log_path in "$burn_dir"/qemu-serial.parallel-dispatch-burn-"${i}".attempt-*.log; do
                if [[ "$log_path" == *.capture.log ]]; then
                    continue
                fi
                if rg -q "PARALLELCANARY REQ" "$log_path" && rg -q "PARALLELCANARY EXE DELTA" "$log_path"; then
                    found_pass_for_iteration=1
                    break
                fi
            done
            shopt -u nullglob

            if (( found_pass_for_iteration == 0 )); then
                echo "smp${smp}: FAIL no passing burn attempt for iteration ${i}" >> "$report_file"
                lane_fail=1
            fi
        done
    fi

    if (( lane_fail == 0 )); then
        echo "smp${smp}: PASS" >> "$report_file"
    else
        failures=$((failures + 1))
    fi
done

if (( failures > 0 )); then
    echo "result: FAIL lanes=${failures}" >> "$report_file"
    echo "[matrix-check] FAIL report: $report_file"
    exit 1
fi

echo "result: PASS" >> "$report_file"
echo "[matrix-check] PASS report: $report_file"
