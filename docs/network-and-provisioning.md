# Network, Wi-Fi onboarding, AP wizard and captive portal

## Mandatory behavior (Wi-Fi capable boards)
First boot with no valid credentials must start a **configurable SoftAP + browser wizard**. Workflow: `UNCONFIGURED → AP_PROVISIONING → CREDENTIALS_RECEIVED → CONNECTING → CONNECTED`; failed credentials return to provisioning with a useful error. On connection loss: `CONNECTED → RETRYING → CONNECTED/AP_FALLBACK`, with bounded exponential backoff, timeout and no busy polling. Physical/recovery trigger or authorized WebGUI request may re-open the wizard.

Use official `esp_wifi`, `esp_netif` and `esp_event`. Keep event callbacks short and event-driven. Use maintained **`espressif/network_provisioning`** for protocomm-based provisioning (migrated from `wifi_provisioning` in ESP-IDF 6), where appropriate. This is **not itself** a fully styled browser captive portal: combine with `esp_http_server` plus reviewed DNS/OS probe handlers, e.g. `nordesems/esp-captive-portal` where supported. Do not rewrite full DNS or OS portal probes without a documented reason.

## Wizard UI and functionality
- Setup SSID: `DEVICE-SETUP-<short-id>`. Lab builds may expose unsecured AP only by explicit dev flag and visible warning; release profile needs unique setup secret or physical unlock.
- HTML wizard accessible through AP IP (often `http://192.168.4.1`, check actual configured IP) even if a phone does not auto-open captive page.
- AP DNS captive handling on UDP/53 and correctly scoped HTTP captive network checks/redirects; no redirect loops or hijacking STA/LAN DNS.
- Scan nearby networks, SSID selection, manual/hidden SSID entry, passphrase show/hide, clear validation, loading, connection state and error/retry.
- Preserve user access during AP→STA switch: display new IP and mDNS name when known; do not assume browser follows networks automatically.
- Protect configuration endpoints with a limited provisioning session token and bounded time/attempts. Separate from admin permissions.

## Credential transaction
1. Validate SSID/password constraints and HTTP body limits.
2. Persist the credential record in ordinary, unencrypted NVS. Do not claim at-rest confidentiality; ensure the Wi-Fi companion driver does not persist a second copy. See [Security profiles](security-and-profiles.md).
3. Start network change asynchronously and return operation ID. Handle power interruption and specify retention/rollback behavior on failed connection.
4. After IP acquired, expose mDNS and normal WebGUI. Test credentials retained after power cycle and recoverable wrong-password flow.
5. Never log/store passwords in HTTP GET responses, events or crash diagnostics.
At boot, after the plaintext NVS store and AP provisioning API are ready, the
application may load the saved credential record and request a STA connection.
This read does not rewrite credentials. A missing record leaves the AP wizard
available; storage or driver errors fail the operation without clearing the
record. The current composition applies a 30-second connection deadline and
keeps AP+STA available during onboarding. After a confirmed STA IP, the setup AP
is stopped after a five-second browser handoff window; the read-only WebGUI and
status remain available at the assigned STA IP. Automatic retry/backoff remains
pending; reboot restoration is implemented, while recurring radio persistence
tests remain hardware work.

## mDNS
Use `espressif/mdns` to publish sanitized configurable `<device>-<id>.local` plus `_http._tcp` and `_https._tcp` if active; TXT may include firmware/API version but never secrets. Start on IP-ready events, update on IP/interface/name change; do not assume resolution across VLANs/client isolation, expose numeric IP too.

## Target-specific constraints
ESP32-P4 has **no built-in Wi-Fi**: an external C6/etc companion must provide a validated network driver (e.g. supported ESP-Hosted/remote Wi-Fi solution) behind `IWifiService`. ESP32-H2 has no regular Wi-Fi; require external transport/gateway, or an explicit headless/network-limited capability profile. Never fake Wi-Fi support on those targets. If Ethernet is supported, integrate it as a separate interface and apply appropriate mDNS policy.

## Tests
First boot, captive detection on mobile, manual IP access, wrong password, hidden SSID, scan failure, AP client disconnect mid-write, STA loss/reconnect, NVS write failure, mDNS service resolution, network name conflict, device reboot, HTTP API authentication and memory/stack impact under UI use.

## Current Implementation State

The local dev wizard now has an AP-peer/origin-scoped readiness endpoint and
authenticated asynchronous network scan and connection operations. Scans use
Espressif's documented `esp_wifi_scan_start(..., false)` and
`WIFI_EVENT_SCAN_DONE` lifecycle, retrieve at most 20 records, and are cancelled
by the application worker after 20 seconds. Scan and connection operations are
serialized so credentials are not applied while a scan is active. The main-task
stack is explicitly sized to 8192 bytes for the remote scan result path. The Vue
client supports scan selection, manual SSID entry and operation polling; it does not
persist the provisioning token or Wi-Fi password.

This is not yet a complete device-hosted captive wizard: the ESP-IDF component
`nordesems/esp-captive-portal` currently does not list ESP32-P4 as a supported
target, so this project has not enabled its DNS interception or replaced it with
a handwritten DNS server. The Vite build is embedded as deterministic gzip
assets and served from the same AP origin at `http://192.168.4.1/` and the
confirmed STA IPv4. GET status/assets work on STA; provisioning mutations remain
AP-only. Mobile captive detection, the scan path after the main-stack fix, STA
loss/retry, and credential persistence across later reboots remain NOT RUN. The
active local `sdkconfig`
sets `APP_WIFI_WIRING_VERIFIED` for the user-confirmed base SKU 30560 transport
mapping and sets `APP_DEV_OPEN_SETUP_AP` for the explicitly authorized dev/lab
AP. Checked-in defaults remain closed; this does not authorize an open AP in
release builds.