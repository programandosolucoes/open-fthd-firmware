/* SPDX-License-Identifier: GPL-2.0-only */
/**
 * ============================================================================
 * fthd-tool - Command-line interface for FaceTime HD hardware controls in C
 * ============================================================================
 */

#include "libfthd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

static void print_usage(const char *progname)
{
    printf("Usage: %s [OPTIONS]\n\n", progname);
    printf("FaceTime HD Camera (BCM1570 PCIe ISP) Control Utility (C Implementation)\n\n");
    printf("Options:\n");
    printf("  -d, --device <path>       Specify V4L2 device node (default: /dev/video0)\n");
    printf("  -s, --status              Print comprehensive hardware state and controls\n");
    printf("  -z, --zoom <factor>       Set ePTZ digital zoom factor (e.g. 1.0, 1.5, 2.0)\n");
    printf("  -m, --metering <mode>     Set exposure photometry: average, center, spot, matrix\n");
    printf("  -w, --hdr <0|1>           Enable (1) or disable (0) hardware HDR / DRC\n");
    printf("  -l, --led <0|1|2>         Set LED: 0=Auto, 1=Always Off (Stealth), 2=Always On\n");
    printf("  -p, --power <0|1|2>       Set power profile: 0=High Perf, 1=Balanced, 2=Low Power\n");
    printf("  -r, --reset               Reset crop window to full 1280x720 frame\n");
    printf("  -h, --help                Show this didactic help guide\n\n");
}

int main(int argc, char *argv[])
{
    const char *device_path = "/dev/video0";
    int opt;
    bool show_status = false;
    float zoom_val = 0.0f;
    int metering_val = -1;
    int hdr_val = -1;
    int led_val = -1;
    int power_val = -1;
    bool reset_val = false;

    static struct option long_options[] = {
        {"device",   required_argument, 0, 'd'},
        {"status",   no_argument,       0, 's'},
        {"zoom",     required_argument, 0, 'z'},
        {"metering", required_argument, 0, 'm'},
        {"hdr",      required_argument, 0, 'w'},
        {"led",      required_argument, 0, 'l'},
        {"power",    required_argument, 0, 'p'},
        {"reset",    no_argument,       0, 'r'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "d:sz:m:w:l:p:rh", long_options, NULL)) != -1) {
        switch (opt) {
        case 'd':
            device_path = optarg;
            break;
        case 's':
            show_status = true;
            break;
        case 'z':
            zoom_val = atof(optarg);
            break;
        case 'm':
            if (strcmp(optarg, "average") == 0) metering_val = 0;
            else if (strcmp(optarg, "center") == 0) metering_val = 1;
            else if (strcmp(optarg, "spot") == 0) metering_val = 2;
            else if (strcmp(optarg, "matrix") == 0) metering_val = 3;
            else metering_val = atoi(optarg);
            break;
        case 'w':
            hdr_val = atoi(optarg);
            break;
        case 'l':
            led_val = atoi(optarg);
            break;
        case 'p':
            power_val = atoi(optarg);
            break;
        case 'r':
            reset_val = true;
            break;
        case 'h':
        default:
            print_usage(argv[0]);
            return 0;
        }
    }

    /* If power profile is requested, set it even without device opening */
    if (power_val >= 0) {
        if (fthd_set_power_profile((fthd_power_profile_t)power_val) == 0) {
            printf("[+] Applied power saving profile: %d\n", power_val);
        } else {
            perror("[!] Failed to set power profile (root required)");
        }
    }

    fthd_device_t *dev = fthd_open(device_path);
    if (!dev) {
        if (power_val < 0) {
            fprintf(stderr, "[!] Cannot open %s. Is the facetimehd driver loaded?\n", device_path);
            return 1;
        }
        return 0;
    }

    if (reset_val) {
        if (fthd_reset_crop(dev) == 0) {
            printf("[+] Reset crop window to full sensor frame.\n");
        } else {
            perror("[!] Failed to reset crop");
        }
    }

    if (zoom_val >= 1.0f) {
        if (fthd_set_zoom(dev, zoom_val) == 0) {
            printf("[+] Applied ePTZ digital zoom factor: %.2fx\n", zoom_val);
        } else {
            perror("[!] Failed to set zoom factor");
        }
    }

    if (metering_val >= 0 && metering_val <= 3) {
        if (fthd_set_exposure_metering(dev, (fthd_metering_mode_t)metering_val) == 0) {
            printf("[+] Applied exposure metering mode: %d\n", metering_val);
        } else {
            perror("[!] Failed to set metering mode");
        }
    }

    if (hdr_val >= 0) {
        if (fthd_set_hdr(dev, hdr_val != 0) == 0) {
            printf("[+] Applied hardware HDR / DRC: %s\n", hdr_val ? "ENABLED" : "DISABLED");
        } else {
            perror("[!] Failed to set HDR");
        }
    }

    if (led_val >= 0 && led_val <= 2) {
        if (fthd_set_led(dev, (fthd_led_mode_t)led_val) == 0) {
            printf("[+] Applied indicator LED mode: %d\n", led_val);
        } else {
            perror("[!] Failed to set LED mode");
        }
    }

    if (show_status || argc == 1) {
        fthd_print_status(dev);
    }

    fthd_close(dev);
    return 0;
}
