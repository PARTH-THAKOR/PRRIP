#!/bin/bash

L2C_REPLACEMENT="prrip"
BINARY_NAME="prrip"
WARMUP_INSTRUCTIONS=1000000
SIMULATION_INSTRUCTIONS=10000000
TRACE_FOLDER="traces"
TRACE_NAME="chirper.gz"
L2_CSV="chirper_DIP_1k.csv"

set -e
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

fail() {
    printf '[ERROR] %s\n' "$1" >&2
    exit 1
}

[ "$#" -eq 0 ] || fail "Edit the settings at the top of this script; no command-line arguments are needed."
case "$BINARY_NAME" in
    ""|.|..|*[!a-zA-Z0-9_.-]*) fail "Invalid binary name: $BINARY_NAME" ;;
esac
case "$L2C_REPLACEMENT" in
    ""|.|..|*[!a-zA-Z0-9_.-]*) fail "Invalid L2 replacement policy: $L2C_REPLACEMENT" ;;
esac
[[ "$WARMUP_INSTRUCTIONS" =~ ^[0-9]+$ ]] || fail "Warmup instructions must be a non-negative integer."
[[ "$SIMULATION_INSTRUCTIONS" =~ ^[1-9][0-9]*$ ]] || fail "Simulation instructions must be a positive integer."
TRACE_PATH="${TRACE_FOLDER}/${TRACE_NAME}"
[ -r "$TRACE_PATH" ] || fail "Trace not found or unreadable: $TRACE_PATH"
case "$TRACE_NAME" in
    *.gz) command -v gunzip >/dev/null 2>&1 || fail "gunzip is required for this trace." ;;
    *.xz) command -v xz >/dev/null 2>&1 || fail "xz is required for this trace." ;;
    *) fail "Trace must use .gz or .xz compression." ;;
esac

if [ "$L2C_REPLACEMENT" = base ] || [ "$L2C_REPLACEMENT" = fixed ]; then
    command -v python3 >/dev/null 2>&1 || fail "python3 is required to write the policy CSV."
    [ -f scripts/save_l2_windows.py ] || fail "scripts/save_l2_windows.py was not found."
fi

# All prefetchers remain disabled. Policy templates stay in replacement/.
sources=(branch/hashed_perceptron.bpred)
targets=(branch/branch_predictor.cc)
for level in l1i l1d l2c llc itlb dtlb stlb; do
    sources+=("prefetcher/no.${level}_pref")
    targets+=("prefetcher/${level}_prefetcher.cc")
done
for level in btb l1i l1d l2c llc itlb dtlb stlb; do
    if [ "$level" = l2c ]; then
        sources+=("replacement/${L2C_REPLACEMENT}.l2c.cc")
    else
        sources+=("replacement/lru.${level}.cc")
    fi
    targets+=("replacement/${level}_replacement.cc")
done
for source in "${sources[@]}"; do
    [ -f "$source" ] || fail "Required source file not found: $source"
done
for index in "${!sources[@]}"; do
    cp "${sources[$index]}" "${targets[$index]}"
done

# Hide successful build output, including warnings. Show diagnostics on failure.
build_log=$(mktemp "${TMPDIR:-/tmp}/prrip-build.XXXXXX")
trap 'rm -f "$build_log"' EXIT
printf 'Building Binary...\n'
if ! { make clean && make app="$BINARY_NAME"; } > /dev/null 2> "$build_log"; then
    printf '[ERROR] Build failed; simulation was not started.\n' >&2
    cat "$build_log" >&2
    exit 1
fi
[ -x "bin/$BINARY_NAME" ] || fail "Build did not produce bin/$BINARY_NAME."

# Restore the default generated policy after the binary has been built.
cp replacement/lru.l2c.cc replacement/l2c_replacement.cc

# A successful build automatically starts the configured simulation.
run_command=("./bin/$BINARY_NAME")
if [ "$L2C_REPLACEMENT" = base ] || [ "$L2C_REPLACEMENT" = fixed ]; then
    export L2_CSV
    run_command=(python3 ./scripts/save_l2_windows.py "./bin/$BINARY_NAME")
fi
"${run_command[@]}" \
    -warmup_instructions "$WARMUP_INSTRUCTIONS" \
    -simulation_instructions "$SIMULATION_INSTRUCTIONS" \
    -traces "$TRACE_PATH"
