// SPDX-License-Identifier: GPL-2.0-only
//! ===========================================================================
//! fthd-ctl: Rust CLI application for FaceTime HD Camera Hardware Controls
//! ===========================================================================

use fthd_ctl::{ExposureMetering, FthdCamera, LedMode, PowerProfile};
use std::env;

fn print_usage(prog: &str) {
    println!("Usage: {} [OPTIONS]\n", prog);
    println!("FaceTime HD Camera (BCM1570 PCIe ISP) Control Utility (Rust Implementation)\n");
    println!("Options:");
    println!("  --device <path>       Specify V4L2 device node (default: /dev/video0)");
    println!("  --status              Print hardware state and active controls");
    println!("  --zoom <factor>       Set ePTZ digital zoom factor (e.g. 1.0, 1.5, 2.0)");
    println!("  --metering <mode>     Set exposure photometry: average, center, spot, matrix");
    println!("  --hdr <on|off>        Enable or disable hardware HDR / DRC");
    println!("  --led <auto|off|on>   Set camera indicator LED mode");
    println!("  --power <perf|bal|low>Set driver energy profile");
    println!("  --reset               Reset crop window to full 1280x720 frame");
    println!("  --compute-aes <text>  Build and test Channel 7 AES-128 Compute packet");
    println!("  --compute-neon        Build and test Channel 7 NEON SIMD Dot Product packet");
    println!("  --help                Show this didactic guide\n");
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let prog = &args[0];

    if args.contains(&"--help".to_string()) || args.contains(&"-h".to_string()) {
        print_usage(prog);
        return;
    }

    let mut device_path = "/dev/video0".to_string();
    let mut show_status = false;
    let mut zoom: Option<f32> = None;
    let mut metering: Option<ExposureMetering> = None;
    let mut hdr: Option<bool> = None;
    let mut led: Option<LedMode> = None;
    let mut power: Option<PowerProfile> = None;
    let mut reset_crop = false;
    let mut compute_aes: Option<String> = None;
    let mut compute_neon = false;

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--compute-aes" => {
                if i + 1 < args.len() {
                    compute_aes = Some(args[i + 1].clone());
                    i += 1;
                }
            }
            "--compute-neon" => {
                compute_neon = true;
            }
            "--device" | "-d" => {
                if i + 1 < args.len() {
                    device_path = args[i + 1].clone();
                    i += 1;
                }
            }
            "--status" | "-s" => show_status = true,
            "--zoom" | "-z" => {
                if i + 1 < args.len() {
                    zoom = args[i + 1].parse::<f32>().ok();
                    i += 1;
                }
            }
            "--metering" | "-m" => {
                if i + 1 < args.len() {
                    metering = match args[i + 1].to_lowercase().as_str() {
                        "average" => Some(ExposureMetering::Average),
                        "center" => Some(ExposureMetering::CenterWeighted),
                        "spot" => Some(ExposureMetering::Spot),
                        "matrix" => Some(ExposureMetering::Matrix),
                        _ => None,
                    };
                    i += 1;
                }
            }
            "--hdr" => {
                if i + 1 < args.len() {
                    hdr = match args[i + 1].to_lowercase().as_str() {
                        "on" | "1" | "true" => Some(true),
                        "off" | "0" | "false" => Some(false),
                        _ => None,
                    };
                    i += 1;
                }
            }
            "--led" => {
                if i + 1 < args.len() {
                    led = match args[i + 1].to_lowercase().as_str() {
                        "auto" | "0" => Some(LedMode::Auto),
                        "off" | "1" => Some(LedMode::ForceOff),
                        "on" | "2" => Some(LedMode::ForceOn),
                        _ => None,
                    };
                    i += 1;
                }
            }
            "--power" => {
                if i + 1 < args.len() {
                    power = match args[i + 1].to_lowercase().as_str() {
                        "perf" | "performance" | "0" => Some(PowerProfile::Performance),
                        "bal" | "balanced" | "1" => Some(PowerProfile::Balanced),
                        "low" | "aggressive" | "2" => Some(PowerProfile::LowPower),
                        _ => None,
                    };
                    i += 1;
                }
            }
            "--reset" => reset_crop = true,
            _ => {}
        }
        i += 1;
    }

    if let Some(pwr) = power {
        match FthdCamera::set_power_profile(pwr) {
            Ok(()) => println!("[+] Set power saving profile to: {:?}", pwr),
            Err(e) => eprintln!("[!] Failed to set power profile: {} (needs root)", e),
        }
    }

    if let Some(ref text) = compute_aes {
        let key = b"0123456789abcdef";
        let packet = fthd_ctl::compute::build_aes_packet(key, text.as_bytes());
        println!("[+] Channel 7 Compute AES-128 Packet Built ({} bytes):", packet.len());
        println!("    Key:       {:?}", std::str::from_utf8(key).unwrap_or(""));
        println!("    Plaintext: {}", text);
        println!("    Opcode:    0x{:02x} (OP_AES_128_ENCRYPT)", packet[0]);
    }

    if compute_neon {
        let a = [1i16, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
        let b = [16i16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1];
        let packet = fthd_ctl::compute::build_neon_dot_packet(&a, &b);
        let dot = fthd_ctl::compute::sim_neon_dot(&a, &b);
        println!("[+] Channel 7 Compute NEON Dot Product Packet Built ({} bytes):", packet.len());
        println!("    Vectors:   16 elements (i16)");
        println!("    Expected:  {}", dot);
        println!("    Opcode:    0x{:02x} (OP_NEON_DOT_PRODUCT)", packet[0]);
    }

    if (compute_aes.is_some() || compute_neon)
        && !show_status
        && zoom.is_none()
        && metering.is_none()
        && hdr.is_none()
        && led.is_none()
        && !reset_crop
    {
        return;
    }


    let cam = match FthdCamera::open(&device_path) {
        Ok(c) => c,
        Err(e) => {
            if power.is_none() {
                eprintln!("[!] Cannot open camera at {}: {}", device_path, e);
            }
            return;
        }
    };

    if reset_crop {
        match cam.reset_crop() {
            Ok(()) => println!("[+] Reset crop to full sensor frame."),
            Err(e) => eprintln!("[!] Failed to reset crop: {}", e),
        }
    }

    if let Some(z) = zoom {
        match cam.set_zoom(z) {
            Ok(()) => println!("[+] Applied ePTZ digital zoom: {:.2}x", z),
            Err(e) => eprintln!("[!] Failed to set zoom: {}", e),
        }
    }

    if let Some(m) = metering {
        match cam.set_exposure_metering(m) {
            Ok(()) => println!("[+] Applied exposure metering: {:?}", m),
            Err(e) => eprintln!("[!] Failed to set metering: {}", e),
        }
    }

    if let Some(h) = hdr {
        match cam.set_hdr(h) {
            Ok(()) => println!("[+] Applied hardware HDR: {}", if h { "ENABLED" } else { "DISABLED" }),
            Err(e) => eprintln!("[!] Failed to set HDR: {}", e),
        }
    }

    if let Some(l) = led {
        match cam.set_led(l) {
            Ok(()) => println!("[+] Applied indicator LED: {:?}", l),
            Err(e) => eprintln!("[!] Failed to set LED: {}", e),
        }
    }

    if show_status || args.len() == 1 {
        cam.print_status();
    }
}
