# Development vs release security profile and encrypted NVS

## Two explicit profiles
| Setting | `dev` (local lab) | `release` (networked deployment) |
| --- | --- | --- |
| Default WebGUI account | **`admin/admin` permitted** only by explicit development compile flag | forbidden; unique password/onboarding identity required |
| API debug routes | available behind dev-mode check, local segment only | disabled/compiled out |
| Serial/JTAG | available for development | as required by deployment threat model |
| HTTP | local AP/LAN allowed with security warning | HTTPS or justified protected-network transport; protect credentials |
| AP wizard | first boot/fallback, time-bound and rate-limited | access-controlled, unique setup secret/physical trigger |
| NVS Wi-Fi credentials | **encrypted at rest required** | **encrypted at rest required** |
| Flash encryption/eFuses | plan separately; irreversible effects require authorization | enforce target-specific key strategy and production policy |
| OTA | optional in dev; no forced backup | signed/validated rollout as required |
| Checksum/integrity | ESP-IDF default intrinsic checks remain | ESP-IDF intrinsic checks remain |

## Dev-mode defaults
- Build-time Kconfig names (proposed project options): `CONFIG_APP_DEV_MODE`, `CONFIG_APP_DEV_DEFAULT_CREDENTIALS`, `CONFIG_APP_WEBGUI_ENABLED`, `CONFIG_APP_LVGL_ENABLED`.
- Permit `admin/admin` only when both first two flags are true, at a deliberately local test device. Show visible “Development firmware / insecure default password” banner until credentials changed.
- Compile- or startup-fail if default credentials appear when `CONFIG_APP_DEV_MODE` is false. Do not enable any anonymous settings/configuration endpoints by accident.
- Prefer hashed password verification and session cookies rather than plain comparison in controllers. Debug-only compiled-in default is not a secret; keep it out of reusable production firmware.
- No known real Wi-Fi passphrase or public IP embedded in source, `sdkconfig.defaults`, frontend bundle or logs.

## Encrypted NVS is not just an encrypted flag
ESP-IDF stores Wi-Fi credentials in the **default NVS** unless driver storage is configured otherwise. If that partition is plaintext, writing a second encrypted copy elsewhere does **not** satisfy the requirement.

Supported implementation choices (verify per chip and ESP-IDF release):
1. Configure ESP-IDF NVS encryption, suitable `nvs_keys` key partition, and a protected key scheme (typically Flash Encryption-based; supported devices may provide an HMAC-based scheme).
2. Alternatively select `esp_wifi_set_storage(WIFI_STORAGE_RAM)` and persist credentials only through a verified encrypted NVS partition, restoring them to Wi-Fi at connection time; audit every code path.

Important: standard NVS **must not** simply be flagged `encrypted` in partition CSV and assumed protected. Encryption is implemented by the NVS library and its dedicated key mechanism. Key storage and first-boot initialization must be designed. Flash Encryption can require eFuse writes and change development flashing capabilities. **Never burn eFuses automatically** as part of ordinary build/flash scripts. If the chip does not support an acceptable non-irreversible NVS key mechanism, document the conflict explicitly and obtain authorization before irreversible security setup. Do not fake encryption with XOR, ROT, encoding or a key stored next to ciphertext in readable Flash.

Official reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_encryption.html

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
Before calling the device “encrypted NVS capable”, capture target, IDF version, config flags, partition scheme, Wi-Fi storage mode and a read/write/reboot test verifying that credentials survive restart without plain copies. When physical readout verification is unavailable, say precisely what has and has not been validated.