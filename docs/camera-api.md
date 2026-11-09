# Camera API Contract (Planned)

## Implemented Provisioning Subset (Local Development Profile)

`ProvisioningApi` now implements POST `/api/v1/provisioning/session`, PUT
`/api/v1/network/credentials` and GET `/api/v1/network/operations/wifi-{id}`.
The server is conditional and blocked by default project settings. Other device/admin
endpoints and frontend asset serving described below remain planned, not running.
The previous descriptions of these three routes as unimplemented are historical.

The current HTTP service is an explicitly authorized local-development open AP
profile only, not a release onboarding design. AP startup requires verified
APP_WIFI_WIRING_VERIFIED and APP_DEV_OPEN_SETUP_AP. Credential sessions additionally
require a ready plaintext NVS store and APP_C6_STORAGE_AUDITED. The matching
ESP-Hosted C6 must receive `nvs_enable=0`; no HMAC key is used. Do not enable
hardware audit flags merely to bypass checks.

Only IPv4 sockets addressed to the actual AP address are eligible. Origin, if
present, must be that numeric HTTP origin; cross-origin Vite and mDNS-name origins
are not currently allowed. There is no permissive CORS or unauthenticated LAN
configuration endpoint. A session is bound to its peer IP, lasts 180 seconds,
uses a random 64-character token and is issued at most once per five seconds.
This is not an admin session. A valid token authorizes writes and operation reads.

JSON bodies require Content-Type application/json, length 1..384 bytes, exactly
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

HTTP runtime/auth/fault tests and actual C6 station behavior are NOT RUN.
Scan, reconnect/backoff, restore-after-reboot, frontend polling/session reuse,
captive DNS and asset serving still require integration before a working device
wizard or release can be claimed.

## Provisioning Client Contract

The Wi-Fi panel now implements this client flow, gated by ready Wi-Fi,
wifi_wizard and credential_storage capabilities:

1. POST `/api/v1/provisioning/session` returns `data.token` (32..256 characters)
  and `data.expires_in_seconds` (1..300). The future server must require a
  physical/bounded provisioning context, rate-limit requests and restrict origins.
2. PUT `/api/v1/network/credentials` with the scoped Bearer token carries bounded
  JSON `ssid` and `password`, and returns 202 with `data.operation_id`.

The client validates a nonempty SSID of at most 32 UTF-8 bytes without nulls,
and an 8..63-character printable ASCII passphrase. Raw 64-hex PSKs and open
station networks are outside this first client contract. Passwords are not logged
or persisted in the browser, and the input is cleared after the request completes.
Queued is not Connected. The server persists credentials in ordinary plaintext
NVS before queuing driver work, rejects missing prerequisites, and exposes
bounded operation polling. Scan, operation polling in the frontend and general
device HTTP handlers are not implemented yet. No cookie credentials are sent on
this Bearer-token flow. The active endpoint is restricted to the explicitly
authorized local-only development AP; never send release credentials over its
plaintext HTTP. Browser API fixtures are test data.

These are the payloads the WebGUI foundation validates. **General device/admin
endpoints and account authentication are not implemented.** The limited
provisioning session API is present, but no live HTTP client request has been tested.
The panel cannot yet configure or authenticate a camera. Do not mistake the
frontend schema or client code for a running API.

All paths are versioned `/api/v1`; successful responses use a `data` envelope.
The richer device/status/capability endpoints require an authenticated session.
Release requires secure admin transport, bounded requests and responses, CSRF
protection for cookie-authenticated writes and conservative CORS. The dev host
is a different origin from the camera; a future device must explicitly allow
that exact origin with credentials, or use a same-origin deployment/proxy.
Never configure credentialed wildcard CORS.

## GET /api/v1/device/status

```json
{
  "data": {
    "firmware_version": "0.1.0",
    "uptime_seconds": 0,
    "free_heap_bytes": 0,
    "camera": null
  }
}
```

Values above illustrate the schema, not measured device values. When a verified
camera is active, `camera` contains numeric `width`, `height`, `fps` collected
from the pipeline. Use null when unavailable, not an invented video profile.

`data.camera_hardware` is an optional, nullable identification object, independent
of the active `camera` stream. Its nullable string fields are `sensor` (1..32
characters), `module` (1..64) and `module_revision` (1..16). Existing status
responses without this object remain valid and display Unknown detection.
Firmware must populate it only from actual identification evidence; it must not
echo the local manifest's candidate sensor as a detected sensor.

The manifest now records user-reported module `UC-439`, revision `B`, and possible
sensor identities `IMX219` / `OV5647`. Diagnostics displays these reported facts
even offline, separately from API-reported detected module/sensor. This module
revision is NOT the Waveshare board revision. The actual sensor and driver
compatibility remain unverified; no hardware detection was added in this change.

## GET /api/v1/device/capabilities

`data` contains `board_id`, `target` (esp32p4) and `capabilities`. Values are
`unsupported`, `unverified`, `disabled`, `not_ready` or `ready`. Base feature keys
come from the existing CapabilityRegistry. The future camera application will
add `usb_uvc`, `http_mjpeg`, `rtsp` and `ethernet` based on actual adapter states,
not GPIO guesses or compiled-in booleans. The panel treats absent keys as Unknown.

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