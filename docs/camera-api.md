# Camera API Contract (Planned)

## Implemented Provisioning Subset (Local Development Profile)

`ProvisioningApi` implements GET `/api/v1/provisioning/status`, POST
`/api/v1/provisioning/session`, POST `/api/v1/network/scan`, GET
`/api/v1/network/scan/{id}`, PUT `/api/v1/network/credentials`, and GET
`/api/v1/network/operations/wifi-{id}`. It also implements read-only GET
`/api/v1/device/status` and GET `/api/v1/device/capabilities` in the development
profile. The server is conditional and blocked by checked-in defaults. General
device/admin endpoints and account authentication remain unimplemented. The
prepared Vue assets are embedded in the development firmware and served from
AP and STA origins.

The current HTTP service is an explicitly authorized local-development open AP
profile only, not a release onboarding design. AP startup requires verified
APP_WIFI_WIRING_VERIFIED and APP_DEV_OPEN_SETUP_AP. Credential sessions additionally
require a ready plaintext NVS store and APP_C6_STORAGE_AUDITED. The matching
ESP-Hosted C6 must receive `nvs_enable=0`; no HMAC key is used. Do not enable
hardware audit flags merely to bypass checks.

Static assets and GET `/api/v1/provisioning/status` accept the setup AP IPv4 or
the currently confirmed STA IPv4. Their Origin, if present, must match that
numeric local origin. The status DTO contains readiness, `station_ip` (string or
null), and `provisioning_available` (true only on the AP); it exposes no
credentials. All session, scan, credential-write and operation routes remain
AP-only. There is no permissive CORS or unauthenticated LAN configuration
endpoint. A provisioning session is bound to its peer IP, lasts 180 seconds,
uses a random 64-character token and is issued at most once per five seconds.
This is not an admin session. A valid token authorizes writes and operation reads
only while the request arrives on the AP interface.

The unauthenticated status route is limited to local AP/STA addresses and returns
`wifi`, `wifi_wizard`, `credential_storage`, `station_ip`, and
`provisioning_available`. It reveals no credentials or board secrets. JSON bodies require Content-Type
application/json, length 1..384 bytes, exactly
two string fields ssid/password, no raw/escaped nulls and bounded parsing depth 4.
Receive deadline is 3 seconds plus at most one 2-second socket receive wait.
Malformed/oversized/nested data is rejected; this does not remove parser/library
resource bounds. Password/body/token scratch buffers are cleared without logging.

PUT queues one operation and returns 202 / operation_id wifi-{id}. The application
worker saves the complete pair first, then requests STA. Storage failure prevents
driver connection; failed persistence has no automatic erase/rollback. GET returns
the current operation state (queued/persisting/connecting/connected/failed); older
IDs return 404. Successful driver request is not Connected: real IP and matching
SSID must be observed. Connection observation is bounded to 30 seconds.
Queued/persisting/connecting states reject another request with 409.

Wi-Fi scans use Espressif's asynchronous `esp_wifi_scan_start(config, false)`,
`WIFI_EVENT_SCAN_DONE`, and documented scan-result retrieval APIs. One scan is
active at a time, results are capped at 20, and the app worker cancels a scan
after 20 seconds. POST `/api/v1/network/scan` returns 202 with `data.operation_id`
(`scan-{id}`); GET `/api/v1/network/scan/{id}` returns `state` and, on completion,
`results` containing SSID, RSSI and security. Scan start/result requests use the
same peer-bound provisioning token as credential writes. Static assets and the
STA-read-only status route were reached from the host on the same LAN; status
returned a valid quoted `station_ip`. The final AP scan path after increasing
the main-task stack is not yet verified from hardware. Captive DNS is NOT
IMPLEMENTED.

## Provisioning Client Contract

The Wi-Fi panel now reads readiness from the AP-local provisioning status route.
It can scan when Wi-Fi and the wizard are ready; credential submission also
requires ready credential storage.

1. GET `/api/v1/provisioning/status` reports AP/STA-local readiness and the
  assigned station IP without secrets; `provisioning_available` is false on STA.
2. POST `/api/v1/provisioning/session` returns `data.token` (32..256 characters)
  and `data.expires_in_seconds` (1..300). The token is held in browser memory only.
3. POST `/api/v1/network/scan` with the scoped Bearer token returns 202 and a
  `scan-{id}` operation. The client polls GET `/api/v1/network/scan/{id}`.
4. PUT `/api/v1/network/credentials` with the same token carries bounded JSON
  `ssid` and `password`, returns 202 with `data.operation_id`, and the client polls
  GET `/api/v1/network/operations/wifi-{id}` until connected or failed.

The client validates a nonempty SSID of at most 32 UTF-8 bytes without nulls,
and an 8..63-character printable ASCII passphrase. Raw 64-hex PSKs and open
station networks are outside this first client contract. Passwords are not logged
or persisted in the browser, and the input is cleared after the request completes.
Queued is not Connected. The server persists credentials in ordinary plaintext
NVS before queuing driver work, rejects missing prerequisites, and exposes
bounded operation polling. No cookie credentials are sent on this Bearer-token
flow. The active endpoint is restricted to the explicitly
authorized local-only development AP; never send release credentials over its
plaintext HTTP. Browser API fixtures are test data.

These are the payloads the WebGUI validates. The development firmware implements
read-only `/api/v1/device/status` and `/api/v1/device/capabilities`; direct HTTP
GET requests returned 200 after flash. General device/admin endpoints and account
authentication are not implemented. The prepared Vue assets are served on AP and
STA origins; successful host GETs to the STA status, index, JS, and CSS routes
were observed. On STA, the UI shows the assigned IP and hides provisioning
controls. Scan/session/credential operations remain AP-only. Browser-to-device
camera status fetching and phone rendering have not been verified.

All paths are versioned `/api/v1`; successful responses use a `data` envelope.
The production device status/capability contract requires an authenticated
session. The current development endpoints are read-only, available only through
eligible local AP/STA addresses, and disabled when `APP_DEV_MODE` is off. They
contain no credentials or detected hardware claims. Release requires secure admin
transport, bounded requests and responses, CSRF protection for cookie-authenticated
writes and conservative CORS. The dev host is a different origin from the camera;
a future device must explicitly allow that exact origin with credentials, or use
a same-origin deployment/proxy. Never configure credentialed wildcard CORS.

## GET /api/v1/device/status

```json
{
  "data": {
    "firmware_version": "0.1.0",
    "uptime_seconds": 0,
    "free_heap_bytes": 0,
    "camera": null,
    "camera_probe_state": "not_run"
  }
}
```

Values above illustrate the schema, not measured device values. When a persistent
camera stream is active, `camera` contains numeric `width`, `height`, `fps`
collected from the pipeline. The one-frame boot probe stops capture before
network startup, so it leaves `camera: null`.

`camera_probe_state` is one of `not_run`, `no_supported_sensor`,
`sensor_identified`, `frame_captured`, `capture_failed` or `error`. In the
development profile, the probe initializes the supported OV5647 path over the
shared Platform I2C bus and attempts to capture one MIPI-CSI frame. A successful
frame verifies the sensor-to-CSI path for that boot, but does not enable the
application stream or mark the board profile verified. `no_supported_sensor`
means no sensor supported by the compiled probe was found; an unpowered module,
unsupported sensor or wiring fault can produce the same result.

`data.camera_hardware` is an optional, nullable identification object, independent
of the active `camera` stream. Its nullable string fields are `sensor` (1..32
characters), `module` (1..64) and `module_revision` (1..16). Existing status
responses without this object remain valid and display Unknown detection.
Firmware must populate it only from actual identification evidence; it must not
echo the local manifest's candidate sensor as a detected sensor.
The current probe can identify OV5647; it does not identify the UC-439 module
revision or detect IMX219 because no compatible driver is resolved for that
sensor.

The Camera view displays `camera_hardware` as hardware detection,
`camera_probe_state` as the one-frame probe result, and `camera` as the active
capture state/profile. A missing or null hardware identity is shown as Unknown,
not as proof that no camera is physically attached. A null `camera` means no
persistent capture profile is active; it is independent of whether the probe
identified a sensor. Connect and Refresh reload the device status.

The manifest records user-reported module `UC-439`, revision `B`, and possible
sensor identities `IMX219` / `OV5647`. The development probe has confirmed
OV5647 PID `0x5647` and one `800x800` CSI frame. Diagnostics continues to
separate the reported module/revision from the API-detected sensor. UC-439
revision B is NOT the Waveshare PCB revision; IMX219 and sustained capture remain
unverified.

## GET /api/v1/device/capabilities

`data` contains `board_id`, `target` (`esp32p4`) and `capabilities`. Values are
`unsupported`, `unverified`, `disabled`, `not_ready` or `ready`. Base feature
keys, including `camera`, come from the CapabilityRegistry. The current firmware
returns `camera=unverified`, `usb_uvc=unverified`, `ethernet=unverified`, and
`http_mjpeg=disabled` / `rtsp=disabled`. These are conservative hardware and
implementation states. The OV5647 ID and one CSI frame are reported separately
by `camera_probe_state`; the camera capability stays `unverified` because the
physical PCB revision and sustained stream are not verified.

## Media

  actual `http_mjpeg=ready`. The current client attempts the same device scheme
  on this port; the final deployment must confirm TLS/same-origin/session handling.

An HTTP image load failure clears the preview and reports Stream unavailable.
RTSP/JPEG must be validated against RFC 2435 and intended players before readiness.
Neither media service exists yet, and no addresses/credentials are embedded in source.

## Configuration (Next Integration)

GET/PATCH `/api/v1/camera/config`: shared profile, supported driver-confirmed
formats and sensor/encoder controls. PATCH Busy maps to 409 while USB streams;
an unverified sensor maps to 503. GET/PATCH `/api/v1/network/config` and
asynchronous scan/connect operations require plaintext NVS and audited C6
credential storage. Never return passwords or tokens in status/config GET responses.