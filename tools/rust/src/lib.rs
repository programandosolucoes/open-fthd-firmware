// SPDX-License-Identifier: GPL-2.0-only
//! ===========================================================================
//! libfthd (Rust) - Didactic Hardware Control Library for FaceTime HD Camera
//! ===========================================================================
//!
//! Provides a safe, idiomatic Rust API to control the Broadcom BCM1570 PCIe ISP
//! coprocessor directly via Linux V4L2 ioctls and driver parameters.
//!
//! Features:
//! 1. Hardware ePTZ (lossless digital zoom / sensor cropping)
//! 2. Auto-exposure photometry metering (Average, Center-Weighted, Spot, Matrix)
//! 3. Hardware Dynamic Range Compression (HDR / DRC)
//! 4. Camera status / privacy indicator LED mode (Auto, Always Off, Always On)
//! 5. ISP & sensor energy profiles (Performance, Balanced, Low Power)

use std::fs::{File, OpenOptions};
use std::io::{self, Read, Write};
use std::os::unix::fs::OpenOptionsExt;
use std::os::unix::io::{AsRawFd, RawFd};
use std::path::Path;

// V4L2 IOCTL Numbers (Linux x86_64 encoding)
const VIDIOC_G_CTRL: u64      = 0xc008561b;
const VIDIOC_S_CTRL: u64      = 0xc008561c;
const VIDIOC_G_SELECTION: u64 = 0xc040565e;
const VIDIOC_S_SELECTION: u64 = 0xc040565f;

// V4L2 Control IDs
const V4L2_CID_BASE: u32               = 0x00980900;
#[allow(dead_code)]
const V4L2_CID_AUTO_WHITE_BALANCE: u32 = V4L2_CID_BASE + 12;
const V4L2_CID_ILLUMINATORS_1: u32     = V4L2_CID_BASE + 37;

const V4L2_CID_CAMERA_CLASS_BASE: u32  = 0x009a0900;
const V4L2_CID_WIDE_DYNAMIC_RANGE: u32 = V4L2_CID_CAMERA_CLASS_BASE + 21;
const V4L2_CID_EXPOSURE_METERING: u32  = V4L2_CID_CAMERA_CLASS_BASE + 25;

const V4L2_BUF_TYPE_VIDEO_CAPTURE: u32 = 1;
const V4L2_SEL_TGT_CROP: u32           = 0;

const SYSFS_POWER_SAVE: &str = "/sys/module/facetimehd/parameters/power_save";
const SYSFS_LED_MODE: &str   = "/sys/module/facetimehd/parameters/led_mode";

extern "C" {
    fn ioctl(fd: RawFd, request: u64, ...) -> i32;
}

#[repr(C)]
#[derive(Debug, Default, Clone, Copy)]
struct V4L2Control {
    id: u32,
    value: i32,
}

#[repr(C)]
#[derive(Debug, Default, Clone, Copy)]
pub struct V4L2Rect {
    pub left: i32,
    pub top: i32,
    pub width: u32,
    pub height: u32,
}

#[repr(C)]
#[derive(Debug, Default, Clone, Copy)]
struct V4L2Selection {
    type_: u32,
    target: u32,
    flags: u32,
    r: V4L2Rect,
    reserved: [u32; 9],
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ExposureMetering {
    Average = 0,
    CenterWeighted = 1,
    Spot = 2,
    Matrix = 3,
}

impl ExposureMetering {
    pub fn from_u32(val: u32) -> Self {
        match val {
            0 => Self::Average,
            1 => Self::CenterWeighted,
            2 => Self::Spot,
            _ => Self::Matrix,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum LedMode {
    Auto = 0,
    ForceOff = 1,
    ForceOn = 2,
}

impl LedMode {
    pub fn from_u32(val: u32) -> Self {
        match val {
            1 => Self::ForceOff,
            2 => Self::ForceOn,
            _ => Self::Auto,
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PowerProfile {
    Performance = 0,
    Balanced = 1,
    LowPower = 2,
}

impl PowerProfile {
    pub fn from_u32(val: u32) -> Self {
        match val {
            0 => Self::Performance,
            2 => Self::LowPower,
            _ => Self::Balanced,
        }
    }
}

/// Camera device handle for FaceTime HD BCM1570
pub struct FthdCamera {
    file: File,
    path: String,
}

impl FthdCamera {
    /// Open the camera device node (default: "/dev/video0")
    pub fn open<P: AsRef<Path>>(path: P) -> io::Result<Self> {
        let path_str = path.as_ref().to_string_lossy().to_string();
        let file = OpenOptions::new()
            .read(true)
            .write(true)
            .custom_flags(libc_nonblock())
            .open(&path)?;

        Ok(Self {
            file,
            path: path_str,
        })
    }

    /// Read an integer V4L2 control
    fn get_control(&self, id: u32) -> io::Result<i32> {
        let mut ctrl = V4L2Control { id, value: 0 };
        let ret = unsafe { ioctl(self.file.as_raw_fd(), VIDIOC_G_CTRL, &mut ctrl) };
        if ret < 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(ctrl.value)
    }

    /// Set an integer V4L2 control
    fn set_control(&self, id: u32, value: i32) -> io::Result<()> {
        let mut ctrl = V4L2Control { id, value };
        let ret = unsafe { ioctl(self.file.as_raw_fd(), VIDIOC_S_CTRL, &mut ctrl) };
        if ret < 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(())
    }

    /// Set sensor crop window for hardware ePTZ digital zoom
    pub fn set_crop(&self, left: i32, top: i32, width: u32, height: u32) -> io::Result<()> {
        let mut sel = V4L2Selection {
            type_: V4L2_BUF_TYPE_VIDEO_CAPTURE,
            target: V4L2_SEL_TGT_CROP,
            flags: 0,
            r: V4L2Rect { left, top, width, height },
            reserved: [0; 9],
        };
        let ret = unsafe { ioctl(self.file.as_raw_fd(), VIDIOC_S_SELECTION, &mut sel) };
        if ret < 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(())
    }

    /// Query current sensor crop window
    pub fn get_crop(&self) -> io::Result<V4L2Rect> {
        let mut sel = V4L2Selection {
            type_: V4L2_BUF_TYPE_VIDEO_CAPTURE,
            target: V4L2_SEL_TGT_CROP,
            ..Default::default()
        };
        let ret = unsafe { ioctl(self.file.as_raw_fd(), VIDIOC_G_SELECTION, &mut sel) };
        if ret < 0 {
            return Err(io::Error::last_os_error());
        }
        Ok(sel.r)
    }

    /// Convenience: Set centered digital zoom factor (1.0x to 3.0x)
    pub fn set_zoom(&self, factor: f32) -> io::Result<()> {
        let f = factor.max(1.0);
        let base_w = 1280u32;
        let base_h = 720u32;

        let new_w = ((base_w as f32 / f) as u32 / 8) * 8;
        let new_h = ((base_h as f32 / f) as u32 / 8) * 8;

        let left = ((base_w - new_w) / 2) as i32;
        let top = ((base_h - new_h) / 2) as i32;

        self.set_crop(left, top, new_w, new_h)
    }

    /// Reset digital zoom to full sensor frame
    pub fn reset_crop(&self) -> io::Result<()> {
        self.set_crop(0, 0, 1280, 720)
    }

    /// Set Auto-Exposure Photometry metering mode
    pub fn set_exposure_metering(&self, mode: ExposureMetering) -> io::Result<()> {
        self.set_control(V4L2_CID_EXPOSURE_METERING, mode as i32)
    }

    /// Query current Auto-Exposure metering mode
    pub fn get_exposure_metering(&self) -> io::Result<ExposureMetering> {
        let val = self.get_control(V4L2_CID_EXPOSURE_METERING)?;
        Ok(ExposureMetering::from_u32(val as u32))
    }

    /// Enable or disable hardware Dynamic Range Compression (HDR / DRC)
    pub fn set_hdr(&self, enable: bool) -> io::Result<()> {
        self.set_control(V4L2_CID_WIDE_DYNAMIC_RANGE, if enable { 1 } else { 0 })
    }

    /// Query hardware HDR state
    pub fn get_hdr(&self) -> io::Result<bool> {
        let val = self.get_control(V4L2_CID_WIDE_DYNAMIC_RANGE)?;
        Ok(val != 0)
    }

    /// Set camera indicator LED mode
    pub fn set_led(&self, mode: LedMode) -> io::Result<()> {
        if self.set_control(V4L2_CID_ILLUMINATORS_1, mode as i32).is_ok() {
            return Ok(());
        }
        // Fallback to sysfs write
        if Path::new(SYSFS_LED_MODE).exists() {
            let mut f = File::create(SYSFS_LED_MODE)?;
            write!(f, "{}\n", mode as u32)?;
            return Ok(());
        }
        Err(io::Error::new(io::ErrorKind::Unsupported, "LED control not supported"))
    }

    /// Query camera indicator LED mode
    pub fn get_led(&self) -> io::Result<LedMode> {
        if let Ok(val) = self.get_control(V4L2_CID_ILLUMINATORS_1) {
            return Ok(LedMode::from_u32(val as u32));
        }
        if Path::new(SYSFS_LED_MODE).exists() {
            let mut f = File::open(SYSFS_LED_MODE)?;
            let mut content = String::new();
            f.read_to_string(&mut content)?;
            if let Ok(val) = content.trim().parse::<u32>() {
                return Ok(LedMode::from_u32(val));
            }
        }
        Ok(LedMode::Auto)
    }

    /// Set kernel driver power saving profile
    pub fn set_power_profile(profile: PowerProfile) -> io::Result<()> {
        let mut f = File::create(SYSFS_POWER_SAVE)?;
        write!(f, "{}\n", profile as u32)?;
        Ok(())
    }

    /// Query kernel driver power saving profile
    pub fn get_power_profile() -> io::Result<PowerProfile> {
        let mut f = File::open(SYSFS_POWER_SAVE)?;
        let mut content = String::new();
        f.read_to_string(&mut content)?;
        let val = content.trim().parse::<u32>().map_err(|_| {
            io::Error::new(io::ErrorKind::InvalidData, "Invalid power_save parameter format")
        })?;
        Ok(PowerProfile::from_u32(val))
    }

    /// Print complete diagnostics
    pub fn print_status(&self) {
        println!("=====================================================");
        println!(" FaceTime HD Camera (BCM1570 PCIe ISP) Rust Status  ");
        println!("=====================================================");
        println!(" Device Node: {}", self.path);

        if let Ok(hdr) = self.get_hdr() {
            println!(" Hardware HDR / DRC:       {}", if hdr { "ENABLED" } else { "DISABLED" });
        }
        if let Ok(met) = self.get_exposure_metering() {
            println!(" Exposure Metering:        {:?}", met);
        }
        if let Ok(led) = self.get_led() {
            println!(" Indicator LED Mode:       {:?}", led);
        }
        if let Ok(pwr) = Self::get_power_profile() {
            println!(" Power Saving Profile:     {:?}", pwr);
        }
        if let Ok(crop) = self.get_crop() {
            println!(" ePTZ Crop Window:         left={}, top={}, size={}x{}",
                crop.left, crop.top, crop.width, crop.height);
        }
        println!("=====================================================");
    }
}

fn libc_nonblock() -> i32 {
    0x800 // O_NONBLOCK on Linux x86_64
}
