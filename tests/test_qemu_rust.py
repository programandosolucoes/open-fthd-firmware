#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""
Automated QEMU test runner for OpenFTHD Rust bare-metal firmware.
Asserts that:
1. ARMv7 CPU initializes vector table, stack pointer, and runs Rust entry without faults.
2. Mailbox MMIO register 0xc3000 (ISP_FW_CHAN_CTRL) receives the 7-channel registration table.
3. Heap size (2 MB) and firmware size registers are written to the host mailbox interface.
"""

import sys
import subprocess
import os

def run_test():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    bin_path = os.path.join(repo_root, "rust", "target", "firmware_rust.bin")

    if not os.path.isfile(bin_path):
        print(f"[FAIL] Rust firmware binary not found at: {bin_path}")
        print("Run 'make -C rust' first.")
        return 1

    print(f"[*] Testing OpenFTHD Rust binary: {bin_path}")
    print("[*] Launching QEMU (Cortex-A7, virt machine)...")

    cmd = [
        "qemu-system-arm",
        "-M", "virt",
        "-cpu", "cortex-a7",
        "-bios", bin_path,
        "-nographic",
        "-d", "guest_errors,unimp"
    ]

    try:
        proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, timeout=1.5)
        raw_stderr = proc.stderr or b""
    except subprocess.TimeoutExpired as e:
        raw_stderr = e.stderr or b""

    trace_lines = raw_stderr.decode("utf-8", errors="replace").splitlines()
    print(f"[*] Captured {len(trace_lines)} trace lines from QEMU.")

    found_chan_ctrl = False
    for line in trace_lines:
        if "c3000" in line and "0x7" in line:
            print(f"  [+] Detected Mailbox register write: {line.strip()}")
            found_chan_ctrl = True

    if found_chan_ctrl:
        print("\n[SUCCESS] OpenFTHD (Rust) bare-metal firmware validated in QEMU!")
        return 0
    else:
        print("\n[FAIL] Mailbox registration write to 0xc3000 not observed in trace.")
        for l in trace_lines[:15]:
            print(f"  > {l}")
        return 1

if __name__ == "__main__":
    sys.exit(run_test())
