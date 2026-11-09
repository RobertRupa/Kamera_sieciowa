# Waveshare ESP32-P4-Module-DEV-KIT Pin Map

## Source

Transcribed from the GPIO/peripheral allocation table supplied by the user in
this conversation on 2026-11-08. The image has no visible PCB revision or source
URL. This is reported board wiring, not a successful hardware/firmware test.
Do not substitute the chip revision or camera UC-439 rev.B for the PCB revision.

## Factory P4 to C6 Link

| Table signal | P4 GPIO | ESP-Hosted setting |
| --- | --- | --- |
| C6_SDIO_D0 | 14 | ESP_HOSTED_HOST_SDIO_PIN_D0 |
| C6_SDIO_D1 | 15 | ESP_HOSTED_HOST_SDIO_PIN_D1 |
| C6_SDIO_D2 | 16 | ESP_HOSTED_HOST_SDIO_PIN_D2 |
| C6_SDIO_D3 | 17 | ESP_HOSTED_HOST_SDIO_PIN_D3 |
| C6_SDIO_CLK | 18 | ESP_HOSTED_HOST_SDIO_PIN_CLK |
| C6_SDIO_CMD | 19 | ESP_HOSTED_HOST_SDIO_PIN_CMD |
| C6_EN | 54 | ESP_HOSTED_HOST_RESET_GPIO (RST/EN line) |
| C6_GPIO2 | 6 | Separate signal; NOT the reset/EN line |

ESP-Hosted 3.1.0's existing configuration matches these GPIOs, with SDIO slot 1
and a four-bit bus. Explicit defaults preserve this mapping; no different-board
BSP is selected. SDIO frequency, pull-ups, EN/reset polarity/timing and C6
firmware/protocol compatibility are NOT established by the table. No changes
to those electrical settings or companion firmware are made here.

## Other Relevant Signals

| Function | Table signal / P4 GPIO |
| --- | --- |
| Camera/display shared SCCB/I2C | SDA 7, SCL 8 |
| Ethernet RMII receive | RXDV 28, RXD0 29, RXD1 30 |
| Ethernet management | MDC 31, MDIO 52 |
| Ethernet transmit | TXD0 34, TXD1 35, TXEN 49 |
| Ethernet clock/reset | REF_CLK 50, PHY_RST 51 |
| UART0 | TX 37, RX 38 |
| SD card | D0 39, D1 40, D2 41, D3 42, CLK 43, CMD 44, PWR_CTRL 45 |

The table also labels GPIO24/25 as USB signal N/P. It does not establish the
high-speed USB device connector routing or VBUS safety. Camera reset, sensor
clock, MIPI lanes, PHY address and power sequencing are not inferred. GPIO35
also carries the table's BOOT annotation; do not ignore strapping constraints.

## Activation Remains Blocked

The manifest records the pinout source without marking Wi-Fi Ready or globally
verified. APP_WIFI_WIRING_VERIFIED remains off pending correspondence/electrical
checks. C6 storage audit, existing dedicated HMAC key and development AP consent
remain separate prerequisites. No flash, erase, eFuse or network startup occurs
as a result of this pin-map update.