# ESP32-P4-Module-DEV-KIT Peripheral Integration Guide

## Scope and Status

This guide maps the manufacturer-documented board peripherals to the ESP-IDF 6.1 driver/component families and the project boundaries they should use. It is an implementation starting point, not a bring-up report. The product variant is the base `ESP32-P4-Module-DEV-KIT`; the physical PCB revision and its correspondence to the linked schematic are still unknown.

Use the [manufacturer GPIO map](pin-map.md) and [board manifest](board.json) as the current hardware record. A GPIO assignment from the vendor table is not proof of voltage, polarity, pull-up, power sequencing, connector routing, or current runtime readiness. Do not copy a BSP/pin configuration from another ESP32-P4 board.

The firmware uses ESP-IDF 6.1. The current component manifest directly declares `espressif/esp_video ~2.5.0`; its lockfile resolves `esp_cam_sensor 2.6.0`, `esp_ipa 2.4.0`, `esp_h264 1.4.1`, and `usb_host_uvc 2.5.0`. These versions are existing project resolution, not blanket recommendations to upgrade. Add any new managed component only after checking target/API compatibility, pin a compatible range in `idf_component.yml`, and commit the resulting lockfile as a deliberate dependency change.

## Board-Level Resource Map

| Resource | Manufacturer/project record | Integration boundary |
| --- | --- | --- |
| Main processor | ESP32-P4, two HP RISC-V cores and one LP core | ESP-IDF `esp32p4`; use board support only for verified static facts. |
| Flash / PSRAM | 16 MiB NOR Flash and 32 MiB stacked PSRAM per Waveshare; 16 MiB Flash was also read from the current device | ESP-IDF SPI Flash/NVS and PSRAM allocator. PSRAM capacity does not imply that a buffer is DMA-capable or cache-safe. |
| Wi-Fi/Bluetooth | ESP32-C6 companion over four-bit SDIO; P4 has no native Wi-Fi/Bluetooth | Existing `esp_hosted` + `esp_wifi_remote`; keep radio access in `connectivity`. SDIO identity and version match have been observed, but Wi-Fi/IP readiness remains unverified. |
| Camera / display data paths | Two-lane MIPI-CSI camera connector and two-lane MIPI-DSI display connector | Camera and LCD drivers are separate applications of P4 hardware; a connector does not mean a camera/display is attached. |
| Wired network | 100 Mbps RJ45, RMII; project manifest records IP101GRI | ESP-IDF `esp_eth` + `esp_netif` + `esp_event`; exact PHY/address/clock configuration remains to verify. |
| Expansion | 2x20 header; Waveshare says 28 remaining programmable GPIOs | SoC interfaces such as SPI, ADC, PWM, RMT and TWAI are available only after checking the matching schematic and board allocation. |

## Peripheral Integration

### C6 Radio Companion (SDIO)

- **Board signals:** P4 GPIO14..17 = C6 SDIO D0..D3, GPIO18 = CLK, GPIO19 = CMD, GPIO54 = C6_EN; GPIO6 is the separate C6_GPIO2 signal.
- **Library:** keep the existing `espressif/esp_hosted` and `espressif/esp_wifi_remote` integration. These are already in the firmware manifest/lockfile. Do not add a second P4 Wi-Fi path or use P4 `esp_wifi` as if the P4 had an integrated radio.
- **Project ownership:** `connectivity` owns Hosted lifecycle and Wi-Fi/IP service adaptation; `app_services` owns provisioning/retry policy. Do not let HTTP handlers or UI call Hosted/ESP-IDF APIs directly.
- **Gate status:** active local dev `sdkconfig` enables `APP_WIFI_WIRING_VERIFIED` after the user confirmed Waveshare base SKU 30560, its manufacturer GPIO allocation matched the existing SDIO slot-1/four-bit configuration, and live startup observed the C6 over SDIO with RPC V2. Physical PCB revision/electrical correspondence remain unverified; do not infer general peripheral readiness or change the bus mapping from this evidence. Keep C6 Wi-Fi driver NVS disabled; application-owned P4 NVS is the only permitted credential record. Transport success is not Wi-Fi readiness.
- **Separate header:** Waveshare also identifies an ESP32-C6 UART header. Its P4/USB routing and pin numbers are not established by the current GPIO allocation table; do not confuse it with P4 UART0 GPIO37/38.

### Ethernet (RMII)

- **Board signals:** RXDV 28, RXD0 29, RXD1 30, MDC 31, TXD0 34, TXD1 35, TXEN 49, REF_CLK 50, PHY_RST 51, MDIO 52. GPIO35 is also marked BOOT in the vendor table and is a P4 strapping pin.
- **Library:** ESP-IDF `esp_eth` internal EMAC + external PHY, attached to `esp_netif` and driven by `esp_event`; see the [IDF 6.1 Ethernet guide](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/api-reference/network/esp_eth.html). Start from the IDF `ethernet/basic` example. The SDK supplies a generic IEEE 802.3 PHY and separate maintained PHY components are available; choose one only after reading the exact fitted PHY marking and support status. Do not assume that a generic driver covers every IP101-specific behavior.
- **Unknowns to resolve:** verify PHY marking/revision, MDIO address, reset polarity/timing, REF_CLK source and signal correspondence against the actual schematic. GPIO50 is an ESP-IDF-supported RMII REF_CLK input; P4's documented EMAC-generated REF_CLK output pins are GPIO23/GPIO39. Since Waveshare labels GPIO50 `REF_CLK`, do not select `EMAC_CLK_OUT` unless the actual schematic shows a supported output/loopback route. If using EMAC-generated REF_CLK, account for the ESP-IDF P4 constraint that 80 MHz PSRAM and the EMAC PLL clock mode can be incompatible; prefer the schematic-supported external clock where appropriate.
- **Project ownership:** add an Ethernet adapter under `connectivity`; publish link/IP changes as snapshots/events. The current firmware does not bring Ethernet up.

### MIPI-CSI Camera, ISP, and Video Codec

- **Board signals:** two-lane MIPI-CSI. GPIO7/8 are labelled DSI/CSI SDA/SCL and also CODEC SDA/SCL/I2C in the manufacturer allocation table. Sensor reset, power, XCLK, lane wiring, and connector/cable orientation are not established by that table.
- **Preferred pipeline:** use the project’s existing Espressif `espressif/esp_video` component and its V4L2-style capture/codec APIs. The component repository includes `capture_stream`, `simple_video_server`, `m2m` and `uvc` examples; `video_lcd_display` is in the separate `esp-iot-solution` repository. Use them as API references, replacing their board-specific setup with verified Waveshare wiring. Keep one camera owner; web, USB, and other sinks consume frames from the existing `FrameBroker`, not by opening the camera independently.
- **Sensor driver:** `espressif/esp_cam_sensor` is already resolved transitively by `esp_video` in `dependencies.lock`. The registry lists OV5647 MIPI support but notes that OV5647 is discontinued; IMX219 is not listed in the current component support table. Waveshare's official `Camera_Preview` and `Camera_ISP_Tuning` examples target OV5647 and reuse the Platform I2C bus. This is a supported setup reference, not proof that the reported UC-439 contains OV5647.
- **ISP/JPEG/H.264:** `esp_video` integrates `esp_cam_sensor`, `esp_ipa`, `esp_h264`, and the hardware codec devices. Prefer this pipeline for camera streaming. Use the lower-level ESP-IDF `esp_driver_isp` (`driver/isp.h`) or `esp_driver_jpeg` (`driver/jpeg_encode.h`, `driver/jpeg_decode.h`) only when a specific application requirement needs direct ownership. JPEG hardware handles encode or decode, not both simultaneously; allocate aligned codec buffers with the provided JPEG allocator. The current device was previously read as ESP32-P4 rev.1.3; Espressif's video component documents JPEG YUV420/YUV444 encode (and YUV420 decode) as ECO3+ features. Verify how the silicon revision maps to that ECO boundary and avoid those formats until confirmed. H.264/JPEG silicon capabilities and Waveshare frame-rate claims are not measured project performance.
- **Important tuning boundary:** RAW sensors need sensor-specific initialization and ISP image tuning from the sensor/module provider. Espressif's sensor-driver guidance explicitly warns not to invent initialization register sequences from a datasheet. Validate actual format, dimensions, FPS, sensor clock and encoded output before advertising a profile.
- **Project ownership:** `camera_stream` owns the OV5647 boot probe, persistent RGB565 capture, hardware JPEG encoder and `FrameBroker`. HTTP MJPEG uses port 81; `CameraNetworkStream` consumes leases for RTSP/RTP UDP on port 8554 and direct RTP/SDP. All five upstream OV5647 modes are selectable and passed short capture/decode tests, including portrait and RAW10 1920x1080. The default remains 800x800 and preview publication is capped at 20 FPS. Capture buffers are capped at 5 MiB each; UDP JPEGs must fit 256 KiB. RTP selects the newest frame; direct UDP has bounded sub-tick pacing. USB UVC and IMX219 remain unsupported by the application. This board has one CSI input; the other ribbon connector is DSI output, not a second camera port. Keep leases until consuming transports finish, as documented in `docs/camera-pipeline.md`. These tests do not verify the PCB revision/endurance or change `camera=unverified`.

### MIPI-DSI Display and Touch

- **Board resource:** two-lane MIPI-DSI connector. Waveshare lists compatible panel sizes (5, 7, 8, 10.1 inch), but no panel is recorded as attached. Panel controller, exact resolution/timing, reset, backlight, touch controller/address/interrupt and touch wiring are unknown.
- **Library:** ESP-IDF `esp_lcd` MIPI-DSI panel IO + a matching panel driver from ESP-IDF/ESP Component Registry or Waveshare BSP; then LVGL v9 via `espressif/esp_lvgl_port` (Registry release 2.9.0 at review time). Use `lvgl_port_add_disp_dsi` for the port integration. For touch, use `espressif/esp_lcd_touch` plus the exact controller component (for example GT911 only if identified). Do not select a controller from a nearby screen product. `lvgl/lvgl` v9 and the port are not currently project dependencies because there is no verified attached panel.
- **BSP source:** Waveshare's `ESP32-P4-Platform` examples `13_Displaycolorbar`, `14_lvgl_demo_v9`, `16_video_lcd_display`, and `18_esp_brookesia_phone` are useful references. Before using one, match exact panel model/revision, pixel format and init sequence; do not carry over another board's GPIOs or panel settings.
- **Project ownership:** a future board display adapter owns DSI/panel/backlight initialization; the LVGL presenter owns only UI presentation. Follow `docs/lvgl-and-hardware.md`: all LVGL access is on the GUI task or under the port lock. Do not start LVGL on this headless/unverified profile.
- **Memory:** place frame/draw buffers according to the actual DMA/cache capabilities. Use PSRAM only after checking alignment and whether the selected LCD path can DMA from it; a smaller internal-RAM transfer buffer may be required.
- **PPA:** ESP-IDF `esp_driver_ppa` (`driver/ppa.h`) provides hardware scale/rotate/mirror, blend and fill operations. Use it for explicit image-processing needs; do not treat it as the DSI panel driver. `esp_lvgl_port` may use PPA for supported rotation paths, so keep that resource under the display stack's ownership unless a shared PPA policy is designed.

### USB 2.0 OTG Type-A

- **Board resource:** Type-A OTG socket with a jumper selecting host/device role; Waveshare calls the interface high-speed. GPIO24/25 are labelled USB1P_N/P in its allocation table. ESP-IDF documents GPIO24/25 as USB-JTAG pins by default, and the P4's separate full-speed USB 1.1 controller signals are GPIO27/26; its high-speed OTG controller uses dedicated USB pads. This naming/routing mismatch cannot be resolved from the product page alone. Trace the Type-A socket, both USB controllers, USB-JTAG, jumpers and VBUS in the matching schematic before selecting the USB peripheral or repurposing any of these GPIOs.
- **USB Host software:** ESP-IDF's managed `usb` host stack plus a maintained class component. Espressif's `usb_host_uvc` is specifically for receiving frames from a USB UVC camera; `esp_video/examples/uvc` is a host-camera example, not a P4 webcam gadget. Host callbacks run from their client event task and must not block. Host role also requires verified VBUS sourcing/current protection and connector jumper state.
- **USB Device software:** use Espressif `espressif/esp_tinyusb` (Registry release 3.0.0 at review time) for the device stack and standard classes. Its documented built-in classes include CDC, MSC, HID, MIDI, DFU, network and vendor-specific interfaces, but not a ready UVC video-device function. The project's goal of appearing to a PC as a camera therefore needs a separately reviewed UVC-device class implementation/component and compatible isochronous endpoint support; TinyUSB base integration alone is not UVC. Reuse the `usb`/TinyUSB stack rather than implementing a second USB controller stack.
- **Current project boundary:** USB frame transport must use `FrameBroker` and hold a frame lease until the USB transfer-completion callback returns ownership. Confirm host/device role and the callback context before designing that adapter. Do not connect two hosts or an ordinary A-to-A cable. No UVC device, connector routing or VBUS safety has been verified.

### TF / SD Card (SDIO 3.0)

- **Board signals:** TF_D0..D3 = GPIO39..42, CLK = GPIO43, CMD = GPIO44, TF_PWR_CTRL = GPIO45.
- **Library:** ESP-IDF `esp_driver_sdmmc` + `sdmmc` protocol/storage APIs, with `esp_vfs_fat`/FatFs if a filesystem is required. Start from the IDF 6.1 `peripherals/sdmmc` example or Waveshare `09_sdmmc`; do not enable any example option that formats a card as a side effect.
- **Controller allocation:** ESP-IDF 6.1 documents two independent P4 SDMMC slots: slot 1 is GPIO-matrix routed and non-UHS-I; slot 0 is dedicated/UHS-I. The C6 already uses slot 1 through its own GPIO mapping. Investigate slot 0 for TF because its documented dedicated signals may match the board table, but confirm exact IO_MUX, voltage supply, power switching and PCB revision before selecting the slot or claiming simultaneous C6+TF operation.
- **Electrical/storage caveat:** P4 SDMMC requires its SD I/O reference voltage to be supplied externally. UHS-I requires validated 1.8 V switching/power design; do not set UHS-I from the “SDIO 3.0” marketing label alone. Keep card detect/write-protect optional only if the schematic actually routes them.

### Onboard Audio (Microphone, Codec, Amplifier, Speaker Header)

- **Board signals:** codec/I2C SDA/SCL share GPIO7/8; I2S_DOUT 9, I2S_LRCK/WS 10, I2S_DIN 11, I2S_BCLK 12, I2S_MCLK 13; PA_CTRL GPIO53. The speaker header is MX1.25 2-pin, specified for an 8 ohm, 2 W speaker. Manufacturer confirms onboard microphone/codec/amplifier but the exact codec/amplifier part numbers and PA control polarity are not in the product text or GPIO table.
- **Library:** ESP-IDF `esp_driver_i2s` for the I2S bus and `espressif/esp_codec_dev` (Registry release 1.6.2 at review time) for supported codec abstractions. It is not currently a project dependency. Use it only after identifying the fitted codec and confirming support; the ESP-IDF P4 `i2s_es8311` example is a pattern, not evidence that this board has ES8311. If its parts are not supported, implement the smallest codec-specific control adapter from the actual datasheet/schematic.
- **Control/data paths:** codec registers travel over the shared I2C bus; PCM samples travel over I2S DMA. Share one initialized bus owner/handle rather than creating duplicate I2C controllers for audio and camera/display control. Apply PA_CTRL polarity and power sequencing from the schematic only. Keep I2S DMA callbacks short and forward work through bounded queues.
- **Current status:** no audio path is implemented. Avoid logging captured audio or presenting mic/speaker readiness without hardware tests.

### I2C Port and Shared SCCB/Control Bus

- **Board signals:** manufacturer table assigns SDA GPIO7 and SCL GPIO8 to the I2C port, camera/display SCCB, and audio CODEC control. These are shared physical nets, not separate independent buses.
- **Library:** ESP-IDF `esp_driver_i2c`, master API in `driver/i2c_master.h`. Initialize the bus once in board support and inject device/service interfaces into camera, display and audio adapters. Use 7-bit addresses from the actual component datasheets; never choose an address or claim that a device exists based on a successful bus initialization. A bounded diagnostic scan is a bring-up tool, not a runtime-ready check.
- **Electrical/ownership:** confirm pull-ups, voltage, bus speed, device addresses and clock-stretching requirements before enabling multiple clients. The camera, DSI control panel and codec may share GPIO7/8; serialize operations through the bus driver and keep long sensor/panel programming out of event-loop and HTTP callbacks.

### I3C Port

- **Board resource:** Waveshare labels an I3C connector, but the vendor GPIO allocation table does not map its SDA/SCL pins or show attached I3C targets.
- **Library:** ESP-IDF 6.1 P4 `esp_driver_i3c` (`driver/i3c_master.h`). The published driver supports I3C and legacy I2C devices on an I3C bus, but legacy I2C devices must not clock-stretch during I3C transfers. The controller's internal pull-up adjustment is documented only on GPIO32/33; use verified external electrical design where needed.
- **Do not activate yet:** obtain the schematic/revision, exact connector pinout and target device first. Do not assume this header is GPIO7/8, do not move current camera/codec users to I3C, and do not assign clocks or pull-up values from example code.

### UART / Programming and Debug

- **P4 UART pin labels:** UART0 TX GPIO37 and RX GPIO38. The current project uses UART0 as primary console and USB-Serial/JTAG as secondary console; preserve this until a separate debug route is deliberately verified. GPIO37/38 are P4 strapping pins, so external circuit loading must be checked before connecting another device.
- **Library:** ESP-IDF `esp_driver_uart` (`driver/uart.h`) for a UART application adapter; use `uart_set_pin()` only for verified routes. Type-C programming/debug and the separate C6 UART header are board-level bridges/connectors, not proof that arbitrary UART pins may be reassigned.
- **Bring-up:** begin with read-only console/loopback diagnostics on explicitly verified wiring. A loopback test needs an actual TX/RX connection and must not share the boot console unexpectedly.

### Buttons, Reset, Power LED, and Power Connectors

- Waveshare identifies BOOT and RESET buttons, a power indicator, a 5 V SH1.0 2-pin power header, a rechargeable-RTC-battery connector, and a reserved PoE-module connector. The table shows `RESET / CHIP_PU` separately from GPIO and labels GPIO35 as BOOT; do not treat CHIP_PU as a GPIO.
- For an application-readable button only after identifying its GPIO and active level, use `esp_driver_gpio` and a bounded debounce/event queue. Keep the BOOT sampling/strapping function intact. The RESET button is a hardware reset input and needs no application driver. No power-indicator GPIO is published.
- The RTC battery connector is specified for rechargeable batteries; it does not identify a separate RTC chip, charger, fuel gauge or software peripheral. Use only a documented SoC/RTC API after confirming its actual power-domain behavior. Do not connect a primary/non-rechargeable cell.
- A reserved PoE interface is not an installed PoE module. No PoE controller/PD chip, GPIO or status bus is identified. If a PoE module is fitted later, document its exact part, power path and isolation requirements before selecting any driver. PoE supplies Ethernet power; it does not create an ESP-IDF network interface by itself.

## P4 Peripherals Available on the Expansion GPIOs

Waveshare lists SPI, ADC, LED PWM, MCPWM, RMT, UART, TWAI and other SoC interfaces as capabilities. These are not necessarily populated external devices. Use the ESP-IDF 6.1 native drivers below only after the actual header pin mapping, voltage, strapping restrictions and conflicts are checked against the schematic and the current [GPIO allocation table](pin-map.md).

| Function | ESP-IDF component / API | Notes |
| --- | --- | --- |
| GPIO / resettable external logic | `esp_driver_gpio`, `driver/gpio.h` | Peripheral drivers usually configure their own signals; avoid `gpio_config()` on a peripheral-owned pad because it overwrites the active configuration. Keep ISRs short and defer work. |
| SPI master | `esp_driver_spi`, `driver/spi_master.h` | No specific SPI device/pin group is identified as populated on this board. |
| ADC | `esp_driver_adc` oneshot/continuous APIs | ADC-capable pins GPIO16..23 and 49..54 have existing onboard allocations; use only after checking conflicts and analog input range. |
| LED PWM | `esp_driver_ledc`, `driver/ledc.h` | SoC PWM capability; no onboard LED GPIO/polarity is assigned. |
| Motor PWM | `esp_driver_mcpwm`, `driver/mcpwm_prelude.h` | For an externally wired motor/control stage only; no power stage is asserted on this board. |
| RMT | `esp_driver_rmt`, `driver/rmt_tx.h` / `driver/rmt_rx.h` | Choose verified free GPIOs and bounded encoder/decoder buffers for the external device/protocol. |
| CAN / TWAI | `esp_driver_twai`, `driver/twai.h` | Requires an external CAN transceiver; do not connect MCU GPIO directly to CANH/CANL. |
| UART | `esp_driver_uart`, `driver/uart.h` | P4 has multiple controllers, but the board only publishes UART0 pins and a separate C6 UART header with unresolved routing. |
| I2C / I3C | `esp_driver_i2c` / `esp_driver_i3c` | Use the shared known I2C bus or separately verified I3C connector; do not invent extra pins. |

The exact public header/API for a driver must be checked against the selected ESP-IDF v6.1 checkout before adding `REQUIRES`/`PRIV_REQUIRES` to a component. ESP-IDF 6.x split legacy peripheral drivers into `esp_driver_*` components; do not copy old `driver` CMake dependencies from IDF 4/5 tutorials without checking migration notes.

## Suggested Integration Sequence

1. Read the PCB silkscreen/revision and choose the matching schematic. Trace component part numbers, connector pins, power rails, pull-ups, reset polarity, jumpers, boot straps and clock direction. Update the manifest before enabling a Kconfig feature.
2. Verify the base board with Waveshare `00_board_check`; record actual target/chip revision, Flash, PSRAM, heap and boot console. Do not infer this from a compile.
3. Implement one vertical slice at a time behind an adapter and typed application contract. Keep driver headers/handles inside the adapter; expose value objects/events to application services. Define ownership, thread/task context, buffers, timeouts, teardown and error states first.
4. Bring up buses and their physical connections in a non-streaming diagnostic. For I2C/I3C/SDMMC/USB, verify the electrical prerequisites before scanning or raising speed. Do not format storage during tests.
5. Add host tests for pure policy and lifetime/queue behavior, then a targeted P4 build, then physical smoke tests for the actual module. Record each separately; a component compile is not a peripheral test.
6. Only mark a capability ready after a real driver initializes and the peripheral-specific smoke path passes. Keep WebGUI/LVGL on the same read-only capability snapshot; never derive readiness from a Kconfig flag alone.

## References

- [Waveshare board overview, pinout, and connectors](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT)
- [Waveshare ESP-IDF examples](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/ESP-IDF)
- [Waveshare schematic and resources](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT/Resources-And-Documents)
- [ESP-IDF v6.1 ESP32-P4 API reference](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/api-reference/index.html)
- [Espressif video components (`esp_video`, `esp_cam_sensor`, `esp_h264`)](https://github.com/espressif/esp-video-components)
- [ESP-IDF USB Host guide](https://docs.espressif.com/projects/esp-usb/en/latest/esp32p4/usb_host.html) and [USB Device guide](https://docs.espressif.com/projects/esp-usb/en/latest/esp32p4/usb_device.html)
- [ESP-IDF Ethernet guide](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/api-reference/network/esp_eth.html)