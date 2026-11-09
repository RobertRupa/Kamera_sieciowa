# Raspberry Pi CSI Camera Compatibility

## Scope

This matrix covers official Raspberry Pi camera module families that can be
used with Raspberry Pi 1 through 4 CSI connectors. It does not cover every
third-party or clone module, USB webcam input, IP camera input, or firmware
running on Raspberry Pi computers. The target remains ESP32-P4.

The upstream sensor inventory was checked against the project's resolved
`espressif/esp_cam_sensor` 2.6.0 and `espressif/esp_video` 2.5.0 components.
Raspberry Pi module and sensor information is from the official camera
documentation. Sensor-driver availability is not proof that a complete module
works on the Waveshare board.

## Module Matrix

| Raspberry Pi module | Sensor | Driver in resolved Espressif components | Project status |
| --- | --- | --- | --- |
| Camera Module 1, including NoIR | OV5647 | Generic MIPI-CSI driver available | Sensor ID and one CSI frame observed on this unit; module-family equivalence and sustained capture unverified |
| Camera Module 2 / NoIR, manufactured before November 2026 | IMX219 | No generic driver listed | Not supported by the current firmware |
| Camera Module 2 / NoIR, manufactured from November 2026 | IMX355 | No generic driver listed | Not supported by the current firmware |
| High Quality Camera | IMX477 | No generic driver listed | Not supported by the current firmware |
| Camera Module 3 variants, including Wide and NoIR | IMX708 | No generic driver listed | Not supported by the current firmware |
| AI Camera | IMX500 | An Arducam-specific IMX500 driver exists; it is not a generic Raspberry Pi AI Camera driver | Compatibility is unverified; do not enable the Arducam driver for this module |
| Global Shutter Camera | IMX296 | No generic driver listed | Not supported by the current firmware |

NoIR, Wide, lens-mount and sensor-assembly variants may change optical or motor
features without changing the sensor family shown above. Autofocus, lens motor,
IR-cut and AI processing support are not implied by sensor-driver availability.

## Current Implementation

The firmware pins Espressif `esp_video` and compiles the generic OV5647 sensor
driver. In `APP_DEV_MODE`, the boot probe initializes MIPI-CSI through the
documented shared Platform I2C bus, reads the supported sensor ID, requests one
bounded V4L2 frame, then stops capture and deinitializes video. The current unit
reported OV5647 chip ID `0x5647` and one `800x800` CSI frame. It does not leave an
application stream running. The probe supports OV5647 only; IMX219/other sensors
are not identified by the current driver set. A detected sensor and received
frame are reported separately from the manifest's verified-board capability.

The reported Waveshare board has shared camera/display I2C labels on GPIO7/8,
but its exact PCB revision, camera connector routing, ribbon orientation,
sensor identity, power sequence, reset/PWDN, and sensor clock are not verified.
The CSI initialization configuration must be based on the actual board and
module; values from another ESP32-P4 example are not interchangeable. The
official Waveshare `Camera_Preview` / `Camera_ISP_Tuning` examples target OV5647
and reuse the Platform I2C bus; their existence does not prove that the reported
UC-439 contains OV5647 or that this physical unit's CSI lane path works. Do not
mark a module Ready or add a supported video profile until the probe passes on
the target hardware.

## Sources

- [Raspberry Pi camera hardware and sensor specifications](https://www.raspberrypi.com/documentation/accessories/camera.html)
- [Raspberry Pi Camera Module 3 specifications](https://www.raspberrypi.com/products/camera-module-3/)
- [Raspberry Pi High Quality Camera specifications](https://www.raspberrypi.com/products/raspberry-pi-high-quality-camera/)
- [Espressif camera sensor inventory](https://github.com/espressif/esp-video-components/blob/master/esp_cam_sensor/README.md)
- [Espressif video component](https://github.com/espressif/esp-video-components/tree/master/esp_video)