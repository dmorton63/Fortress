#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
  desktop_surface_contract_smoke.sh [timeout_seconds]

Runs boot smoke telemetry capture and validates desktop surface contract markers.
Generates two logs and enforces deterministic summary parity.
EOF
}

if [[ $# -gt 1 ]]; then
    usage
    exit 1
fi

timeout_seconds="${1:-40}"
if [[ ! "$timeout_seconds" =~ ^[0-9]+$ ]] || (( timeout_seconds < 10 )); then
    echo "timeout_seconds must be an integer >= 10"
    exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

mkdir -p build
log_a="build/qemu-serial.contract-a.log"
log_b="build/qemu-serial.contract-b.log"

run_capture() {
    local out_log="$1"
    rm -f "$out_log"
    echo "Capturing log: $out_log"
    if ! timeout "${timeout_seconds}s" make run-log QEMU_LOG="$out_log" >/dev/null 2>&1; then
        if [[ ! -s "$out_log" ]]; then
            echo "Run-log capture failed and no output log was produced: $out_log"
            exit 1
        fi
    fi
}

run_capture "$log_a"
run_capture "$log_b"

"$script_dir/desktop_surface_contract_smoke_check.sh" "$log_a" --compare "$log_b"
cp "$log_a" build/qemu-serial.log

echo "Contract smoke harness pass."
echo "Primary log copied to build/qemu-serial.log"
