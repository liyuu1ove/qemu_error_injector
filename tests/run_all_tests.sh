#!/bin/bash
#
# run_all_tests.sh - Comprehensive test runner for QEMU error injector
# Runs all test programs against corruption modes
#
# To match manual command behavior, use:
#   ./run_all_tests.sh -a -p 5 -m 0 -t test_db_sim -v
#
# This is equivalent to:
#   QEMU_INJECT_ADD=1 QEMU_INJECT_ADD_PROB=50 QEMU_INJECT_MODE=0 \
#   ../qemu/build/qemu-x86_64 ./test_db_sim

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QEMU_BIN="${QEMU_BIN:-../../qemu/build/qemu-x86_64}"
TEST_DIR="${SCRIPT_DIR}"

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test programs
TESTS=(
    "test_numeric"
    "test_string"
    "test_crypto"
    "test_db_sim"
    "test_io_buffer"
    "test_file_io_hex"
    "test_AES"
)

# Corruption modes
declare -A MODES
MODES[0]="add_one"
MODES[1]="bitflip"
MODES[2]="zero_byte"
MODES[3]="invert"
MODES[4]="random_delta"

# Default injection parameters - match manual command defaults
PROB="${QEMU_INJECT_ADD_PROB:-50}"
SKIP="${QEMU_INJECT_ADD_SKIP:-0}"
SEED="${QEMU_INJECT_ADD_SEED:-42}"
MIN_PC="${QEMU_INJECT_ADD_MIN_PC:-}"
MAX_PC="${QEMU_INJECT_ADD_MAX_PC:-}"

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -t TEST     Run specific test (${TESTS[*]})"
    echo "  -m MODE     Run specific mode (0-4). Default: runs mode 0 only"
    echo "  -M          Run ALL modes (0-4)"
    echo "  -p PROB     Injection probability percentage (default: $PROB)"
    echo "  -s SKIP     Skip first N ADD operations (default: $SKIP)"
    echo "  -S SEED     Random seed (default: $SEED)"
    echo "  -P MIN:MAX  PC range filter (e.g., -P 0x401775:0x401900)"
    echo "  -n          Native run only (no QEMU)"
    echo "  -c          Compile tests only"
    echo "  -a          Aggressive mode: inject into ALL code (no PC filter)"
    echo "  -v          Verbose: show [INJECT] debug messages"
    echo "  -b          Baseline only: show native run, no injection"
    echo "  -h          Show this help"
    echo ""
    echo "Modes:"
    echo "  0 - add_one:      Add +1 to result (subtle off-by-one)"
    echo "  1 - bitflip:      Flip random bit (position 0-31)"
    echo "  2 - zero_byte:    Zero low byte (null-terminator simulation)"
    echo "  3 - invert:       Invert all bits (catastrophic)"
    echo "  4 - random_delta: Random delta -16 to +16"
    echo ""
    echo "Examples:"
    echo "  $0 -a -p 50 -m 0 -t test_db_sim       # Same as manual command"
    echo "  $0 -a -p 10 -m 1 -v                   # Aggressive, bitflip, verbose"
    echo "  $0 -p 50 -t test_crypto               # Safe mode with auto PC range"
    echo "  $0 -P 0x401775:0x401900 -p 50 -m 0 -t test_db_sim  # Manual PC range"
    echo ""
    echo "To match this manual command:"
    echo "  QEMU_INJECT_ADD=1 QEMU_INJECT_ADD_PROB=50 QEMU_INJECT_ADD_SEED=42 \\"
    echo "  QEMU_INJECT_MODE=0 ../qemu/build/qemu-x86_64 ./test_db_sim"
    echo ""
    echo "Use:"
    echo "  $0 -a -p 50 -S 42 -m 0 -t test_db_sim -v"
}

compile_tests() {
    echo -e "${BLUE}=== Compiling Test Programs ===${NC}"
    cd "$TEST_DIR"
    
    for test in "${TESTS[@]}"; do
        src="${test}.c"
        if [ -f "$src" ]; then
            echo -n "Compiling $src... "
            if gcc -O0 -g -static -o "$test" "$src" 2>/dev/null; then
                echo -e "${GREEN}OK${NC}"
            else
                echo -e "${RED}FAILED${NC}"
                return 1
            fi
        else
            echo -e "${YELLOW}SKIP${NC} ($src not found)"
        fi
    done
    echo ""
    return 0
}

run_native() {
    local test=$1
    echo -e "${YELLOW}--- Native Run: $test ---${NC}"
    if [ -x "${TEST_DIR}/${test}" ]; then
        "${TEST_DIR}/${test}"
    else
        echo -e "${RED}Test binary not found: ${test}${NC}"
        return 1
    fi
    echo ""
}

# Get PC range for target function using objdump
get_pc_range() {
    local binary=$1
    local func=$2
    
    # Get start address of function
    local start=$(objdump -d "$binary" 2>/dev/null | grep "<${func}>:" | head -1 | awk '{print $1}')
    
    if [ -z "$start" ]; then
        echo ""
        return
    fi
    
    # Estimate end address (start + 0x200 is usually enough for our small functions)
    local start_dec=$((16#$start))
    local end_dec=$((start_dec + 512))
    local end=$(printf "%x" $end_dec)
    
    echo "0x$start 0x$end"
}

# Map test names to their target functions
get_target_function() {
    local test=$1
    case $test in
        test_numeric)    echo "compute_sum_unrolled" ;;
        test_string)     echo "fill_string_unrolled" ;;
        test_crypto)     echo "compute_hash" ;;
        test_db_sim)     echo "fill_records" ;;
        test_io_buffer)  echo "fill_buffer_unrolled" ;;
        test_file_io_hex) echo "fill_pattern_unrolled" ;;
        test_AES) echo "AddRoundKey" ;;
        *)               echo "" ;;
    esac
}

run_with_injection() {
    local test=$1
    local mode=$2
    local mode_name=${MODES[$mode]}
    
    echo -e "${BLUE}--- QEMU Injection: $test (mode=$mode: $mode_name) ---${NC}"
    echo -e "  Settings: PROB=${PROB}% SKIP=${SKIP} SEED=${SEED}"
    
    if [ ! -x "$QEMU_BIN" ]; then
        echo -e "${RED}QEMU binary not found: $QEMU_BIN${NC}"
        return 1
    fi
    
    if [ ! -x "${TEST_DIR}/${test}" ]; then
        echo -e "${RED}Test binary not found: ${test}${NC}"
        return 1
    fi
    
    # Build the command
    local cmd_prefix="QEMU_INJECT_ADD=1"
    cmd_prefix="$cmd_prefix QEMU_INJECT_ADD_PROB=$PROB"
    cmd_prefix="$cmd_prefix QEMU_INJECT_ADD_SKIP=$SKIP"
    cmd_prefix="$cmd_prefix QEMU_INJECT_ADD_SEED=$SEED"
    cmd_prefix="$cmd_prefix QEMU_INJECT_MODE=$mode"
    
    # Determine PC range
    local use_min_pc=""
    local use_max_pc=""
    
    if [ -n "$MIN_PC" ] && [ -n "$MAX_PC" ]; then
        # Manual PC range specified via -P flag
        use_min_pc="$MIN_PC"
        use_max_pc="$MAX_PC"
        echo -e "  PC Range (manual): ${use_min_pc}-${use_max_pc}"
    elif [ "$AGGRESSIVE_MODE" -eq 1 ]; then
        # Aggressive mode: no PC filtering
        echo -e "${RED}  Injecting into ALL code (no PC filter)${NC}"
    else
        # Safe mode: auto-detect PC range
        local target_func=$(get_target_function "$test")
        if [ -n "$target_func" ]; then
            local pc_range=$(get_pc_range "${TEST_DIR}/${test}" "$target_func")
            if [ -n "$pc_range" ]; then
                use_min_pc=$(echo $pc_range | awk '{print $1}')
                use_max_pc=$(echo $pc_range | awk '{print $2}')
                echo -e "  Target: ${target_func}() at ${use_min_pc}-${use_max_pc}"
            else
                echo -e "${YELLOW}  Warning: Could not find ${target_func}(), no PC filter${NC}"
            fi
        else
            echo -e "${YELLOW}  Warning: No target function for ${test}, no PC filter${NC}"
        fi
    fi
    
    # Add PC range to command if we have it
    if [ -n "$use_min_pc" ] && [ -n "$use_max_pc" ]; then
        cmd_prefix="$cmd_prefix QEMU_INJECT_ADD_MIN_PC=$use_min_pc"
        cmd_prefix="$cmd_prefix QEMU_INJECT_ADD_MAX_PC=$use_max_pc"
    fi
    
    # Show the equivalent manual command
    if [ "$VERBOSE" -eq 1 ]; then
        echo -e "  Command: ${cmd_prefix} $QEMU_BIN ${TEST_DIR}/${test}"
    fi
    
    # Run the command
    echo ""
    if [ "$VERBOSE" -eq 1 ]; then
        # Show all output including [INJECT] messages
        eval "$cmd_prefix $QEMU_BIN ${TEST_DIR}/${test}"
    else
        # Filter out [INJECT] messages
        eval "$cmd_prefix $QEMU_BIN ${TEST_DIR}/${test}" 2>&1 | grep -v '^\[INJECT\]'
    fi
    
    local exit_code=$?
    if [ $exit_code -ne 0 ]; then
        echo -e "${RED}  >>> CRASHED/FAILED! Exit code: $exit_code <<<${NC}"
    fi
    
    echo ""
    return $exit_code
}

run_comparison() {
    local test=$1
    local mode=$2
    
    echo -e "${GREEN}======================================${NC}"
    echo -e "${GREEN}Test: $test | Mode: $mode (${MODES[$mode]})${NC}"
    if [ "$AGGRESSIVE_MODE" -eq 1 ]; then
        echo -e "${RED}AGGRESSIVE MODE: Injecting into ALL code (may crash!)${NC}"
    else
        echo -e "${GREEN}SAFE MODE: Injecting only into target functions${NC}"
    fi
    echo -e "${GREEN}======================================${NC}"
    echo ""
    
    echo -e "${YELLOW}[BASELINE - Native]${NC}"
    run_native "$test"
    
    echo -e "${YELLOW}[INJECTED - QEMU]${NC}"
    run_with_injection "$test" "$mode"
}

# Parse arguments
SPECIFIC_TEST=""
SPECIFIC_MODE=""
NATIVE_ONLY=0
COMPILE_ONLY=0
AGGRESSIVE_MODE=0
VERBOSE=0
RUN_ALL_MODES=0
BASELINE_ONLY=0

while getopts "t:m:p:s:S:P:ncavMbh" opt; do
    case $opt in
        t) SPECIFIC_TEST=$OPTARG ;;
        m) SPECIFIC_MODE=$OPTARG ;;
        M) RUN_ALL_MODES=1 ;;
        p) PROB=$OPTARG ;;
        s) SKIP=$OPTARG ;;
        S) SEED=$OPTARG ;;
        P) 
            # Parse PC range like "0x401775:0x401900"
            MIN_PC=$(echo "$OPTARG" | cut -d: -f1)
            MAX_PC=$(echo "$OPTARG" | cut -d: -f2)
            ;;
        n) NATIVE_ONLY=1 ;;
        c) COMPILE_ONLY=1 ;;
        a) AGGRESSIVE_MODE=1 ;;
        v) VERBOSE=1 ;;
        b) BASELINE_ONLY=1 ;;
        h) usage; exit 0 ;;
        ?) usage; exit 1 ;;
    esac
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  QEMU Error Injector Test Suite${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "Configuration:"
echo "  QEMU Binary: $QEMU_BIN"
echo "  Probability: ${PROB}%"
echo "  Skip Count:  $SKIP"
echo "  Random Seed: $SEED"
if [ -n "$MIN_PC" ] && [ -n "$MAX_PC" ]; then
    echo "  PC Range:    ${MIN_PC}-${MAX_PC} (manual)"
elif [ "$AGGRESSIVE_MODE" -eq 1 ]; then
    echo "  PC Range:    NONE (aggressive mode)"
else
    echo "  PC Range:    Auto-detect (safe mode)"
fi
echo "  Verbose:     $( [ $VERBOSE -eq 1 ] && echo 'Yes' || echo 'No' )"
echo ""

# Compile tests
# compile_tests || exit 1

if [ $COMPILE_ONLY -eq 1 ]; then
    echo "Compile-only mode, exiting."
    exit 0
fi

# Determine which tests to run
if [ -n "$SPECIFIC_TEST" ]; then
    RUN_TESTS=("$SPECIFIC_TEST")
else
    RUN_TESTS=("${TESTS[@]}")
fi

# Determine which modes to run
# Default: just mode 0 (like manual command)
# Use -M to run all modes
if [ -n "$SPECIFIC_MODE" ]; then
    RUN_MODES=("$SPECIFIC_MODE")
elif [ "$RUN_ALL_MODES" -eq 1 ]; then
    RUN_MODES=(0 1 2 3 4)
else
    RUN_MODES=(0)  # Default: just mode 0
fi

# Run tests
for test in "${RUN_TESTS[@]}"; do
    if [ $NATIVE_ONLY -eq 1 ] || [ $BASELINE_ONLY -eq 1 ]; then
        run_native "$test"
    else
        for mode in "${RUN_MODES[@]}"; do
            run_comparison "$test" "$mode"
        done
    fi
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  Test Suite Complete${NC}"
echo -e "${GREEN}========================================${NC}"
