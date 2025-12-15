import re
import sys
import os
from collections import defaultdict

# --- Configuration ---
DEBUG_SHOW_DIFFS = False 

def strip_ansi(text):
    """Removes ANSI escape codes (colors) from the text."""
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    return ansi_escape.sub('', text)

def get_clean_payload(lines):
    """
    Filters out metadata, injection logs, and headers to extract 
    ONLY the application output.
    """
    clean_lines = []
    
    # Lines starting with these strings are considered "System Noise" and ignored.
    ignore_prefixes = (
        "[INJECT]",      # The specific injection log
        "Settings:",     # Metadata
        "Target:",       # Metadata
        "Command:",      # Metadata
        "---",           # Separators
        "QEMU_",         # Env vars
        "Compiling",     # Build steps
        "Mode:",         # Test header
        "SAFE MODE:"     # Test header
    )

    for line in lines:
        s_line = strip_ansi(line).strip()
        if not s_line:
            continue
        if s_line.startswith(ignore_prefixes):
            continue
        clean_lines.append(s_line)
    
    return "\n".join(clean_lines)

def process_test(stats, prob, baseline_raw, injected_raw, was_injected, filename):
    """Compares buffers and updates the shared stats dictionary."""
    
    # 1. Category: No Injection
    if not was_injected:
        stats[prob]["no_error"] += 1
        return

    # 2. Extract strictly the application output
    base_text = get_clean_payload(baseline_raw)
    inj_text  = get_clean_payload(injected_raw)

    # 3. Compare
    if base_text == inj_text:
        stats[prob]["benign_error"] += 1
    else:
        stats[prob]["silent_data_corruption"] += 1
        if DEBUG_SHOW_DIFFS:
            print(f"\n[DIFF DETECTED] File: {filename} | Prob: {prob}")
            print("--- BASELINE ---")
            print(base_text)
            print("--- INJECTED ---")
            print(inj_text)
            print("-" * 30)

def parse_file(filepath, stats):
    """
    Parses a single file and updates the 'stats' dictionary in place.
    """
    current_prob = "Unknown"
    baseline_buffer = []
    injected_buffer = []
    
    in_baseline = False
    in_injected = False
    injection_occurred = False 
    
    filename = os.path.basename(filepath)

    try:
        with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
            for line in f:
                clean_line = strip_ansi(line).strip()

                # --- Detect New Test Block ---
                if clean_line.startswith("Test:") and "Mode:" in clean_line:
                    if baseline_buffer or injected_buffer:
                        process_test(stats, current_prob, baseline_buffer, injected_buffer, injection_occurred, filename)
                    
                    # Reset state
                    current_prob = "Unknown"
                    baseline_buffer = []
                    injected_buffer = []
                    in_baseline = False
                    in_injected = False
                    injection_occurred = False
                    continue

                # --- Capture Probability ---
                if "Settings:" in clean_line and "PROB=" in clean_line:
                    match = re.search(r'PROB=(\d+%)', clean_line)
                    if match:
                        current_prob = match.group(1)

                # --- Detect Sections ---
                if clean_line == "[BASELINE - Native]":
                    in_baseline = True
                    in_injected = False
                    continue
                elif clean_line == "[INJECTED - QEMU]":
                    in_baseline = False
                    in_injected = True
                    continue

                # --- Collect Data ---
                if in_baseline:
                    baseline_buffer.append(line)
                elif in_injected:
                    injected_buffer.append(line)
                    if clean_line.startswith("[INJECT]"):
                        injection_occurred = True

            # Process last block
            if baseline_buffer or injected_buffer:
                process_test(stats, current_prob, baseline_buffer, injected_buffer, injection_occurred, filename)

    except Exception as e:
        print(f"Error reading {filepath}: {e}")

def print_report(stats):
    # Wider columns to fit "Count (Percent%)"
    header = f"{'PROBABILITY':<12} | {'NO INJECTION':<22} | {'BENIGN (MATCH)':<22} | {'SDC (DIFF)':<22}"
    print(header)
    print("=" * len(header))
    
    sorted_probs = sorted(stats.keys(), key=lambda x: int(x.strip('%')) if x.strip('%').isdigit() else -1)
    
    for prob in sorted_probs:
        d = stats[prob]
        
        # Calculate Total for this probability group
        total = d['no_error'] + d['benign_error'] + d['silent_data_corruption']
        
        if total > 0:
            p_no_err = (d['no_error'] / total) * 100
            p_benign = (d['benign_error'] / total) * 100
            p_sdc    = (d['silent_data_corruption'] / total) * 100
        else:
            p_no_err = p_benign = p_sdc = 0.0

        # Create strings like "50 (10.5%)"
        s_no_err = f"{d['no_error']} ({p_no_err:.1f}%)"
        s_benign = f"{d['benign_error']} ({p_benign:.1f}%)"
        s_sdc    = f"{d['silent_data_corruption']} ({p_sdc:.1f}%)"

        print(f"{prob:<12} | {s_no_err:<22} | {s_benign:<22} | {s_sdc:<22}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python analyze_folder.py <path_to_folder>")
        sys.exit(1)
        
    folder_path = sys.argv[1]
    
    if not os.path.isdir(folder_path):
        print(f"Error: {folder_path} is not a directory.")
        sys.exit(1)

    global_stats = defaultdict(lambda: {
        "no_error": 0,
        "benign_error": 0,
        "silent_data_corruption": 0
    })

    print(f"Scanning folder: {folder_path}...")
    
    file_count = 0
    for filename in os.listdir(folder_path):
        if filename.endswith(".txt"):
            file_count += 1
            full_path = os.path.join(folder_path, filename)
            parse_file(full_path, global_stats)
    
    print(f"Processed {file_count} files.\n")
    print_report(global_stats)

if __name__ == "__main__":
    main()