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

Latest camera result (2026-11-09): ESP-IDF 6.1 P4 build and incremental flash
PASS on `/dev/ttyACM0`. The OV5647 probe received an 800x800 CSI frame; persistent
capture runs at an 800x800 profile capped at 20 FPS. HTTP MJPEG returned 200 and
the embedded WebGUI decoded an 800x800 frame. `http_mjpeg=ready`; `camera` remains
`unverified` because PCB revision and sustained-operation criteria are unresolved.
Direct RTP/SDP and RTSP-negotiated UDP decoded actual 800x800 frames with FFmpeg.
The current driver enumerates all five upstream OV5647 modes, including portrait
and RAW10 1920x1080; short mode-switch/decode tests passed. The UI exposes
runtime preview FPS and JPEG quality with authenticated writes. RTP takes the
latest frame; direct UDP has sub-tick pacing and live sender-timing diagnostics.
High-resolution JPEGs must fit the bounded 256 KiB RTP budget. USB UVC and
simultaneous second-camera capture remain unimplemented; the second ribbon
connector is DSI, not CSI. See [camera verification](verification.md).

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
| OV5647 capture and FrameBroker | Implemented; live frame verified on current unit | 800x800 RGB565 capture, P4 hardware JPEG, bounded 10 MiB PSRAM frame pool; preview capped at 20 FPS |
| HTTP MJPEG preview | Implemented and verified on current unit | Port 81 `/stream.mjpg`; HTTP 200; browser decoded 800x800; readiness is conditional on active capture/server |
| Hardware codec selection | Implemented; JPEG/H.264 switching decoded on current unit | Hardware H.264 via native YUV420, bitrate control, payload 96/SDP, codec-tagged leases; MJPEG unavailable under H.264; unaligned modes rejected |
| Camera configuration | Implemented; FPS/quality change verified on hardware | Actual driver mode enumeration, peer-bound dev session, asynchronous capture-task application and first-frame confirmation |
| RTP/JPEG over UDP | Implemented; both receiver paths decoded on hardware | Managed espp/rtsp 1.4.2, RTSP port 8554 and direct RTP/SDP, bounded frames and destination |
| One-frame camera probe | Implemented and passed on current unit | OV5647 PID `0x5647`; one 800x800 MIPI-CSI frame (1,280,000 bytes); preview starts after AP initialization |
| ESP-IDF P4 firmware | Build, flash and boot pass | ESP-IDF 6.1.0 / esp32p4 on COM8 |
| Vue WebGUI | Implemented, host and device-browser tested | Same-origin auto-connect displays OV5647, probe/capture status, profile and live MJPEG preview |
| CI | Configured, not run on GitHub | Host, frontend and diagnostic build jobs; no release certification |
| GitHub publication | Blocked | No authenticated fork/create tool or gh available |

## Not Implemented

- USB UVC device adapter and buffer-return integration.
- Release-grade media authentication; simultaneous multi-sensor capture requires additional verified hardware.
- Ethernet, routing/failover, full AP wizard/captive portal, network scan/reconnect
    and full mDNS lifecycle management.
- Hardware activation/read-write-reboot verification of plaintext NVS,
  unique administrator onboarding and authenticated device API.
- Long-duration OV5647 endurance and board-revision verification; IMX219 has no driver in the current resolved sensor component.
- Persistent device settings and release-grade authenticated media/API transport.
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