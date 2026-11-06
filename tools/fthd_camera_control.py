#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""
===============================================================================
OpenFTHD: Advanced Camera & Hardware Feature Controller
===============================================================================

This tool and Python library allows developers, hobbyists, and applications to
control the hardware features of the FaceTime HD camera (Broadcom BCM1570 PCIe ISP).

Capabilities Exposed:
1. Hardware Digital Zoom / Pan / Tilt (ePTZ Cropping via VIDIOC_S_SELECTION)
2. Auto-Exposure Photometry Modes (Spot, Matrix, Center-Weighted, Average)
3. Hardware Dynamic Range Compression (HDR / DRC)
4. Color adjustments (Brightness, Contrast, Saturation, Hue, White Balance)

Usage in Python scripts:
    from fthd_camera_control import FTHDCamera

    cam = FTHDCamera("/dev/video0")
    cam.set_exposure_metering("spot")      # Spot photometry for backlit faces
    cam.set_hdr(True)                      # Enable on-chip dynamic range adaptation
    cam.set_digital_zoom(zoom_factor=1.5)  # 1.5x lossless hardware ePTZ crop

CLI Usage:
    python3 fthd_camera_control.py --info
    python3 fthd_camera_control.py --metering spot
    python3 fthd_camera_control.py --hdr on
    python3 fthd_camera_control.py --crop 160 90 960 540
===============================================================================
"""

import os
import sys
import fcntl
import struct
import argparse

# V4L2 Constants and IOCTL definitions
VIDIOC_G_CTRL        = 0xc008561b
VIDIOC_S_CTRL        = 0xc008561c
VIDIOC_QUERYCTRL     = 0xc0445624
VIDIOC_G_SELECTION   = 0xc040565e
VIDIOC_S_SELECTION   = 0xc040565f

# V4L2 Control IDs
V4L2_CID_BASE                 = 0x00980900
V4L2_CID_BRIGHTNESS           = V4L2_CID_BASE + 0
V4L2_CID_CONTRAST             = V4L2_CID_BASE + 1
V4L2_CID_SATURATION           = V4L2_CID_BASE + 2
V4L2_CID_HUE                  = V4L2_CID_BASE + 3
V4L2_CID_AUTO_WHITE_BALANCE   = V4L2_CID_BASE + 12
V4L2_CID_ILLUMINATORS_1       = V4L2_CID_BASE + 37

V4L2_CID_CAMERA_CLASS_BASE    = 0x009a0900
V4L2_CID_WIDE_DYNAMIC_RANGE   = V4L2_CID_CAMERA_CLASS_BASE + 21
V4L2_CID_EXPOSURE_METERING    = V4L2_CID_CAMERA_CLASS_BASE + 25

# Selection targets for hardware cropping
V4L2_BUF_TYPE_VIDEO_CAPTURE   = 1
V4L2_SEL_TGT_CROP             = 0x0000
V4L2_SEL_TGT_CROP_DEFAULT     = 0x0001
V4L2_SEL_TGT_CROP_BOUNDS      = 0x0002

EXPOSURE_METERING_MODES = {
    "average": 0,
    "center": 1,
    "spot": 2,
    "matrix": 3,
}

LED_MODES = {
    "auto": 0,
    "off": 1,
    "on": 2,
}

POWER_PROFILES = {
    "perf": 0,
    "performance": 0,
    "balanced": 1,
    "low": 2,
    "aggressive": 2,
}

SYSFS_PARAMS_DIR = "/sys/module/facetimehd/parameters"

class FTHDCamera:
    """High-level, friendly controller for FaceTime HD camera hardware features."""

    def __init__(self, device_path="/dev/video0"):
        self.device_path = device_path
        if not os.path.exists(device_path):
            raise FileNotFoundError(f"Camera device node not found: {device_path}")
        self.fd = os.open(device_path, os.O_RDWR | os.O_NONBLOCK)

    def close(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def set_control(self, ctrl_id: int, value: int):
        """Set an integer V4L2 control value."""
        # struct v4l2_control { __u32 id; __s32 value; };
        data = struct.pack("Ii", ctrl_id, int(value))
        fcntl.ioctl(self.fd, VIDIOC_S_CTRL, data)

    def get_control(self, ctrl_id: int) -> int:
        """Query current integer V4L2 control value."""
        data = struct.pack("Ii", ctrl_id, 0)
        res = fcntl.ioctl(self.fd, VIDIOC_G_CTRL, data)
        _, value = struct.unpack("Ii", res)
        return value

    def set_exposure_metering(self, mode: str):
        """
        Set on-chip photometer metering mode:
        - 'average': evaluates illumination across whole scene
        - 'center': prioritizes central subject
        - 'spot': tight spot metering on subject (ideal for backlit faces)
        - 'matrix': multi-zone evaluative grid
        """
        mode_lower = mode.lower()
        if mode_lower not in EXPOSURE_METERING_MODES:
            raise ValueError(f"Unknown metering mode '{mode}'. Choose from: {list(EXPOSURE_METERING_MODES.keys())}")
        val = EXPOSURE_METERING_MODES[mode_lower]
        self.set_control(V4L2_CID_EXPOSURE_METERING, val)
        print(f"[+] Exposure metering mode set to: {mode_lower.upper()} (val={val})")

    def set_hdr(self, enabled: bool):
        """Enable or disable hardware Dynamic Range Compression (DRC / HDR)."""
        val = 1 if enabled else 0
        self.set_control(V4L2_CID_WIDE_DYNAMIC_RANGE, val)
        print(f"[+] Hardware HDR / DRC: {'ENABLED' if enabled else 'DISABLED'}")

    def set_hardware_crop(self, left: int, top: int, width: int, height: int):
        """
        Instruct the ISP coprocessor to crop a sub-rectangle from the sensor.
        This provides zero-overhead digital pan, tilt, and zoom (ePTZ).
        """
        # struct v4l2_selection:
        # __u32 type, __u32 target, __u32 flags, struct v4l2_rect { __s32 left, top, width, height }, __u32 reserved[9]
        data = struct.pack("IIIIiiii9I",
            V4L2_BUF_TYPE_VIDEO_CAPTURE,
            V4L2_SEL_TGT_CROP,
            0, # flags
            int(left), int(top), int(width), int(height),
            0, 0, 0, 0, 0, 0, 0, 0, 0
        )
        fcntl.ioctl(self.fd, VIDIOC_S_SELECTION, data)
        print(f"[+] Applied hardware crop window: left={left}, top={top}, size={width}x{height}")

    def reset_crop(self):
        """Reset digital zoom to full frame."""
        self.set_hardware_crop(0, 0, 1920, 1080)

    def set_led(self, mode: str):
        """
        Configure camera indicator LED:
        - 'auto': Illuminated automatically during active video stream
        - 'off': Force indicator light OFF (privacy stealth mode)
        - 'on': Force indicator light continuously ON
        """
        mode_lower = mode.lower()
        if mode_lower not in LED_MODES:
            raise ValueError(f"Unknown LED mode '{mode}'. Choose from: {list(LED_MODES.keys())}")
        val = LED_MODES[mode_lower]
        try:
            self.set_control(V4L2_CID_ILLUMINATORS_1, val)
            print(f"[+] Camera Indicator LED set to: {mode_lower.upper()} (val={val})")
        except OSError:
            # Fallback to sysfs if available
            self.write_driver_param("led_mode", str(val))
            print(f"[+] Camera Indicator LED set via sysfs to: {mode_lower.upper()} (val={val})")

    def get_led(self) -> str:
        """Query camera indicator LED mode."""
        try:
            val = self.get_control(V4L2_CID_ILLUMINATORS_1)
        except OSError:
            val_str = self.read_driver_param("led_mode")
            val = int(val_str) if val_str is not None else 0
        names = {0: "Auto", 1: "Always Off (Stealth)", 2: "Always On"}
        return names.get(val, str(val))

    @staticmethod
    def read_driver_param(param_name: str):
        path = os.path.join(SYSFS_PARAMS_DIR, param_name)
        if os.path.exists(path):
            with open(path, "r") as f:
                return f.read().strip()
        return None

    @staticmethod
    def write_driver_param(param_name: str, value: str):
        path = os.path.join(SYSFS_PARAMS_DIR, param_name)
        if not os.path.exists(path):
            raise FileNotFoundError(f"Sysfs parameter {path} not found.")
        with open(path, "w") as f:
            f.write(str(value))

    def set_power_profile(self, profile: str):
        """
        Configure ISP & sensor energy profile:
        - 'perf' / 'performance': Low latency, maintains ISP/DDR clocks active
        - 'balanced': Default, standard standby when stream idle
        - 'low' / 'aggressive': Deep sensor power off on idle
        """
        prof_lower = profile.lower()
        if prof_lower not in POWER_PROFILES:
            raise ValueError(f"Unknown power profile '{profile}'. Choose from: {list(POWER_PROFILES.keys())}")
        val = POWER_PROFILES[prof_lower]
        self.write_driver_param("power_save", str(val))
        print(f"[+] Driver power profile set to: {prof_lower.upper()} (val={val})")

    def get_power_profile(self) -> str:
        """Query current power saving profile."""
        val = self.read_driver_param("power_save")
        names = {"0": "High Performance", "1": "Balanced (Default)", "2": "Aggressive Low Power"}
        return names.get(val, str(val)) if val is not None else "Unknown"

    def set_digital_zoom(self, zoom_factor: float):
        """
        Convenience helper: apply center zoom (e.g. 1.0x to 3.0x).
        Automatically computes centered crop rectangle.
        """
        if zoom_factor < 1.0:
            zoom_factor = 1.0
        max_w = 1280
        max_h = 720
        new_w = int(max_w / zoom_factor)
        new_h = int(max_h / zoom_factor)
        # Ensure 8-byte alignment for hardware
        new_w = (new_w // 8) * 8
        new_h = (new_h // 8) * 8
        left = (max_w - new_w) // 2
        top = (max_h - new_h) // 2
        self.set_hardware_crop(left, top, new_w, new_h)


def main():
    parser = argparse.ArgumentParser(description="OpenFTHD Hardware Camera Feature Controller")
    parser.add_argument("--device", default="/dev/video0", help="V4L2 device node (default: /dev/video0)")
    parser.add_argument("--info", action="store_true", help="Display camera status and available controls")
    parser.add_argument("--metering", choices=["average", "center", "spot", "matrix"], help="Set exposure metering")
    parser.add_argument("--hdr", choices=["on", "off"], help="Enable/disable hardware HDR / DRC")
    parser.add_argument("--led", choices=["auto", "off", "on"], help="Set camera status indicator LED mode")
    parser.add_argument("--power", choices=["perf", "balanced", "low"], help="Set ISP power saving profile")
    parser.add_argument("--crop", nargs=4, type=int, metavar=("LEFT", "TOP", "WIDTH", "HEIGHT"), help="Set ePTZ crop")
    parser.add_argument("--zoom", type=float, help="Set digital zoom factor (e.g. 1.5, 2.0)")
    parser.add_argument("--reset-crop", action="store_true", help="Reset crop to full frame")
    args = parser.parse_args()

    if not os.path.exists(args.device):
        print(f"[!] Device {args.device} not found. (Driver loaded?)")
        sys.exit(1)

    with FTHDCamera(args.device) as cam:
        if args.metering:
            cam.set_exposure_metering(args.metering)
        if args.hdr:
            cam.set_hdr(args.hdr == "on")
        if args.led:
            cam.set_led(args.led)
        if args.power:
            cam.set_power_profile(args.power)
        if args.crop:
            cam.set_hardware_crop(*args.crop)
        if args.zoom:
            cam.set_digital_zoom(args.zoom)
        if args.reset_crop:
            cam.reset_crop()

        if args.info or len(sys.argv) == 1:
            print("==================================================")
            print(" FaceTime HD Camera (BCM1570 PCIe ISP) Controls   ")
            print("==================================================")
            try:
                wb = cam.get_control(V4L2_CID_AUTO_WHITE_BALANCE)
                print(f"  Auto White Balance: {'ON' if wb else 'OFF'}")
            except OSError:
                pass
            try:
                drc = cam.get_control(V4L2_CID_WIDE_DYNAMIC_RANGE)
                print(f"  Hardware HDR / DRC: {'ENABLED' if drc else 'DISABLED'}")
            except OSError:
                pass
            try:
                met = cam.get_control(V4L2_CID_EXPOSURE_METERING)
                names = {0: "Average", 1: "Center-Weighted", 2: "Spot", 3: "Matrix"}
                print(f"  Exposure Metering:  {names.get(met, met)}")
            except OSError:
                pass
            print(f"  Indicator LED Mode: {cam.get_led()}")
            print(f"  Power Save Profile: {cam.get_power_profile()}")
            if os.path.exists(SYSFS_PARAMS_DIR):
                print("  Driver Parameters (/sys/module/facetimehd/parameters):")
                for p in sorted(os.listdir(SYSFS_PARAMS_DIR)):
                    val = cam.read_driver_param(p)
                    print(f"    - {p}: {val}")

if __name__ == "__main__":
    main()
