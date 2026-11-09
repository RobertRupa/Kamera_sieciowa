# Waveshare ESP32-P4-Module-DEV-KIT Pin Map

See the [peripheral integration guide](peripheral-integration.md) for ESP-IDF drivers, component choices, bring-up sequence, and unresolved board-specific details.

## Sources and Scope

The assignments below are transcribed from Waveshare's [GPIO allocation table](https://docs.waveshare.com/assets/images/ESP32-P4-Module-DEV-KIT-GPIO-Table-62db22ecf65136c07d4a40058d27121d.webp) for the [ESP32-P4-Module-DEV-KIT](https://docs.waveshare.com/ESP32-P4-Module-DEV-KIT). Waveshare describes the table as an allocation reference and directs readers to the [schematic](https://files.waveshare.com/wiki/ESP32-P4-Module-DEV-KIT/ESP32-P4-Module-DEV-KIT.pdf) for circuit details.

The user confirmed the base product model; the physical PCB revision and its correspondence to this documentation remain unknown. These are manufacturer-documented model-level pin assignments, not electrical measurements or peripheral bring-up results. A blank table cell means no onboard signal is listed there; it does not prove that the GPIO is available for general use. Bare `IOx` labels in the table are recorded as labels only, not as confirmation that those pins are unreserved.

## Manufacturer GPIO Allocation

| P4 GPIO | LCD/Camera | SD Card | Audio | UART | I2C | Ethernet | KEY_IO | Other |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 |  |  |  |  |  |  |  | IO0 |
| 1 |  |  |  |  |  |  |  | IO1 |
| 2 |  |  |  |  |  |  |  | IO2 |
| 3 |  |  |  |  |  |  |  | IO3 |
| 4 |  |  |  |  |  |  |  | IO4 |
| 5 |  |  |  |  |  |  |  | IO5 |
| 6 |  |  |  |  |  |  |  | C6_GPIO2 |
| 7 | DSI/CSI_SDA |  | CODEC_SDA |  | SDA |  |  |  |
| 8 | DSI/CSI_SCL |  | CODEC_SCL |  | SCL |  |  |  |
| 9 |  |  | I2S_DOUT |  |  |  |  |  |
| 10 |  |  | I2S_LRCK |  |  |  |  |  |
| 11 |  |  | I2S_DIN |  |  |  |  |  |
| 12 |  |  | I2S_BCLK |  |  |  |  |  |
| 13 |  |  | I2S_MCLK |  |  |  |  |  |
| 14 |  |  |  |  |  |  |  | C6_SDIO_D0 |
| 15 |  |  |  |  |  |  |  | C6_SDIO_D1 |
| 16 |  |  |  |  |  |  |  | C6_SDIO_D2 |
| 17 |  |  |  |  |  |  |  | C6_SDIO_D3 |
| 18 |  |  |  |  |  |  |  | C6_SDIO_CLK |
| 19 |  |  |  |  |  |  |  | C6_SDIO_CMD |
| 20 |  |  |  |  |  |  |  | IO20 |
| 21 |  |  |  |  |  |  |  | IO21 |
| 22 |  |  |  |  |  |  |  | IO22 |
| 23 |  |  |  |  |  |  |  | IO23 |
| 24 |  |  |  |  |  |  |  | USB1P_N |
| 25 |  |  |  |  |  |  |  | USB1P_P |
| 26 |  |  |  |  |  |  |  | IO26 |
| 27 |  |  |  |  |  |  |  | IO27 |
| 28 |  |  |  |  |  | RXDV |  |  |
| 29 |  |  |  |  |  | RXD0 |  |  |
| 30 |  |  |  |  |  | RXD1 |  |  |
| 31 |  |  |  |  |  | MDC |  |  |
| 34 |  |  |  |  |  | TXD0 |  |  |
| 35 |  |  |  |  |  | TXD1 | BOOT |  |
| 37 |  |  |  | U0_TX |  |  |  |  |
| 38 |  |  |  | U0_RX |  |  |  |  |
| 39 |  |  |  |  |  |  |  | TF_D0 |
| 40 |  |  |  |  |  |  |  | TF_D1 |
| 41 |  |  |  |  |  |  |  | TF_D2 |
| 42 |  |  |  |  |  |  |  | TF_D3 |
| 43 |  |  |  |  |  |  |  | TF_CLK |
| 44 |  |  |  |  |  |  |  | TF_CMD |
| 45 |  |  |  |  |  |  |  | TF_PWR_CTRL |
| 46 |  |  |  |  |  |  |  | IO46 |
| 47 |  |  |  |  |  |  |  | IO47 |
| 48 |  |  |  |  |  |  |  | IO48 |
| 49 |  |  |  |  |  | TXEN |  |  |
| 50 |  |  |  |  |  | REF_CLK |  |  |
| 51 |  |  |  |  |  | PHY_RST |  |  |
| 52 |  |  |  |  |  | MDIO |  |  |
| 53 |  |  | PA_CTRL |  |  |  |  |  |
| 54 |  |  |  |  |  |  |  | C6_EN |
| Not a GPIO |  |  |  |  |  |  | RESET | CHIP_PU |

GPIO7/8 carry the table's display/camera, audio-codec and I2C labels on the same pins. GPIO35 is also marked BOOT in the table. Reset/CHIP_PU is shown as a separate row, not assigned a GPIO number.

## Current Firmware Mapping

The current ESP-Hosted defaults match the manufacturer table for C6 SDIO D0-D3 on GPIO14-17, CLK on GPIO18, CMD on GPIO19 and C6_EN on GPIO54. Firmware selects SDIO slot 1 and a four-bit bus. GPIO6 is the separate C6_GPIO2 signal, not reset/EN.

The current defaults do not configure the table's camera/display/codec I2C, I2S audio, Ethernet RMII, USB, UART0 or TF-card signals as application peripherals. The table does not establish USB routing/jumper state or VBUS safety, Ethernet PHY address and electrical timing, camera reset/XCLK/power, audio codec configuration, or the physical header wiring. Refer to the schematic and the actual PCB revision before enabling those functions.

## Verification Status

The user confirmed base SKU 30560. Its manufacturer GPIO allocation matches the active ESP-Hosted SDIO slot-1/four-bit configuration, and device startup observed C6 identity plus a successful RPC V2 handshake over those pins. On that evidence, `APP_WIFI_WIRING_VERIFIED` is enabled in the active local development `sdkconfig`; no bus pin or transport configuration was changed. The physical PCB revision remains unknown. This gate verifies the P4/C6 transport path only, not Wi-Fi client/IP/HTTP operation or readiness. `APP_C6_STORAGE_AUDITED` is a separate control; the local open AP also requires explicit dev-mode authorization and is not enabled in checked-in defaults or release.