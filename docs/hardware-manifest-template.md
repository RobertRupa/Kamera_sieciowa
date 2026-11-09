# Hardware manifest template for a firmware project

Copy into the firmware repository as `docs/hardware.md` and replace every TBD with evidence. **No GPIO, power level, display type or pin assignment below is a confirmed hardware fact.**

## Board identity
| Property | Verified value | Evidence |
| --- | --- | --- |
| Board + PCB revision | TBD | silkscreen/schematic |
| MCU target (`IDF_TARGET`) | TBD | chip marking + official IDF support |
| ESP-IDF release tested | TBD | local build log |
| Flash / PSRAM | TBD | vendor datasheet + boot logs |
| Native Wi-Fi / external companion | TBD | MCU + BOM/schematic |
| LCD controller, size, interface | TBD / absent | panel datasheet/BSP |
| Touch controller/address/pins | TBD / absent | schematic/BSP |
| Power input/current/rails | TBD | schematic / measurement |
| Boot/USB/JTAG pins | TBD | official SoC + board docs |

## GPIO/bus assignments
| Function | ESP GPIO | Direction | Voltage | Connected IC/connector | Conflicts and source |
| --- | --- | --- | --- | --- | --- |
| LCD reset | TBD | out | TBD | TBD | TBD |
| LCD backlight | TBD | out | TBD | TBD | TBD |
| Touch SDA/SCL | TBD | I2C | TBD | TBD | TBD |
| External radio bus | TBD | TBD | TBD | TBD | TBD |
| Other peripherals | TBD | TBD | TBD | TBD | TBD |

## Firmware capabilities
- Wi-Fi driver: native / remote adapter / unsupported; firmware/protocol for companion:
- AP wizard + mDNS feasibility:
- Display LVGL feasibility/driver:
- Credential NVS partition, plain-storage risk disclosure and Wi-Fi driver persistence path:
- Partitions (NVS/NVS keys/app/UI/OTA as needed) and Flash budget:
- Draw buffers location and DMA/PSRAM constraints:
- Tasks/queues with stack/heap reservations:
- Serial boot/upload port and bootloader constraints:

## Assumptions and open questions
Record each unverified mapping, datasheet ambiguity and needed bench measurement here. Agents must not implement unverified pins as established fact.