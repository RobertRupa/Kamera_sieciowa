# Security profiles and plaintext NVS

## Current Device Key State

The user-authorized BLOCK_KEY0/HMAC_UP key was programmed on COM8 on 2026-11-08,
with official read/write and purpose protections verified. It cannot be exported
or rewritten. Secure Boot and Flash Encryption were not enabled. The temporary
key file was removed, and no key values were logged or committed. Older statements
below that the device has no key refer to earlier stages.

This verifies key metadata/protection only. Current firmware does not use this
key for NVS, and plaintext credential storage makes no at-rest confidentiality
claim. Never rerun key generation as part of normal build, flash, onboarding or
retries.

## Two explicit profiles
| Setting | `dev` (local lab) | `release` (networked deployment) |
| --- | --- | --- |
| Default WebGUI account | **`admin/admin` permitted** only by explicit development compile flag | forbidden; unique password/onboarding identity required |
| API debug routes | available behind dev-mode check, local segment only | disabled/compiled out |
| Serial/JTAG | available for development | as required by deployment threat model |
| HTTP | local AP/LAN allowed with security warning | HTTPS or justified protected-network transport; protect credentials |
| AP wizard | first boot/fallback, time-bound and rate-limited | access-controlled, unique setup secret/physical trigger |
| NVS Wi-Fi credentials | Plaintext; warn Flash readers can recover them | Plaintext by project policy; warn Flash readers can recover them |
| Flash encryption/eFuses | plan separately; irreversible effects require authorization | enforce target-specific key strategy and production policy |
| OTA | optional in dev; no forced backup | signed/validated rollout as required |
| Checksum/integrity | ESP-IDF default intrinsic checks remain | ESP-IDF intrinsic checks remain |

## Dev-mode defaults
- Build-time Kconfig names (proposed project options): `CONFIG_APP_DEV_MODE`, `CONFIG_APP_DEV_DEFAULT_CREDENTIALS`, `CONFIG_APP_WEBGUI_ENABLED`, `CONFIG_APP_LVGL_ENABLED`.
- Permit `admin/admin` only when both first two flags are true, at a deliberately local test device. Show visible “Development firmware / insecure default password” banner until credentials changed.
- Compile- or startup-fail if default credentials appear when `CONFIG_APP_DEV_MODE` is false. Do not enable any anonymous settings/configuration endpoints by accident.
- Prefer hashed password verification and session cookies rather than plain comparison in controllers. Debug-only compiled-in default is not a secret; keep it out of reusable production firmware.
- No known real Wi-Fi passphrase or public IP embedded in source, `sdkconfig.defaults`, frontend bundle or logs.

## Plaintext NVS policy
This project intentionally stores Wi-Fi credentials unencrypted in the ordinary
`app_config` NVS partition, namespace `camera_hub`, key `wifi_v1`. This policy
applies to development and release firmware. Anyone able to read or dump Flash
can recover the stored SSID and passphrase. Do not describe the data as encrypted,
protected at rest, or confidential against physical access.

The P4 application is the only persistent credential owner. ESP-Hosted passes
`nvs_enable=0` to the matching C6 Wi-Fi initialization, disabling the companion
driver's NVS path. The remote `esp_wifi_set_storage()` implementation is a no-op;
do not rely on `WIFI_STORAGE_RAM` as proof of persistence behavior. The C6 source
and matching 3.1.0 runtime identity have been audited. AP startup and DHCP at
`192.168.4.1` were observed; Windows did not detect the AP SSID, and client
reachability, STA operation and credential reboot persistence remain untested.

The NVS adapter uses ordinary `nvs_flash_init_partition`, the `camera_hub`
namespace and one fixed 98-byte versioned blob. Writes use `nvs_set_blob` followed
by `nvs_commit`; clear removes only `wifi_v1`. Initialization or read/write
errors never trigger partition erase, format, credential logging, or eFuse
operations. Existing records from any earlier format are not migrated or erased
automatically; a failed plain-NVS open must be investigated without destructive
recovery.

BLOCK_KEY0/HMAC_UP was provisioned and protected on COM8 on 2026-11-08 under
separate authorization. This firmware does not derive or use that key for NVS.
Secure Boot and Flash Encryption remain disabled. Do not regenerate keys, burn
eFuses, enable flash encryption, or change the partition table as part of normal
build, onboarding, or flashing.

## Web API security minimum
- Authenticated session for settings, reboot, scans of local networks and firmware actions; provisioning session has limited scope and timeout.
- Bound payload lengths, validate all strings, restrict HTTP methods, cap auth attempts, use CSRF protection for cookie-authenticated state changes.
- No credentials, tokens, NVS key material or private identity information in logs, WebGUI `GET` output or error traces.
- Define session expiration, password-change endpoint and logout; use secure/random tokens from ESP-IDF RNG and hash credentials with suitable maintained library.
- Restrict CORS; use cookie `HttpOnly`/`SameSite` and `Secure` only when HTTPS in use. With plain HTTP on local dev LAN, warn about transport exposure and never extend to the internet.
- Provide a deliberate factory reset option scoped to app settings; do not conflate reset with routine upload/flash.

## Key and flash lifecycle
The first enablement of Flash Encryption, key provisioning, Secure Boot or anti-rollback may be device-irreversible. Require a separate documented operation with explicit human authorization. Development build/flash fast path must **not** include these steps. Ordinary ESP-IDF firmware image integrity mechanisms remain enabled even when skipping human checksum comparisons.

## Acceptance evidence
Capture target, IDF version, config flags, partition scheme, C6 `nvs_enable` setting,
and read/write/reboot test results. Confirm the application is the only persistent
credential owner. State plainly that Flash confidentiality is not provided. When
hardware readback or reboot verification is unavailable, report it as not tested.