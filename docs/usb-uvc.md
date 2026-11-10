# ESP32-P4 USB UVC webcam (experimental integration)

## Hardware and firmware

Target: Waveshare ESP32-P4-Module-DEV-KIT (base model), OV5647 MIPI-CSI,
ESP-IDF 6.1, PSRAM, one existing shared hardware JPEG encoder.

USB: ESP32-P4 internal **High-Speed USB 2.0 OTG** in **device** mode,
Espressif `usb_device_uvc ~1.3.1` / TinyUSB, MJPEG bulk endpoint.
This is a native UVC USB device; host systems use their standard webcam
drivers, without OBS virtual camera software.

**Waveshare SKU 30560 USB routing (verified against the manufacturer FAQ and
the three-page schematic):** The two USB-C connectors on this board are for
USB-UART programming/debug and native P4 USB serial/programming. They are
**not** routed to the P4's dedicated **High-Speed USB OTG** controller used
by the UVC firmware. Therefore a serial device appearing on either USB-C port
is not UVC enumeration.

The four USB-A sockets are numbered as viewed facing the sockets, with the
Ethernet jack on the **left**:

| Connector | Location | Manufacturer jumper selection |
| --- | --- | --- |
| USB-A **1** | Left pair, lower socket (nearest Ethernet) | **HOST** position — direct ESP32-P4 link |
| USB-A **2–4** | Remaining sockets | **DEVICE** position — CH334 hub path |

**Important:** The jumper switches the **USB data path (direct vs hub)**.
Its silkscreen is not a reliable indication of the running firmware's USB
host/device role. In the DEVICE position the direct USB-A 1 path needed for
UVC device firmware is disconnected. See
[official Waveshare FAQ](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/FAQ),
[port numbering photo](https://docs.waveshare.com/assets/images/ESP32-P4-USB-6848eed08e2e1a2477a910ccc822e6c4.webp)
and [schematic](https://files.waveshare.com/wiki/ESP32-P4-Module-DEV-KIT/ESP32-P4-Module-DEV-KIT.pdf).

**Electrical safety:** USB-A female to USB-A female is not a conventional
device connection. Do **not** join the PC's USB-A host to the board's Type-A
receptacle using an ordinary male-A/male-A cable: board-side VBUS output and
PC-side VBUS may conflict. The correct bench solution requires checking the
actual schematic, the Type-A port-1 VBUS/USB power switch, data routing and
whether host VBUS is safely sensed by the UVC device. Use a verified
**device-mode breakout/adapter** with correct D+/D-, GND and VBUS
management (or an appropriate hardware modification) before connecting
the PC. Simply moving the jumper is not enough to guarantee electrical
USB device compatibility. The other USB-A sockets routed through CH334
will not enumerate the P4's native UVC endpoint.

## First bring-up: Linux host cannot detect a webcam

1. Confirm camera-side serial log includes
   `USB HS UVC MJPEG stack started`. This proves only that TinyUSB started,
   **not** that the host enumerated the device.
2. Check the physical USB route (device-role jumper, correct direct OTG data
   connector, good USB 2.0 high-speed data cable, host-side VBUS sensing).
   A USB-C programming/serial link on this board is not a P4 HS UVC link.
   Waveshare's FAQ notes that the Type-A ports are switched between direct
   connection and the CH334 hub; do not treat any Type-A port as a safe
   standard host-to-host cable attachment.
3. On the **Ubuntu host**, monitor connection attempts while reconnecting the
   verified data connector:

   ```bash
   sudo dmesg -w
   # In another terminal:
   lsusb
   sudo apt-get install -y v4l-utils
   v4l2-ctl --list-devices
   ```

   If `dmesg` shows **no new USB device at all**, investigate cable,
   connector, jumper, power and VBUS first. If `lsusb` shows a device but
   `/dev/video*` is missing, inspect its USB descriptors and `uvcvideo`
   binding: `lsusb -v -d VID:PID`, `lsmod | grep uvcvideo`,
   `sudo modprobe uvcvideo`. Do not guess a VID/PID; read it from `lsusb`.
4. When the host starts the UVC stream, the serial console should print
   `USB host requested UVC ...` and `UVC COMMIT ...`. No such log means
   the video application has not initiated a stream (even if descriptors
   were enumerated).
5. Confirm the earlier serial log also contains a successful
   `camera probe=...` and that persistent capture is running. UVC
   initializes only when camera capture has started.

USB device enumeration and Wi-Fi association are independent. The first
cannot be repaired solely by changing AP/STA configuration.

## Development Wi-Fi recovery

The checked-in `sdkconfig.dev.defaults` (included only by the **default**
CMake preset) authorizes the **already tested**
ESP32-P4 ↔ ESP32-C6 SDIO wiring and an **open development setup AP**.
This is intentionally insecure and suitable **only** for an isolated
development environment. The AP SSID is `CAMERA-HUB-SETUP`; once started,
it is expected at `192.168.4.1`. Configure a station's SSID/password
through the setup WebGUI or use previously saved credentials.

Because an existing `build/default/sdkconfig` survives Git pulls, it
may still contain `# CONFIG_APP_WIFI_WIRING_VERIFIED is not set` and/or
`# CONFIG_APP_DEV_OPEN_SETUP_AP is not set`. Use
`idf.py --preset default menuconfig` → **Camera Hub** to enable both,
then `idf.py --preset default build flash monitor`. On boot check
`Wi-Fi gates: wiring_verified=1 open_ap_authorized=1 ...`. If
initialization then fails in ESP-Hosted/SDIO, inspect the **first**
`wifi_service` or `esp_hosted` error; it is a separate issue.

## Sensor mode vs actual delivered FPS

The firmware no longer limits camera publication to 20 FPS. The source
enumerates driver-supported OV5647 modes; the mode selector in WebGUI
includes both dimensions and nominal sensor FPS:

| Sensor mode | Nominal FPS | WebGUI | USB UVC descriptor |
| --- | ---: | --- | --- |
| 800×800 RAW8 | 50 | Yes | Yes |
| 800×640 RAW8 | 50 | Yes | Yes |
| 800×1280 RAW8 | 50 | Yes | No (UVC class currently advertises four frames) |
| 1280×960 RAW10 binning | 45 | Yes | Yes |
| 1920×1080 RAW10 | 30 | Yes | Yes |

These are sensor/driver target modes, **not guaranteed output frame rates**.
Actual USB transfer rate depends on JPEG size/quality, hardware encode speed,
USB host and bus overhead. High-resolution modes must fit a 1 MiB JPEG slot.
The USB transfer copy has a separate bounded 1 MiB PSRAM buffer; FrameBroker
reserves ten 1 MiB slots shared between HTTP, RTSP and USB. Capture has two
additional V4L2 buffers. Sustained peak-throughput testing is pending.

USB UVC negotiates only JPEG and the four statically advertised modes.
The shared `CameraHardwareProbe` owns CSI, capture and JPEG encoding; USB
borrows the newest immutable `FrameLease` until copied to the UVC transfer
buffer. WebGUI mode/quality/codec configuration returns HTTP 409 while a
USB stream is active. The camera can capture for USB without any Wi-Fi, AP
or STA connection; network setup failures do not stop the USB service.
While no USB host is streaming, USB has no enabled frame queue.

## Build / flash in VS Code sidebar

Open `Kamera_sieciowa.code-workspace`, install the recommended
`espressif.esp-idf-extension`, select the **Kamera_sieciowa (ESP32-P4)** root workspace folder, ESP-IDF 6.1,
the `default` CMake preset (target `esp32p4`) and the programming port.
Use the **Espressif** icon at left and click **Build Your Project**,
**Flash Your Project**, or **Build, Flash and Start a Monitor**.

The pre-build hook `scripts/prepare_web_assets.py` installs frontend
dependencies when missing and rebuilds the embedded Vue WebGUI only if
sources changed. To build manually from a configured ESP-IDF environment:

```bash
# Run from the repository root (Kamera_sieciowa/).
python3 scripts/prepare_web_assets.py
idf.py --preset default build
idf.py --preset default -p /dev/ttyACM0 flash monitor # choose actual programming port
```

In Windows use your selected `COMx` port in place of `/dev/ttyACM0`.
Do not erase flash/eFuses or write backup images as part of normal flashing.

The new UVC component changes the dependency graph. Regenerate and inspect
`dependencies.lock` at the repository root by running IDF Component Manager on a machine
with network access and ESP-IDF 6.1; this step remains pending here.

## Host verification checklist (not yet executed)

- Check USB device-role jumper, connection, VBUS and safe cable.
- Linux: `v4l2-ctl --list-devices`; `v4l2-ctl -d /dev/videoX --list-formats-ext`.
- Linux MJPEG capture: `ffplay -f v4l2 -input_format mjpeg -video_size 800x800 -framerate 50 /dev/videoX`.
- Windows: test the native **Camera** app and OBS **Video Capture Device** (not virtual camera).
- Check each advertised mode and host UVC COMMIT response, actual FPS,
  frame drops, codec/quality, and capture memory peaks.
- Confirm USB still works with Wi-Fi disconnected and after a USB unplug/replug.
- Confirm HTTP/RTSP operate independently and that WebGUI settings return
  HTTP 409 while a USB host actively streams.

A successful build or descriptor enumeration alone is not proof of frame
transport performance. No hardware test result is claimed in this document.
