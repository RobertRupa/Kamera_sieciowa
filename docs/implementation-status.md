# Implementation Status

Current C6 result: P4-hosted SDIO OTA to3.1.0 completed and runtime activation
was verified after restoring P4's3.1.0 host. RPC V2, target esp32c6, chip13 and
identity success were observed. Factory C6 firmware has been replaced; its exact
former version was not advertised. Old mismatch/no-C6-update notes are historical.
P4/C6 transport now succeeds; AP/client reachability and plaintext credential
read/write/reboot persistence remain unverified. The complete device WebGUI is pending.

Current key state: BLOCK_KEY0/HMAC_UP was programmed on COM8 after explicit
authorization, with read/write and purpose protections verified. Firmware no
longer derives or uses this key for NVS. No Secure Boot or Flash Encryption is
enabled.
Current Wi-Fi implementation (2026-11-09): credentials use ordinary plaintext
NVS in `app_config`; the P4 owns the single record and ESP-Hosted sends
`nvs_enable=0` to C6. AP startup has a separate local-development gate, while
credential sessions require both store readiness and the C6 storage audit.
Host tests and ESP-IDF 6.1 P4 build pass; COM8 flash and boot pass. Runtime logs
confirm ordinary NVS initialization, C6 RPC V2 3.1.0, and DHCP on AP IP
192.168.4.1. Windows did not detect the AP SSID; STA and credential reboot
persistence have not been tested.

Current C6 result: normal P4 host and C6 companion report matching ESP-Hosted
3.1.0, RPC V2, target esp32c6 and identity success over SDIO. This confirms
transport compatibility, not AP/STA operation or credential persistence.

Latest physical result (2026-11-09): native Windows esptool 5.3.1 flash PASS on
COM8 using SDK-generated `flash_args`; device booted the updated ESP-IDF 6.1
firmware. Ordinary app_config NVS initialization and credential lookup ran; no
credential write, NVS erase or eFuse change was performed. Historical
verification entries below describe earlier stages and may refer to retired
implementations.

## Available

| Area | State | Evidence |
| --- | --- | --- |
| FrameBroker / FrameLease | Implemented, host tested | CTest ownership, backpressure, pool recovery, 10,000-frame threaded run |
| ProfilePolicy | Implemented, host tested | CTest format bounds and USB conflicts |
| Hardware manifest | Manufacturer GPIO allocation and model-level peripheral inventory documented; runtime unverified | Waveshare product page/GPIO table; PCB revision and schematic correspondence remain unknown |
| Capability diagnostics | Implemented in dev profile | OV5647/one CSI frame verified; camera capability remains `unverified` because PCB revision and sustained capture are not verified |
| Wi-Fi AP/STA adapter | AP DHCP starts; Windows SSID visibility unconfirmed; STA IP acquired | ESP-Hosted 3.1.0 / remote 1.6.5; station `192.168.69.126` observed |
| mDNS adapter | Start path returned without error; resolution untested | mDNS 1.15.1; gated by actual AP IP |
| Plain NVS credential store | Device initialization and host contract tests pass; credential reboot persistence untested | `app_config`, one versioned Wi-Fi blob, no automatic erase |
| Wi-Fi setup panel | Typecheck, Vitest and browser tests pass | Scoped-session client, plaintext warning, manual SSID, validation |
| Network provisioner | Implemented, host tested | One queued request, persist before STA, boot restore, IP-confirmed connection |
| Limited provisioning HTTP API | Server start path reached; no HTTP client request tested | AP-address restriction, client-bound token, credential and operation routes |
| Device status/capabilities API | HTTP GET and embedded browser GUI verified | `/api/v1/device/status` reports OV5647 and `frame_captured`; capability remains `unverified` |
| One-frame camera probe | Implemented and passed on current unit | OV5647 PID `0x5647`; one 800x800 MIPI-CSI frame (1,280,000 bytes); probe stops before network startup |
| ESP-IDF P4 firmware | Build, flash and boot pass | ESP-IDF 6.1.0 / esp32p4 on COM8 |
| Vue WebGUI foundation | Implemented, host and device-browser tested | Same-origin auto-connect displays OV5647 and one-frame probe; persistent stream remains absent |
| CI | Configured, not run on GitHub | Host, frontend and diagnostic build jobs; no release certification |
| GitHub publication | Blocked | No authenticated fork/create tool or gh available |

## Not Implemented

- Persistent MIPI-CSI/ISP capture, FrameBroker producer and integration of the
    P4 hardware JPEG/H.264 encoders.
- USB UVC device adapter and buffer-return integration.
- HTTP multipart MJPEG and RTSP/RTP JPEG, including media authentication.
- Ethernet, routing/failover, full AP wizard/captive portal, network scan/reconnect
    and full mDNS lifecycle management.
- Hardware activation/read-write-reboot verification of plaintext NVS,
  unique administrator onboarding and authenticated device API.
- Sustained OV5647 capture/FrameBroker producer; IMX219 has no driver in the current resolved sensor component.
- Device-side serving of WebGUI and persistent settings.
- Production release security profile and hardware endurance verification.

## Required Inputs Before Hardware Integration

1. Exact PCB revision and matching schematic; camera model/sensor and ribbon cable.
2. Verified USB device routing, jumpers and power/VBUS arrangement.
3. Camera clock/power/reset, Ethernet PHY address/clock/reset, and electrical correspondence of C6 SDIO/EN to this PCB revision.
4. Hardware verification that C6 does not persist Wi-Fi credentials and P4 NVS survives reboot.
5. Reviewed RTSP component/API, compatible JPEG payload and authenticated sessions.

ESP-IDF image builds and host tests cannot establish these hardware facts. No
settings were erased; no Wi-Fi credential write or NVS erase was performed.

## Historical Notes (Before Plaintext NVS Run)

Read-only COM7 identification is complete: ESP32-P4 rev.1.3 and 16 MiB Flash.
All six key slots have USER/EMPTY purpose, and Flash Encryption is disabled.
The latest device verification recorded ESP-Hosted 3.1.0 matching on P4/C6 and
successful C6 identity. BLOCK_KEY0/HMAC_UP is provisioned and protected, but
`CONFIG_APP_NVS_HMAC_KEY_ID` remains -1 and the C6 credential-storage audit is
incomplete. Conditional Wi-Fi/API/mDNS startup therefore remains refused by
default. ESP-Hosted automatic pre-app_main initialization is disabled and guarded
by a compile error. Pin-map configuration is compiled, but the radio and secure
credential path have not been exercised on hardware.