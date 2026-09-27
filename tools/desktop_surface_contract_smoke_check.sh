#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage:
    desktop_surface_contract_smoke_check.sh LOGFILE [--compare OTHER_LOGFILE] [--expect-occlusion] [--token-audit] [--strict-fail-token-map] [--geometry-profile PROFILE]

Checks required desktop-surface telemetry markers and validates metric shape.
Optional compare mode checks deterministic parity of summary lines across two logs.
Optional occlusion mode enforces split counters expected for overlap subtraction scenarios.
Optional token audit maps legacy fail lines to contract error tokens.
Geometry profile mode enforces exact counter expectations for known deterministic scenarios.
EOF
}

if [[ $# -lt 1 ]]; then
    usage
    exit 1
fi

log_file="$1"
shift

compare_file=""
expect_occlusion=false
token_audit=false
strict_fail_token_map=false
geometry_profile=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --compare)
            if [[ $# -lt 2 ]]; then
                usage
                exit 1
            fi
            compare_file="$2"
            shift 2
            ;;
        --expect-occlusion)
            expect_occlusion=true
            shift
            ;;
        --token-audit)
            token_audit=true
            shift
            ;;
        --strict-fail-token-map)
            token_audit=true
            strict_fail_token_map=true
            shift
            ;;
        --geometry-profile)
            if [[ $# -lt 2 ]]; then
                usage
                exit 1
            fi
            geometry_profile="$2"
            shift 2
            ;;
        *)
            usage
            exit 1
            ;;
    esac
done

if [[ ! -s "$log_file" ]]; then
    echo "Missing or empty log file: $log_file"
    exit 1
fi

required_markers=(
    "DESKTOP SMOKE PASS"
    "DSKSURF SMOKE PASS"
    "DSKSURF OVERLAY ON"
    "DESKTOP S "
    "DESKTOP DIRTY TOP N "
    "DESKTOP SPLIT TOP N "
    "DESKTOP INPUT RX "
)

for marker in "${required_markers[@]}"; do
    if grep -Fq "$marker" "$log_file"; then
        echo "FOUND: $marker"
    else
        echo "MISSING: $marker"
        exit 1
    fi
done

last_desktop_s=$(grep -F "DESKTOP S " "$log_file" | tail -n 1)
last_dirty_top=$(grep -F "DESKTOP DIRTY TOP N " "$log_file" | tail -n 1)
last_split_top=$(grep -F "DESKTOP SPLIT TOP N " "$log_file" | tail -n 1)
last_input=$(grep -F "DESKTOP INPUT RX " "$log_file" | tail -n 1)

if [[ -z "$last_desktop_s" ]] || [[ -z "$last_dirty_top" ]] || [[ -z "$last_split_top" ]] || [[ -z "$last_input" ]]; then
    echo "Failed to collect required summary lines from $log_file"
    exit 1
fi

metric_from_line() {
    local line="$1"
    local key="$2"
    awk -v key="$key" '
        {
            gsub(/\r/, "")
            for (i = 1; i < NF; i++) {
                if ($i == key) {
                    print $(i + 1)
                    exit
                }
            }
        }
    ' <<<"$line"
}

assert_uint() {
    local value="$1"
    local name="$2"
    if [[ ! "$value" =~ ^[0-9]+$ ]]; then
        echo "Invalid integer for $name: '$value'"
        exit 1
    fi
}

surface_count=$(metric_from_line "$last_desktop_s" "S")
dirty_count=$(metric_from_line "$last_desktop_s" "D")
dirty_area=$(metric_from_line "$last_desktop_s" "DA")
ack_count=$(metric_from_line "$last_desktop_s" "ACK")
ack_pixels=$(metric_from_line "$last_desktop_s" "APX")
frame_ack_count=$(metric_from_line "$last_desktop_s" "FACK")
frame_ack_pixels=$(metric_from_line "$last_desktop_s" "FPX")
frame_fallback_count=$(metric_from_line "$last_desktop_s" "FFBK")
frame_fallback_pixels=$(metric_from_line "$last_desktop_s" "FFBP")
frame_split_attempts=$(metric_from_line "$last_desktop_s" "FSPL")
frame_frag_generated=$(metric_from_line "$last_desktop_s" "FGEN")
frame_frag_dropped=$(metric_from_line "$last_desktop_s" "FDROP")
highest_z=$(metric_from_line "$last_desktop_s" "Z")
component_count=$(metric_from_line "$last_desktop_s" "C")
tick_count=$(metric_from_line "$last_desktop_s" "T")
focus_id=$(metric_from_line "$last_desktop_s" "F")
capture_id=$(metric_from_line "$last_desktop_s" "CAP")
click_count=$(metric_from_line "$last_desktop_s" "CLK")

assert_uint "$surface_count" "S"
assert_uint "$dirty_count" "D"
assert_uint "$dirty_area" "DA"
assert_uint "$ack_count" "ACK"
assert_uint "$ack_pixels" "APX"
assert_uint "$frame_ack_count" "FACK"
assert_uint "$frame_ack_pixels" "FPX"
assert_uint "$frame_fallback_count" "FFBK"
assert_uint "$frame_fallback_pixels" "FFBP"
assert_uint "$frame_split_attempts" "FSPL"
assert_uint "$frame_frag_generated" "FGEN"
assert_uint "$frame_frag_dropped" "FDROP"
assert_uint "$highest_z" "Z"
assert_uint "$component_count" "C"
assert_uint "$tick_count" "T"
assert_uint "$focus_id" "F"
assert_uint "$capture_id" "CAP"
assert_uint "$click_count" "CLK"

if (( surface_count < 1 )); then
    echo "Unexpected S value ($surface_count)."
    exit 1
fi

if (( frame_frag_dropped > frame_frag_generated )); then
    echo "Invalid fragment counters: FDROP ($frame_frag_dropped) > FGEN ($frame_frag_generated)."
    exit 1
fi

normalize_line() {
    tr -d '\r' <<<"$1" | tr -s ' ' | sed 's/^ //; s/ $//'
}

map_fail_token_from_line() {
    local line="$(normalize_line "$1")"

    if [[ "$line" =~ ERR_DSKSURF_[A-Z0-9_]+ ]]; then
        grep -oE 'ERR_DSKSURF_[A-Z0-9_]+' <<<"$line" | head -n 1
        return 0
    fi

    case "$line" in
        "DSKSURFCREATE FAIL") echo "ERR_DSKSURF_CREATE_FAILED"; return 0 ;;
        "DSKSURFCLOSE FAIL") echo "ERR_DSKSURF_CLOSE_FAILED"; return 0 ;;
        "DSKSURFMOVE FAIL") echo "ERR_DSKSURF_MOVE_FAILED"; return 0 ;;
        "DSKSURFRESIZE FAIL") echo "ERR_DSKSURF_RESIZE_FAILED"; return 0 ;;
        "DSKSURFDAMAGE FAIL") echo "ERR_DSKSURF_DAMAGE_FAILED"; return 0 ;;
        "DESKTOPFOCUS FAIL") echo "ERR_DSKSURF_FOCUS_FAILED"; return 0 ;;
        "DESKTOPFOCUS NEXT FAIL") echo "ERR_DSKSURF_FOCUS_NEXT_FAILED"; return 0 ;;
        "DESKTOPCAPTURE FAIL") echo "ERR_DSKSURF_CAPTURE_FAILED"; return 0 ;;
        "DSKSURFDIRTY RANGE 1..8 OR ALL") echo "ERR_DSKSURF_DIRTY_RANGE"; return 0 ;;
        "DSKSURFINSPECT ARG INVALID") echo "ERR_DSKSURF_INSPECT_INVALID"; return 0 ;;
        "DSKSURFINSPECT NO FOCUS") echo "ERR_DSKSURF_INSPECT_NO_FOCUS"; return 0 ;;
        "DSKSURFINSPECT NO DATA") echo "ERR_DSKSURF_INSPECT_NO_DATA"; return 0 ;;
        "DESKTOPFOCUS ARG INVALID") echo "ERR_DSKSURF_FOCUS_INVALID"; return 0 ;;
        "DESKTOPCAPTURE ARG INVALID") echo "ERR_DSKSURF_CAPTURE_INVALID"; return 0 ;;
        "DSKSURFCREATE USAGE X Y W H Z") echo "ERR_DSKSURF_CREATE_USAGE"; return 0 ;;
        "DSKSURFMOVE USAGE ID X Y") echo "ERR_DSKSURF_MOVE_USAGE"; return 0 ;;
        "DSKSURFRESIZE USAGE ID W H") echo "ERR_DSKSURF_RESIZE_USAGE"; return 0 ;;
    esac

    return 1
}

extract_split_top_counts() {
    local line
    line="$(normalize_line "$1")"
    awk '
        {
            for (i = 1; i < NF; i++) {
                if ($i == "CNT") {
                    printf("%s%s", (seen ? " " : ""), $(i + 1));
                    seen = 1;
                }
            }
            if (!seen) {
                printf("")
            }
        }
    ' <<<"$line"
}

summary_sig=$(
    {
        normalize_line "$last_desktop_s"
        normalize_line "$last_dirty_top"
        normalize_line "$last_split_top"
        normalize_line "$last_input"
    } | sha256sum | awk '{print $1}'
)

echo "SUMMARY: S=$surface_count D=$dirty_count DA=$dirty_area ACK=$ack_count APX=$ack_pixels"
echo "SUMMARY: FACK=$frame_ack_count FPX=$frame_ack_pixels FFBK=$frame_fallback_count FFBP=$frame_fallback_pixels"
echo "SUMMARY: FSPL=$frame_split_attempts FGEN=$frame_frag_generated FDROP=$frame_frag_dropped"
echo "SUMMARY: Z=$highest_z C=$component_count T=$tick_count F=$focus_id CAP=$capture_id CLK=$click_count"
echo "SIGNATURE: $summary_sig"

if $expect_occlusion; then
    split_top_n=$(metric_from_line "$last_split_top" "N")
    dirty_top_n=$(metric_from_line "$last_dirty_top" "N")
    assert_uint "$split_top_n" "SPLIT_TOP_N"
    assert_uint "$dirty_top_n" "DIRTY_TOP_N"

    if (( frame_split_attempts < 1 )); then
        echo "Occlusion expectation failed: FSPL must be > 0."
        exit 1
    fi
    if (( frame_frag_generated < 1 )); then
        echo "Occlusion expectation failed: FGEN must be > 0."
        exit 1
    fi
    if (( split_top_n < 1 )); then
        echo "Occlusion expectation failed: DESKTOP SPLIT TOP N must be > 0."
        exit 1
    fi
    if (( frame_ack_count < 1 )) && (( dirty_top_n < 1 )); then
        echo "Occlusion expectation failed: expected ACK activity or DESKTOP DIRTY TOP contributors."
        exit 1
    fi
    echo "OCCLUSION CHECK: PASS (FSPL=$frame_split_attempts FGEN=$frame_frag_generated TOP_SPLIT_N=$split_top_n)"
fi

if [[ -n "$geometry_profile" ]]; then
    case "$geometry_profile" in
        boot-occlusion-v1)
            split_top_n=$(metric_from_line "$last_split_top" "N")
            assert_uint "$split_top_n" "SPLIT_TOP_N"
            split_counts="$(extract_split_top_counts "$last_split_top")"

            if (( frame_split_attempts != 15 )); then
                echo "Geometry profile boot-occlusion-v1 failed: expected FSPL=15, got $frame_split_attempts"
                exit 1
            fi
            if (( frame_frag_generated != 19 )); then
                echo "Geometry profile boot-occlusion-v1 failed: expected FGEN=19, got $frame_frag_generated"
                exit 1
            fi
            if (( frame_frag_dropped != 0 )); then
                echo "Geometry profile boot-occlusion-v1 failed: expected FDROP=0, got $frame_frag_dropped"
                exit 1
            fi
            if (( split_top_n != 3 )); then
                echo "Geometry profile boot-occlusion-v1 failed: expected split-top N=3, got $split_top_n"
                exit 1
            fi
            if [[ "$split_counts" != "6 5 3" ]]; then
                echo "Geometry profile boot-occlusion-v1 failed: expected split-top CNT sequence '6 5 3', got '$split_counts'"
                exit 1
            fi
            echo "GEOMETRY PROFILE: PASS (boot-occlusion-v1 FSPL=15 FGEN=19 FDROP=0 CNT=6 5 3)"
            ;;
        boot-fallback-v1)
            split_top_n=$(metric_from_line "$last_split_top" "N")
            assert_uint "$split_top_n" "SPLIT_TOP_N"
            split_counts="$(extract_split_top_counts "$last_split_top")"

            if (( frame_fallback_count != 1 )); then
                echo "Geometry profile boot-fallback-v1 failed: expected FFBK=1, got $frame_fallback_count"
                exit 1
            fi
            if (( frame_fallback_pixels != 79200 )); then
                echo "Geometry profile boot-fallback-v1 failed: expected FFBP=79200, got $frame_fallback_pixels"
                exit 1
            fi
            if (( frame_ack_count != 4 )); then
                echo "Geometry profile boot-fallback-v1 failed: expected FACK=4, got $frame_ack_count"
                exit 1
            fi
            if (( frame_ack_pixels != 198400 )); then
                echo "Geometry profile boot-fallback-v1 failed: expected FPX=198400, got $frame_ack_pixels"
                exit 1
            fi
            if (( frame_split_attempts != 15 )); then
                echo "Geometry profile boot-fallback-v1 failed: expected FSPL=15, got $frame_split_attempts"
                exit 1
            fi
            if (( frame_frag_generated != 19 )); then
                echo "Geometry profile boot-fallback-v1 failed: expected FGEN=19, got $frame_frag_generated"
                exit 1
            fi
            if (( frame_frag_dropped != 0 )); then
                echo "Geometry profile boot-fallback-v1 failed: expected FDROP=0, got $frame_frag_dropped"
                exit 1
            fi
            if (( split_top_n != 3 )); then
                echo "Geometry profile boot-fallback-v1 failed: expected split-top N=3, got $split_top_n"
                exit 1
            fi
            if [[ "$split_counts" != "6 5 3" ]]; then
                echo "Geometry profile boot-fallback-v1 failed: expected split-top CNT sequence '6 5 3', got '$split_counts'"
                exit 1
            fi
            echo "GEOMETRY PROFILE: PASS (boot-fallback-v1 FFBK=1 FFBP=79200 FACK=4 FPX=198400 FSPL=15 FGEN=19 FDROP=0 CNT=6 5 3)"
            ;;
        *)
            echo "Unknown geometry profile: $geometry_profile"
            exit 1
            ;;
    esac
fi

if $token_audit; then
    echo "--- fail token audit ---"
    tmp_fail_lines="$(mktemp)"
    trap 'rm -f "$tmp_fail_lines"' EXIT
    grep -E 'ERR_DSKSURF_|DSKSURF[A-Z]+ (FAIL|USAGE|RANGE)|DSKSURFINSPECT (ARG INVALID|NO FOCUS|NO DATA)|DESKTOPFOCUS (FAIL|NEXT FAIL|ARG INVALID)|DESKTOPCAPTURE (FAIL|ARG INVALID)' "$log_file" > "$tmp_fail_lines" || true

    if [[ ! -s "$tmp_fail_lines" ]]; then
        echo "No fail token lines found."
    else
        token_total=0
        unknown_total=0
        while IFS= read -r fail_line; do
            normalized="$(normalize_line "$fail_line")"
            if mapped="$(map_fail_token_from_line "$normalized")"; then
                token_total=$((token_total + 1))
                echo "TOKEN: $mapped <= $normalized"
            else
                unknown_total=$((unknown_total + 1))
                echo "TOKEN: UNKNOWN <= $normalized"
            fi
        done < "$tmp_fail_lines"

        echo "TOKEN SUMMARY: mapped=$token_total unknown=$unknown_total"
        if $strict_fail_token_map && (( unknown_total > 0 )); then
            echo "Strict fail token mapping failed: unknown token lines present."
            exit 1
        fi
    fi
fi

if [[ -n "$compare_file" ]]; then
    if [[ ! -s "$compare_file" ]]; then
        echo "Missing or empty compare log file: $compare_file"
        exit 1
    fi

    cmp_desktop_s=$(grep -F "DESKTOP S " "$compare_file" | tail -n 1)
    cmp_dirty_top=$(grep -F "DESKTOP DIRTY TOP N " "$compare_file" | tail -n 1)
    cmp_split_top=$(grep -F "DESKTOP SPLIT TOP N " "$compare_file" | tail -n 1)
    cmp_input=$(grep -F "DESKTOP INPUT RX " "$compare_file" | tail -n 1)

    if [[ -z "$cmp_desktop_s" ]] || [[ -z "$cmp_dirty_top" ]] || [[ -z "$cmp_split_top" ]] || [[ -z "$cmp_input" ]]; then
        echo "Compare log missing required summary lines: $compare_file"
        exit 1
    fi

    compare_sig=$(
        {
            normalize_line "$cmp_desktop_s"
            normalize_line "$cmp_dirty_top"
            normalize_line "$cmp_split_top"
            normalize_line "$cmp_input"
        } | sha256sum | awk '{print $1}'
    )

    echo "COMPARE SIGNATURE: $compare_sig"
    if [[ "$summary_sig" != "$compare_sig" ]]; then
        echo "Determinism check failed: signature mismatch."
        echo "PRIMARY:  $summary_sig"
        echo "COMPARE:  $compare_sig"
        echo "PRIMARY DESKTOP S: $last_desktop_s"
        echo "COMPARE DESKTOP S: $cmp_desktop_s"
        exit 1
    fi
    echo "Determinism check passed."
fi
