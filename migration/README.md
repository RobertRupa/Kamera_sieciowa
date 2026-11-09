# P4-Hosted C6 OTA Migration

This directory records the completed one-time transition from the factory C6
legacy RPC firmware to ESP-Hosted3.1.0/RPCV2. It is not the camera application.
Normal firmware remains in ../firmware. No real SSIDs, passwords or keys are used.

## Projects

- host_legacy: ESP32-P4/ESP-Hosted2.13.1, remote1.6.5, IDF6.1. First used only
  get-mode to establish a real old RPC reply; then temporarily embedded the new
  C6 application and used begin/write/end/activate over the on-board SDIO link.
- radio_c6: C6/ESP-Hosted3.1.0 application, RPCV2/Wi-Fi, driver NVS disabled.
  No Secure Boot/Flash Encryption or automatic NVS erase. Its build partition
  table is only an image budget, NOT the device's table and must NOT be flashed.

The C6 application binary is a generated ignored artifact, not a committed image:

```powershell
docker run --rm --mount "type=bind,source=$($PWD.Path),target=/workspace" -w /workspace/migration/radio_c6 espressif/idf:v6.1 idf.py -B /workspace/build/radio-c6 -D SDKCONFIG=/workspace/build/sdkconfig.radio-c6 -D IDF_TARGET=esp32c6 build
Copy-Item build/radio-c6/camera_hub_radio.bin migration/host_legacy/main/c6_image.bin
docker run --rm --mount "type=bind,source=$($PWD.Path),target=/workspace" -w /workspace/migration/host_legacy espressif/idf:v6.1 idf.py -B /workspace/build/migration-host -D SDKCONFIG=/workspace/build/sdkconfig.migration-host -D IDF_TARGET=esp32p4 build
```

## Safety and Result

Do not rerun this migration routinely. The embedded host attempts an update on
boot only after a legacy RPC reply. Remove/restore that temporary host immediately
after the attempt; it is not a general boot/retry/update engine. Never substitute
a C6 bootloader, table, key image or a different-target binary for c6_image.bin.
The service requires a C6/version3.1.0 header and length within the build budget.
The C6 OTA handler validates the actual slot and full image; failures stop before
activation. No forced checks, partition erase or automatic write retry is allowed.

The completed transfer used1156672 application bytes. ota_end succeeded but
the old factory did not answer OTAActivate. No second transfer was attempted.
Restoring P4's3.1.0 host reset the C6; a real handshake then reported version3.1.0,
RPCV2 and chip13/esp32c6 with successful identity. That runtime confirmation,
not merely the transfer, establishes successful activation. See ../docs/verification.md.

No C6 direct flashing or eFuse programming occurred here. Wi-Fi network operation,
device UI and credential read/write tests are separate follow-up work.