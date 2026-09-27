#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
    post_merge_hardware_soak_archive.sh [serial_log] [capture_log]

Inputs can also be provided via environment variables:
    HW_SOAK_LOG=/path/to/serial.log
    HW_SOAK_CAPTURE_LOG=/path/to/capture.log (optional)
    HW_SOAK_ID=amd-20260926-run1 (optional)

Outputs:
    artifacts/hardware-soak-<id>/
    artifacts/hardware-soak-<id>.tar.gz
EOF
}

serial_log="${1:-${HW_SOAK_LOG:-}}"
capture_log="${2:-${HW_SOAK_CAPTURE_LOG:-}}"
soak_id="${HW_SOAK_ID:-$(date +%Y%m%d-%H%M%S)}"

if [[ -z "$serial_log" ]]; then
    echo "Missing serial log path"
    usage
    exit 1
fi

if [[ ! -f "$serial_log" ]]; then
    echo "Serial log not found: $serial_log"
    exit 1
fi

if [[ -n "$capture_log" && ! -f "$capture_log" ]]; then
    echo "Capture log not found: $capture_log"
    exit 1
fi

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

artifact_dir="artifacts/hardware-soak-${soak_id}"
archive_path="artifacts/hardware-soak-${soak_id}.tar.gz"
mkdir -p "$artifact_dir"

serial_basename="$(basename "$serial_log")"
cp "$serial_log" "$artifact_dir/$serial_basename"

capture_basename=""
if [[ -n "$capture_log" ]]; then
    capture_basename="$(basename "$capture_log")"
    cp "$capture_log" "$artifact_dir/$capture_basename"
fi

summary_file="$artifact_dir/summary.txt"
missing=0

{
    echo "hardware-soak-summary"
    echo "id: $soak_id"
    echo "serial_log: $serial_basename"
    if [[ -n "$capture_basename" ]]; then
        echo "capture_log: $capture_basename"
    else
        echo "capture_log: none"
    fi
    echo ""
    echo "required_markers:"
} > "$summary_file"

required_markers=(
    "PARALLELPROBE MODE"
    "PARALLEL ON AP_DRAIN"
    "PARALLELCANARY REQ"
    "PARALLELCANARY EXE DELTA"
    "APW START"
)

for marker in "${required_markers[@]}"; do
    if rg -q "$marker" "$artifact_dir/$serial_basename"; then
        echo "- PASS: $marker" >> "$summary_file"
    else
        echo "- FAIL: $marker" >> "$summary_file"
        missing=1
    fi
done

warn_count=$(rg -c "PARALLELPROBE WARN AP_DRAIN NOEXEC STREAK" "$artifact_dir/$serial_basename" || true)
pause_count=$(rg -c "XHCI INTRIN BG PAUSED AP_DRAIN" "$artifact_dir/$serial_basename" || true)
resume_count=$(rg -c "XHCI INTRIN BG RESUME" "$artifact_dir/$serial_basename" || true)
warn_count=${warn_count:-0}
pause_count=${pause_count:-0}
resume_count=${resume_count:-0}

{
    echo ""
    echo "diagnostics:"
    echo "- PARALLELPROBE_WARN_NOEXEC: $warn_count"
    echo "- XHCI_BG_PAUSED_AP_DRAIN: $pause_count"
    echo "- XHCI_BG_RESUME: $resume_count"
} >> "$summary_file"

result="PASS"
if [[ $missing -ne 0 ]]; then
    result="FAIL"
fi

echo "" >> "$summary_file"
echo "result: $result" >> "$summary_file"

tar -czf "$archive_path" -C artifacts "hardware-soak-${soak_id}"

echo "[hw-soak] summary: $summary_file"
echo "[hw-soak] archive: $archive_path"

if [[ "$result" == "FAIL" ]]; then
    echo "[hw-soak] required markers missing"
    exit 1
fi

echo "[hw-soak] PASS"
