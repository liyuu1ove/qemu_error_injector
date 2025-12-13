#!/bin/bash
#
# run_injection_tests.sh - Automated error injection test runner
#
# This script:
# 1. Compiles all test programs
# 2. Discovers PC ranges for target functions using objdump
# 3. Runs tests with injection targeting those functions
#
# Usage: ./run_injection_tests.sh [test_name] [prob] [mode]
#   test_name: file_io_hex, db_sim, crypto, io_buffer, numeric, string (default: all)
#   prob: injection probability 0-100 (default: 50)
#   mode: 0=+1, 1=bitflip, 2=zero-byte, 3=invert, 4=random-delta (default: 0)

QEMU="../../qemu/build/qemu-x86_64"
PROB=${2:-50}
MODE=${3:-0}
SEED=42
SKIP=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Compile a test if needed
compile_test() {
    local name=$1
    local src="${name}.c"
    local bin="${name}"
    
    if [ ! -f "$src" ]; then
        echo -e "${RED}ERROR: Source file $src not found${NC}"
        return 1
    fi
    
    if [ ! -f "$bin" ] || [ "$src" -nt "$bin" ]; then
        echo -e "${YELLOW}Compiling $src...${NC}"
        gcc -O0 -g -static -o "$bin" "$src"
        if [ $? -ne 0 ]; then
            echo -e "${RED}ERROR: Compilation failed${NC}"
            return 1
        fi
    fi
    return 0
}

# Get PC range for a function using objdump
get_pc_range() {
    local bin=$1
    local func=$2
    
    # Get the disassembly for the function
    local disasm=$(objdump -d "$bin" | grep "<${func}>:" -A100 | head -102)
    
    # Extract start address (first line after function header)
    local start=$(echo "$disasm" | grep "<${func}>:" | head -1 | awk '{print $1}' | sed 's/://')
    
    # Find the ret instruction or next function to determine end
    # Look for 'ret' or the next function label
    local end_line=$(echo "$disasm" | grep -n -E '(retq|ret$|^[0-9a-f]+ <[^>]+>:)' | head -2 | tail -1)
    local end_addr=$(echo "$disasm" | sed -n '2,101p' | grep -E '^[[:space:]]*[0-9a-f]+:' | tail -1 | awk '{print $1}' | sed 's/://')
    
    if [ -z "$start" ] || [ -z "$end_addr" ]; then
        echo "0 0"
        return 1
    fi
    
    echo "0x$start 0x$end_addr"
}

# Run a test with injection
run_test() {
    local name=$1
    local func=$2
    local description=$3
    
    echo ""
    echo "=========================================="
    echo -e "${GREEN}TEST: $name${NC}"
    echo "Target function: $func"
    echo "Description: $description"
    echo "=========================================="
    
    compile_test "$name" || return 1
    
    # Get PC range
    local range=$(get_pc_range "$name" "$func")
    local min_pc=$(echo $range | awk '{print $1}')
    local max_pc=$(echo $range | awk '{print $2}')
    
    if [ "$min_pc" = "0" ]; then
        echo -e "${RED}ERROR: Could not find function $func in $name${NC}"
        return 1
    fi
    
    echo "PC range: $min_pc - $max_pc"
    echo "Injection: PROB=$PROB%, MODE=$MODE, SEED=$SEED"
    echo "------------------------------------------"
    
    # Run without injection first
    echo -e "${YELLOW}[Without injection]${NC}"
    $QEMU ./"$name" 2>/dev/null
    
    echo ""
    echo -e "${YELLOW}[With injection]${NC}"
    QEMU_INJECT_ADD=1 \
    QEMU_INJECT_ADD_PROB=$PROB \
    QEMU_INJECT_ADD_SKIP=$SKIP \
    QEMU_INJECT_ADD_MIN_PC=$min_pc \
    QEMU_INJECT_ADD_MAX_PC=$max_pc \
    QEMU_INJECT_ADD_SEED=$SEED \
    QEMU_INJECT_MODE=$MODE \
    $QEMU ./"$name" 2>&1 | grep -v '^\[INJECT\]'
}

# Main test cases
run_all_tests() {
    echo "============================================"
    echo "QEMU ADD Instruction Error Injection Tests"
    echo "============================================"
    echo "QEMU: $QEMU"
    echo "Probability: $PROB%"
    echo "Mode: $MODE (0=+1, 1=bitflip, 2=zero-byte, 3=invert, 4=random)"
    echo "Seed: $SEED"
    echo ""
    
    # Test 1: File I/O (hex output)
    run_test "test_file_io_hex" "fill_pattern_unrolled" \
        "Tests buffer fill corruption visible as hex output"
    
    # Test 2: Database simulation
    run_test "test_db_sim" "fill_records" \
        "Tests database record corruption (IDs and values)"
    
    # Test 3: Crypto/Hash
    run_test "test_crypto" "compute_hash" \
        "Tests hash computation corruption"
    
    # Test 4: I/O Buffer
    run_test "test_io_buffer" "fill_buffer_unrolled" \
        "Tests I/O buffer fill and checksum corruption"
    
    # Test 5: Numeric
    run_test "test_numeric" "compute_sum_unrolled" \
        "Tests numeric accumulation corruption"
    
    # Test 6: String
    run_test "test_string" "fill_string_unrolled" \
        "Tests string buffer fill corruption"
    
    echo ""
    echo "============================================"
    echo "All tests complete!"
    echo "============================================"
}

# Single test mode
run_single_test() {
    local test_name=$1
    
    case $test_name in
        file_io_hex|file_io)
            run_test "test_file_io_hex" "fill_pattern_unrolled" "File I/O buffer test"
            ;;
        db_sim|database|db)
            run_test "test_db_sim" "fill_records" "Database record test"
            ;;
        crypto|hash)
            run_test "test_crypto" "compute_hash" "Crypto hash test"
            ;;
        io_buffer|buffer)
            run_test "test_io_buffer" "fill_buffer_unrolled" "I/O buffer test"
            ;;
        numeric|num)
            run_test "test_numeric" "compute_sum_unrolled" "Numeric test"
            ;;
        string|str)
            run_test "test_string" "fill_string_unrolled" "String test"
            ;;
        *)
            echo "Unknown test: $test_name"
            echo "Available: file_io_hex, db_sim, crypto, io_buffer, numeric, string"
            exit 1
            ;;
    esac
}

# Entry point
if [ ! -f "$QEMU" ]; then
    echo -e "${RED}ERROR: QEMU not found at $QEMU${NC}"
    echo "Please build QEMU first: cd ../qemu/build && make -j4"
    exit 1
fi

if [ -z "$1" ] || [ "$1" = "all" ]; then
    run_all_tests
else
    run_single_test "$1"
fi
