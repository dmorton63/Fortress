#!/usr/bin/env bash
set -euo pipefail

LOG_PATH="${1:-build/qemu-serial.log}"
require_pending_drain=false
drain_probe_window=3
expect_mode="probe"
allow_canary_blocked=false
allow_probe_canary_blocked=false
require_positive_dpt=true
skip_canary_check=false
require_shutdown=false

if [[ $# -ge 2 ]]; then
    shift
    while [[ $# -gt 0 ]]; do
        case "$1" in
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
            --expect-mode)
                if [[ $# -lt 2 ]]; then
                    echo "Missing value for --expect-mode"
                    exit 1
                fi
                expect_mode="$2"
                shift 2
                ;;
            --allow-canary-blocked)
                allow_canary_blocked=true
                shift
                ;;
            --allow-probe-canary-blocked)
                allow_probe_canary_blocked=true
                shift
                ;;
            --allow-zero-dpt)
                require_positive_dpt=false
                shift
                ;;
            --skip-canary-check)
                skip_canary_check=true
                shift
                ;;
            --require-shutdown)
                require_shutdown=true
                shift
                ;;
            *)
                echo "Unknown argument: $1"
                exit 1
                ;;
        esac
    done
fi

if [[ ! "$drain_probe_window" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid --drain-window value: $drain_probe_window"
    exit 1
fi

if [[ "$expect_mode" != "probe" && "$expect_mode" != "drain" && "$expect_mode" != "either" ]]; then
    echo "Invalid --expect-mode value: $expect_mode"
    exit 1
fi

if [[ ! -s "$LOG_PATH" ]]; then
    echo "Missing or empty log: $LOG_PATH"
    exit 1
fi

count_help=$(grep -Fc "TYPE HELP FOR COMMANDS" "$LOG_PATH" || true)
if [[ "$count_help" -ne 1 ]]; then
    echo "Unexpected boot banner count: $count_help (expected 1)"
    exit 1
fi

required_markers=(
    "PARALLEL ON AP_PARKED"
    "PARALLEL DRAIN ON"
    "PARALLELPROBE MODE"
    "PARALLELPROBE AP START"
)

for marker in "${required_markers[@]}"; do
    if ! grep -Fq "$marker" "$LOG_PATH"; then
        echo "Missing marker: $marker"
        exit 1
    fi
    echo "FOUND: $marker"
done

if [[ "$expect_mode" == "probe" ]]; then
    if ! grep -Fq "PARALLEL ON AP_PROBE" "$LOG_PATH"; then
        echo "Missing marker: PARALLEL ON AP_PROBE"
        exit 1
    fi
    echo "FOUND: PARALLEL ON AP_PROBE"
elif [[ "$expect_mode" == "drain" ]]; then
    if ! grep -Fq "PARALLEL ON AP_DRAIN" "$LOG_PATH"; then
        echo "Missing marker: PARALLEL ON AP_DRAIN"
        exit 1
    fi
    echo "FOUND: PARALLEL ON AP_DRAIN"
else
    if grep -Fq "PARALLEL ON AP_PROBE" "$LOG_PATH"; then
        echo "FOUND: PARALLEL ON AP_PROBE"
    elif grep -Fq "PARALLEL ON AP_DRAIN" "$LOG_PATH"; then
        echo "FOUND: PARALLEL ON AP_DRAIN"
    else
        echo "Missing marker: PARALLEL ON AP_PROBE|PARALLEL ON AP_DRAIN"
        exit 1
    fi
fi

if [[ "$skip_canary_check" == false ]]; then
    if [[ "$allow_canary_blocked" == true ]] && grep -Fq "PARALLELCANARY BLOCKED AP_DRAIN_UNSAFE" "$LOG_PATH"; then
        echo "FOUND: PARALLELCANARY BLOCKED AP_DRAIN_UNSAFE"
    elif [[ "$allow_canary_blocked" == true ]] && grep -Fq "PARALLELCANARY BLOCKED AP_DRAIN_ENQUEUE_UNSTABLE" "$LOG_PATH"; then
        echo "FOUND: PARALLELCANARY BLOCKED AP_DRAIN_ENQUEUE_UNSTABLE"
    elif [[ "$allow_probe_canary_blocked" == true ]] && grep -Fq "PARALLELCANARY BLOCKED AP_PROBE_NO_DISPATCH" "$LOG_PATH"; then
        echo "FOUND: PARALLELCANARY BLOCKED AP_PROBE_NO_DISPATCH"
        if grep -Fq "PARALLELCANARY AP_PARKED" "$LOG_PATH"; then
            echo "FOUND: PARALLELCANARY AP_PARKED"
        elif grep -Fq "PARALLELCANARY AP_PARK_FAIL" "$LOG_PATH"; then
            echo "Canary fallback to AP_PARKED failed"
            exit 1
        else
            echo "Missing marker: PARALLELCANARY AP_PARKED"
            exit 1
        fi
    else
        for marker in "PARALLELCANARY REQ" "PARALLELCANARY EXE DELTA"; do
            if ! grep -Fq "$marker" "$LOG_PATH"; then
                if [[ "$marker" == "PARALLELCANARY REQ" ]] &&
                   grep -Fq "PARALLELCANARY ENQ ITEM" "$LOG_PATH" &&
                   ! grep -Fq "SHUTDOWN REQUESTED" "$LOG_PATH"; then
                    echo "Missing marker: $marker"
                    echo "Likely crash window: after PARALLELCANARY ENQ ITEM and before shutdown"
                    exit 1
                fi
                echo "Missing marker: $marker"
                exit 1
            fi
            echo "FOUND: $marker"
        done
    fi
fi

if [[ "$require_shutdown" == true ]]; then
    if ! grep -Fq "SHUTDOWN REQUESTED" "$LOG_PATH"; then
        echo "Missing marker: SHUTDOWN REQUESTED"
        exit 1
    fi
    echo "FOUND: SHUTDOWN REQUESTED"
fi

if grep -Fq "PARALLELCANARY EXE TOTAL" "$LOG_PATH"; then
    echo "Legacy canary marker detected: PARALLELCANARY EXE TOTAL"
    exit 1
fi

if [[ "$require_positive_dpt" == true ]]; then
    if ! awk '
        /PARALLELPROBE MODE/ {
            for (i = 1; i <= NF; ++i) {
                if ($i == "DPT" && (i + 1) <= NF) {
                    if ($(i + 1) + 0 > 0) {
                        found = 1
                    }
                }
            }
        }
        END { exit found ? 0 : 1 }
    ' "$LOG_PATH"; then
        echo "No PARALLELPROBE MODE line reported DPT > 0"
        exit 1
    fi
fi

if [[ "$require_pending_drain" == true ]]; then
    if awk -v window="$drain_probe_window" '
        BEGIN {
            probeCount = 0
        }
        /PARALLELCANARY REQ/ && canaryLine == 0 {
            canaryLine = NR
        }
        /PARALLELPROBE AP START/ {
            pend = ""
            for (i = 1; i <= NF; ++i) {
                if ($i == "PEND" && (i + 1) <= NF) {
                    pend = $(i + 1) + 0
                }
            }
            if (pend != "" && canaryLine > 0 && NR > canaryLine) {
                probePend[probeCount] = pend
                probeCount++
            }
        }
        END {
            if (probeCount == 0) {
                exit 2
            }

            first = probePend[0]
            if (first == 0) {
                exit 0
            }

            limit = probeCount
            if (limit > window) {
                limit = window
            }

            for (i = 1; i < limit; ++i) {
                if (probePend[i] < first || probePend[i] == 0) {
                    exit 0
                }
            }

            exit 1
        }
    ' "$LOG_PATH"; then
        :
    else
        awk_status=$?
        if [[ $awk_status -eq 2 ]]; then
            echo "No PARALLELPROBE AP START samples found after PARALLELCANARY REQ"
        else
            echo "Pending canary work did not drain within ${drain_probe_window} PARALLELPROBE sample(s)"
            exit 1
        fi
        exit 1
    fi
fi

if grep -Eq "reboot|reset|triple fault" "$LOG_PATH"; then
    echo "Reset/reboot indicator detected in log"
    exit 1
fi

echo "Parallel probe smoke check passed"
