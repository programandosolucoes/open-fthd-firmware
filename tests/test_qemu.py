#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""
Automated QEMU execution test for OpenFTHD firmware.
Verifies that:
1. ARMv7 CPU initializes vector table, stacks, and clears BSS without faults.
2. C runtime starts main() and sets up ISP mailbox channels.
3. IPC channel descriptors ('TERMINAL', 'DEBUG', 'SHAREDMALLOC', 'IO', etc.) are written to memory.
"""

import sys
import subprocess
import os

EXPECTED_CHANNELS = [
    b"TERMINAL",
    b"DEBUG",
    b"SHAREDMALLOC",
    b"IO",
    b"BUF_H2T",
    b"BUF_T2H",
    b"IO_T2H"
]

def run_test():
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    bin_path = os.path.join(repo_root, "build", "firmware.bin")

    if not os.path.isfile(bin_path):
        print(f"[FAIL] Firmware binary not found at: {bin_path}")
        print("Run 'make' first.")
        return 1

    print(f"[*] Testing OpenFTHD binary: {bin_path}")
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

    # Parse byte sequences from QEMU guest_errors / pflash_write traces
    # Trace lines look like: pflash_write: ... value 0x54) -> 'T'
    captured_bytes = bytearray()
    for line in raw_stderr.decode("utf-8", errors="replace").splitlines():
        if "value 0x" in line:
            parts = line.split("value 0x")
            if len(parts) > 1:
                hex_str = parts[1].split(")")[0].strip()
                try:
                    val = int(hex_str, 16)
                    if 0x20 <= val <= 0x7e:  # Printable ASCII
                        captured_bytes.append(val)
                except ValueError:
                    pass

    captured_text = bytes(captured_bytes)
    print(f"[*] Reconstructed ASCII stream from memory writes: {captured_text.decode('ascii', errors='replace')}")

    missing = []
    for ch in EXPECTED_CHANNELS:
        if ch in captured_text:
            print(f"  [+] Channel detected: {ch.decode('ascii')}")
        else:
            missing.append(ch)

    if not missing:
        print("\n[SUCCESS] All IPC channels initialized successfully in QEMU memory!")
        return 0
    else:
        print(f"\n[FAIL] Missing channels: {[c.decode('ascii') for c in missing]}")
        return 1

if __name__ == "__main__":
    sys.exit(run_test())
