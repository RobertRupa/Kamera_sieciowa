# LVGL, display hardware and BSP

## Activation and stack
Only enable when the target has a **verified physical LCD**. Use **LVGL v9** via official `espressif/esp_lvgl_port`, `esp_lcd` and maintained board BSP/display/touch driver. If no display exists, omit GUI component/tasks/buffers and do not add fake LCD code. LVGL version and ESP port compatibility must be verified during dependency selection.

## MVC / ownership
- LVGL **View** owns objects/styles/screens/widgets.
- UI presenter/controller translates user actions to application services and translates immutable state snapshots back to widgets.
- Service/domain state is shared with WebGUI; LVGL never directly writes Wi-Fi credentials or modifies HTTP handlers.
- BSP owns LCD init, touch, backlight, power, bus and DMA requirements; pin mapping is documented per board revision.

**Threading invariant:** mutate LVGL only from owning GUI task or under the documented `esp_lvgl_port` lock. HTTP/network/ISR callbacks enqueue bounded events; never call LVGL APIs directly from another task. Prefer port's tick/task/rotation/power features over a custom duplicate scheduler. Do not block rendering callbacks with NVS writes, Wi-Fi scan or large JSON generation.

## Hardware manifest checklist
LCD controller, panel resolution/rotation, interface (SPI/RGB/DSI/i80), physical pins/clock, backlight logic/PWM, touch controller address/reset/interrupt, pixel format, SRAM/PSRAM size, frame buffers, DMA/cache alignment, display initialization order and render buffers. Board revision/schematic required: similar panels may have incompatible pinouts. ESP32-P4 with C6 companion must separate display logic from network transport.

## UI behavior
Create dashboard, status/settings and optional Wi-Fi wizard; use componentized screen classes and consistent navigation. Support theme tokens (`system` mapping documented if no OS, `light` and `dark`), English and Polish UI resources. Include fonts with the required Polish glyph range; verify clipping and touch target size.

## Performance and fault behavior
Measure refresh rate, touch latency, lowest free heap, largest block, PSRAM usage, frame/draw buffer placement, Wi-Fi+GUI concurrency, power draw if relevant. PSRAM presence alone does not guarantee DMA-compatible buffers. Use partial draw buffers when appropriate and initialize hardware only after board capability validation. Handle missing touch, failed display initialization and UI task faults without corrupting network settings or breaking headless WebGUI operation.