# Verification
## Plaintext NVS Wi-Fi Build and Hardware Run

2026-11-09: replaced the encrypted credential adapter with ordinary NVS in
`app_config`, removed HMAC key derivation/selection, and configured the P4
application as the sole persistent credential owner. ESP-Hosted 3.1.0 receives
`nvs_enable=0` for C6 Wi-Fi initialization. The remote `esp_wifi_set_storage()`
API is a no-op, so the firmware no longer treats it as a RAM-storage guarantee.
An AP can start in the explicitly authorized local dev profile; credential
sessions and STA restore remain gated on a ready store and C6 storage audit.

PASS: Host CTest 7/7, including plaintext NVS save/load/clear, failed-init
no-erase behavior, provisioning policy and capability registry. Frontend
`npm run typecheck`, Vitest 10/10 and Playwright 18/18 also passed. Chromium was
installed inside the temporary Node 22 test container.

PASS: Incremental ESP-IDF 6.1 `esp32p4` build; `camera_hub.bin` size
`0xAE960` bytes in the 8 MiB app partition. The observed device partition table
was read from COM8 and parsed with ESP-IDF; all entries matched the build,
including `app_config` at `0xE10000/24 KiB`.

PASS: Native Windows esptool 5.3.1 flash on COM8 using the ESP-IDF-generated
`flash_args` (bootloader, unchanged partition table, otadata and application).
The device booted ESP-IDF 6.1/P4; plaintext NVS initialization succeeded.
ESP-Hosted connected to the C6 over four-bit SDIO, negotiated RPC V2 with
matching 3.1.0 host/coproc versions, and the AP setup interface started DHCP at
`192.168.4.1`. The provisioning/API and mDNS startup paths returned without an
error log. No NVS/Flash erase or eFuse operation was performed, and no Wi-Fi
credentials were entered.

NOT VERIFIED: Windows Wi-Fi scan did not show `CAMERA-HUB-SETUP`; no client
associated with the AP, no HTTP request or mDNS resolution was made, and no STA
join or credential write/read-after-reboot test was performed. AP beacon/client
reachability and real credential persistence remain open hardware checks.

## Boot-Time Wi-Fi Credential Restore

2026-11-09: `NetworkProvisioner::restoreSavedCredentials()` now loads the
existing encrypted credential record after the gated AP/API startup and requests
STA connection without saving it again. Missing credentials leave the AP setup
path available; storage/connection failures do not erase the stored record. The
application waits up to30 seconds for an IP. This is implementation/build
evidence only, not a radio test.

PASS: `scripts/test-host.ps1`, CTest 7/7, including empty-store setup behavior,
credential restoration without a write, and pending connection state.

PASS: incremental ESP-IDF6.1 `esp32p4` build using
`build/sdkconfig.p4`; `camera_hub.bin` size0xB2E40 bytes. No flash, erase,
eFuse operation, credentials, AP/STA join, mDNS or encrypted-NVS hardware
read/write/reboot test was performed for this change.

## C6 OTA Through P4 Completed

2026-11-08: user authorized updating C6 via OTA from P4. Initial detailed3.1.0
probe established SDIO connect0 but identity-1: factory C6 emitted no firmware
version/RPC-version TLV, so host displayed defaults0.0.0/V2. Those values were
not proof of the installed version. Source inspection confirmed the firmware
version verdict was informational, not the direct cause of RPC failure.

Built an isolated ESP-Hosted2.13.1 P4 migration host (with remote1.6.5, IDF6.1).
PASS compile and native COM8 flash. Basic get-mode produced a real legacy RPC
response ESP_ERR_WIFI_NOT_INIT; source confirmed it calls rpc_wifi_get_mode,
not a local no-op. No Wi-Fi init/credentials were sent by the test.

Built camera_hub_radio app for ESP32-C6, ESP-Hosted3.1.0/RPCV2, with Wi-Fi NVS
disabled, no BT, no Secure Boot or Flash Encryption. Automatic NVS erase is
wrapped/refused. SDK default C6 SDIO pins were CMD18/CLK19/D020/D121/D222/D323.
Core constructor initialization and Wi-Fi feature were selected. This used a
build-time4MiB/dual-OTA budget from the official example, not a flashed partition
table or assertion of measured C6 Flash capacity. The first conservative896KiB
budget failed size validation; corrected it and reran the build successfully.
Application image size0x11a640 (1156672bytes); SDK image-info validated C6 chip13,
version3.1.0 and checksum/hash. No C6 bootloader/table/key images were transmitted.

Compiled migration host with that image, local header/version bounds and official
OTA begin/write/end/activate (1024byte chunks, no write retries). Image1a5a70
fit preserved8MiB P4 app partition. Native COM8 flash PASS; P4 transferred the
application through SDIO. At15552ms C6 accepted ota_end, then the older firmware
did not respond to Req_OTAActivate0x10a. Result was peer_validated1/activated0,
not blindly treated as success or retried. Restored normal3.1.0 P4 firmware,
whose startup reset/reconnected the coprocessor.

RUNTIME SUCCESS:

```text
RPC version negotiated: V2
esp-hosted fw versions: host=3.1.0 coprocessor=3.1.0 (match)
C6_PROBE connect=0
C6_PROBE identity=0
C6_PROBE result=ESP_OK target=esp32c6 chip_id=13
```

This verified new C6 activation/compatibility, resolving the old warning.
Normal P4 firmware was restored and all serial monitors stopped. C6 was NOT
directly UART-flashed; no eFuse changes in this OTA step. The existing HMAC key
was detected on P4 but its store slot remained-1, so AP/credentials remained
blocked. No Wi-Fi/IP/mDNS or NVS persistence test is claimed by successful RPC.
The migration host is temporary and must not remain the normal device firmware.

## HMAC eFuse Provisioning Completed

2026-11-08: after explicit user authorization for only BLOCK_KEY0/HMAC_UP and
its read/write protection, the official espefuse5.3.1 operation was executed
through scripts/provision-nvs-key.ps1 on COM8 (confirmed same P4rev1.3).
Preflight showed USER purpose, RD_DIS0 and WR_DIS0 in the actual P4v1.0 table.

PASS: ValidateOnly generated a 32-byte CSPRNG key into a current-user-only ACL
temporary file and removed it without eFuse operations. The authorized run then
generated a fresh key and called burn-key with default protections. No force,
debug or show-sensitive-info flags were used. The official tool hid all key
bytes, verified key/protection writes and reported Successful. Cleanup removed
the temporary key and cleared the generator's byte array. No actual key material
was printed, stored in Git or retained as a backup.

Post-operation metadata: KEY_PURPOSE_0=HMAC_UP0x8 (not writable), RD_DIS=1,
WR_DIS=0x00800100. get-security-info showed key purposes (8,0,0,0,0,0,12),
Secure Boot Disabled and Flash Encryption Disabled. Only the authorized key0
and its purpose/read/write protections were changed. This is irreversible;
do not rerun provisioning or attempt to read/replace the protected key.

NOT RUN: actual encrypted NVS derivation/read-write-reboot testing. No Wi-Fi
credentials, C6 OTA, network activation or new application flash were performed
in this key-provisioning step. C6 RPC readiness remains a separate issue.

## COM8 UART Boot and C6 Compatibility

2026-11-08: COM8 identified as USB-Enhanced-SERIAL CH343 (VID1A86/PID55D3).
Read-only esptool identification confirmed the same ESP32-P4 rev.1.3. Primary
console changed to UART0 with USB-Serial/JTAG secondary; CPU remains360MHz,
official legacy silicon profile min1.0/max1.99, Flash16MiB. Build PASS.

Native esptool5.3.1 flash on COM8 PASS using SDK @flash_args, app0xb28f0
(731376bytes) at0x110000; standard integrity verification passed for all images.
No whole-device erase or manual readback. Existing NVS/SPIFFS locations retained.
Hard reset over UART completed normally. Monitor opened with --no-reset.

BOOT PASS: camera_hub reached C6_PROBE at405ms, with no clock assert or observed
boot loop in this session. C6 handshake FAIL at7305ms. Actual log:

```text
E (2165) eh_init_evt: major version mismatch - OTA coprocessor from host
I (7305) camera_hub: C6_PROBE result=ESP_FAIL target= chip_id=0
W (7415) camera_hub: Provisioning start refused: ESP_ERR_INVALID_STATE
```

This establishes a factory-C6 versus ESP-Hosted3.1.0 major-version mismatch,
not wrong SDIO pins or a failed flash. Exact C6 firmware version was not read.
No Wi-Fi/mDNS readiness or encrypted persistence is claimed. No station
credentials were supplied; NVS key remains absent/unselected. Limited HMAC
BLOCK_KEY0 authorization exists, but no eFuse or C6 firmware write was executed.
Monitor was stopped and COM8 released after recording the result.

## Physical Flash and Startup Diagnostics

2026-11-08: actual flash was executed on COM7. Initially SDK rejected a rev.3.x
image for the observed ESP32-P4 rev.1.3. No force option was used. The official
ESP32P4_SELECTS_REV_LESS_V3 profile and minimum1.0/maximum1.99 were selected.
The first valid flash passed standard integrity checks, but startup asserted in
esp_clk_init because the cached CPU choice remained400MHz. SDK rtc_clk.c for
the selected legacy silicon explicitly supports360MHz, not400MHz; corrected the
CPU choice without disabling the assert. Primary console now USB-Serial/JTAG.

An RFC2217 retry failed negotiation before writing. Per user's request, switched
to native Windows esptool5.3.1 on COM7, using SDK-generated @flash_args. Native
flash PASS, including standard verification for all four images. Latest application
0xb1330 (725808bytes), written at0x110000. Bootloader0x2000, table0x8000,
OTA initialization0x10d000. No full erase or redundant manual readback/checksum.

The actual prior table was read as metadata and parsed with the official SDK tool.
Existing nvsfactory0x9000/200KiB, nvs0x3b000/840KiB, phy_init0x10f000 and
storage SPIFFS0x910000/5MiB retain their addresses and were not flashed.
Factory app remains0x110000/8MiB; new app_config is0xe10000/24KiB in free space.
OTA metadata is initialized as part of normal flash. Old NVS may contain legacy
credentials; preserving it is not proof that all historical device data is encrypted.

Monitor resets selected USB download mode; watchdog reset triggered USB
reenumeration (esptool reported a lost Windows port handle). Physical RST at an
open monitor likewise produced ClearCommError/reconnect. After reopening, no
C6_PROBE result was captured. Therefore flash PASS, latest application boot/C6
handshake UNVERIFIED, Wi-Fi/mDNS NOT TESTED. Do not claim the clock fix's runtime
success solely from the new build or absence of logs. UART/DEBUG console capture
is needed next. All temporary bridges and monitors were stopped.

The user explicitly authorized BLOCK_KEY0/HMAC_UP and its read/write protection.
No eFuse programming has yet been executed: deferred until startup/transport can
be verified. Last read showed all six key purposes USER/EMPTY, Flash Encryption
and Secure Boot disabled. No credentials or network readiness were invented.

## Supplied Pin Map

2026-11-08: the user supplied a GPIO allocation image. Transcribed SDIO D0..D3
14..17, CLK18, CMD19 and C6_EN54 into the manifest and explicit defaults. GPIO6
is C6_GPIO2, not reset. Shared camera I2C is SDA7/SCL8 and PHY reset is GPIO51.
Source/provenance and remaining electrical uncertainty are recorded in
hardware/pin-map.md; the image has no visible PCB revision.

PASS: PowerShell structured checks against build/p4/config/sdkconfig.json verified
all six SDIO signals, EN, slot1 and four-bit width. Manifest EN matched GPIO54.
Automatic ESP-Hosted startup and wiring/C6-storage audit flags remained off.
This checked the existing successful build configuration; no new target build,
flash, eFuse programming or hardware/network test was executed for this update.

## Provisioning and STA Integration

2026-11-08: implemented IWifiStation, NetworkProvisioner, WifiService STA/IP
tracking and ProvisioningApi with session/credentials/operation handlers. The
composition root now conditionally starts AP, HTTP and mDNS and services queued
persistence/connect requests outside HTTP/event callbacks. Default start remains
blocked: HMAC slot -1, audited wiring/C6-storage/open-AP flags all false.

PASS: `scripts/test-host.ps1`, CTest 7/7. New NetworkProvisioner tests cover no
I/O from submit, one pending request, missing store, write-before-connect ordering,
no driver call after failed write, timeout/retry and IP-confirmed Connected.
These are port fakes, NOT a radio or live HTTP test.

PASS: final ESP-IDF 6.1.0 esp32p4 build with linked conditional startup and HTTP,
image 0x7e4f0 bytes, 51% of factory partition free. cJSON moved from SDK json to
Component Manager; a missing component error was fixed and the build rerun.
HTTPD_DEFAULT_CONFIG required an explicit C++ type, and mbedtls_ct_memcmp required
C linkage. Both compile/link defects were fixed without disabling checks.
Current parser limits are CJSON_NESTING_LIMIT=4 and CJSON_CIRCULAR_LIMIT=8.
ESP-Hosted automatic pre-app_main initialization remains disabled.

NOT RUN: live endpoint authentication, malformed HTTP input/fault/stack tests,
AP/STA/mDNS, C6 compatibility and credential RAM-storage audit, actual encrypted
NVS persistence. No flash, erase, network credentials or eFuse modifications.
This is still an incomplete device wizard: frontend assets/capability endpoints,
scan/reconnect/reboot restoration and captive DNS are pending.

## Encrypted NVS Store Implementation

2026-11-08: user selected the secure persistent variant, no RAM-only substitute,
no flash and no eFuse programming. Added WifiCredentials/ICredentialStore,
versioned WifiCredentialCodec and EncryptedNvsStore. Store initialization is
called by the diagnostic root but blocked by APP_NVS_HMAC_KEY_ID=-1 by default.
No credential save/clear calls are made by startup.

PASS: `scripts/test-host.ps1`, CTest 6/6. Added codec roundtrip/boundary/version
tests and actual-adapter contract tests with isolated fake NVS SDK APIs. Covered
missing key, unselected slot, missing/preinitialized partition, secure-init failure,
NotReady write refusal, record read/write, corruption, unknown version, commit
failure and explicit scoped clear. These fakes are NOT XTS-AES/HMAC verification.

Verified SDK 6.1 source: secure_init_partition returns success for any existing
storage. Added a read-only preinitialization probe and regression tests to avoid
a false encrypted-ready claim. Exclusive partition initialization is required.

PASS: incremental ESP-IDF 6.1.0 build for esp32p4 after final integration,
`idf.py -B /workspace/build/p4 -D SDKCONFIG=/workspace/build/sdkconfig.p4
-D IDF_TARGET=esp32p4 build`. Image 0x41b60 bytes, 74% of the 1 MiB app partition
free. Proposed app_config at 0x110000/0x6000; no partition writes executed.

NOT RUN/BLOCKED: real encrypted NVS writes/readout/reboot/power-failure tests,
key provisioning, C6 storage audit, partition migration and full Wi-Fi/API workflow.
Last observed device has no HMAC key. No flash, erase, eFuse or credentials changes.

## Wi-Fi and mDNS Implementation

2026-11-08: user selected implementation/build only, without eFuse programming
or flash. READ-ONLY hardware identification with local Windows esptool 5.3.1:

```text
python -m esptool --port COM7 --baud 115200 get-security-info
python -m esptool --port COM7 --baud 115200 flash-id
```

Observed ESP32-P4 revision 1.3, USB-Serial/JTAG and 16 MiB flash. Key purposes
for slots 0..5 were all USER/EMPTY; Flash Encryption and Secure Boot disabled.
No flash writes, erase or eFuse programming were performed. The ROM stub ran in
RAM and the device was reset by esptool. PCB revision and C6 firmware remain unknown.

PASS: ESP-IDF 6.1.0 build of NvsKeyReader, WifiService and MdnsService. Component
Manager resolved esp_hosted 3.1.0, esp_wifi_remote 1.6.5, mdns 1.15.1 and saved
the lockfile. Auto-init before app_main was found enabled by default in ESP-Hosted;
it was disabled in sdkconfig.defaults/current sdkconfig and guarded at compile time.
The subsequent build passed with image size 0x38c80 (78% factory partition free).
Upstream Kconfig notes and an unused-function warning remain; no safety check was disabled.

PASS: host CTest 4/4, including all 16 combinations of provisioning prerequisites
and SSID/password/mDNS label bounds. Frontend typecheck and build PASS; Vitest
10/10 and Chromium 18/18 across desktop/mobile/small-mobile. Wi-Fi screenshots
were inspected. Successful submission tests use controlled API fixtures, NOT C6.
Current JS bundle is 607.42 kB (158.37 kB gzip); Vite size warning remains.

NOT RUN: AP radio startup, C6 handshake, provisioning HTTP server/captive portal,
real Wi-Fi credentials, encrypted NVS persistence/reboot, actual mDNS resolution.
These adapters are deliberately not called from the diagnostic composition root.
NvsKeyReader derives from existing HMAC only and never generates keys; default
nvs_flash_init is not used. Physical security provisioning and C6 storage audit
remain blockers, and the user did not authorize irreversible operations.

Observed on Windows with Docker Linux engine. This report is for the development
foundation, not proof of a functioning camera or network service.

## Host

GCC 14.4 / CMake 3.31, `-Wall -Wextra -Wpedantic -Werror`:

```text
cmake -S tests/host -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

PASS: 3/3 CTest executables (frame_broker, profile_policy, capability_registry).
The capability tests are reused directly from the upstream template.
The broker test includes
10,000 publications with one producer and three concurrent consumers. The first
build caught a test self-move warning; the test was repaired and rerun successfully.

## Target

PASS: ESP-IDF 6.1.0 / esp32p4 diagnostic build in `espressif/idf:v6.1`:

```text
idf.py -B /workspace/build/p4 -D SDKCONFIG=/workspace/build/sdkconfig.p4 -D IDF_TARGET=esp32p4 build
```

Output: `build/p4/camera_hub.bin`, size `0x2f730` bytes, 81% of the 1 MiB
factory partition free. The SDK compiled all added C++ components successfully.
The default image header is conservative (2 MiB flash); this does not replace
the actual board flash/PSRAM probe. No hardware claim follows from a build.
The initial large image download had no observed completion; the command was
rerun with an unlimited timeout and completed successfully.

## Frontend

PASS: `npm run typecheck`, `npm test` (4 tests), `npm run build`,
`npm run test:browser` (3 Chromium projects: 1440x900, 390x844, 320x740).
Browser checks cover offline/error state, absence of a simulated video,
locale and theme switching, diagnostics and no horizontal overflow. Desktop
and mobile screenshots were inspected. Tests use a deliberately aborted device
request, not a camera or hardware simulation.

Latest PrimeVue 5 required a commercial license and showed an error in screenshots.
It was replaced with registry-verified MIT PrimeVue 4.5.5 / themes 1.2.5,
and all checks were rerun; the absence of the license error is now asserted.
Fonts are bundled locally. Npm audit reported zero vulnerabilities on installation.
Vite warns about the 582.57 kB JS chunk (151.21 kB gzip): partition/asset
budgeting and code splitting remain pending before device-side deployment.

Camera diagnostics update: PASS `npm run typecheck`, `npm test` (7 tests),
`npm run build` and `npm run test:browser` (12 checks across the same three
viewports). Added checks cover legacy status compatibility, independent hardware
identification without an active stream, reported UC-439 rev.B versus Unknown
detection, and IMX219/OV5647 identification from controlled API fixtures.
These fixtures are browser test data, not a physical camera detection result.

## Hardware Manifest Gate

BLOCKED: `python examples/hardware/validate_manifest.py docs/hardware/board.json`
returns errors for missing PCB revision and unresolved hardware issues. This
is intentionally not hidden by marking the actual board as an example or
inventing wiring. The manifest is a draft; hardware/release acceptance is blocked.

## GitHub and CI

CI configuration was added but has NOT RUN remotely. No GitHub token environment
variables or gh CLI are available; no secret was displayed or requested through
chat. No fork, git commit or push was performed. The upstream remote is named
`upstream`, with no origin until the user's fork is created.

## Not Run

Hardware boot/flash, actual camera capture, USB enumeration, MJPEG/RTSP clients,
simultaneous streaming/endurance, GPIO audit, Ethernet/C6 Wi-Fi, captive portal,
encrypted NVS/key provisioning and production security verification: NOT RUN.

There is no authenticated GitHub publishing tool available in this session.
The checkout retains upstream history; no fork or remote publication was performed.