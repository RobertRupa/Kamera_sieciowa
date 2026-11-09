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
returned a valid quoted `station_ip`. The embedded GUI's camera status and live
preview were also verified on STA. The final AP scan path after increasing the
main-task stack is not yet verified from hardware. Captive DNS is NOT IMPLEMENTED.

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
controls. Scan/session/credential operations remain AP-only. The embedded
camera page was verified to decode the device's 800x800 stream; phone rendering
has not been verified.

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
    "camera": { "width": 800, "height": 800, "fps": 20 },
    "camera_probe_state": "frame_captured",
    "camera_hardware": {
      "sensor": "OV5647",
      "module": null,
      "module_revision": null
    }
  }
}
```

The camera values above match the currently tested device profile; uptime and
heap values are illustrative. When persistent capture is running and has received
a frame, `camera` contains its configured `width`, `height`, and capped `fps`.
The one-frame probe itself stops and deinitializes capture. After the setup AP
starts, a successful probe allows the application to initialize persistent
capture; failure leaves `camera: null`.

`camera_probe_state` is one of `not_run`, `no_supported_sensor`,
`sensor_identified`, `frame_captured`, `capture_failed` or `error`. In the
development profile, the probe initializes the supported OV5647 path over the
shared Platform I2C bus and attempts to capture one MIPI-CSI frame. A successful
frame verifies the sensor-to-CSI path for that boot. The application then starts
the persistent preview only after network initialization; this does not mark the
board profile verified. `no_supported_sensor`
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
OV5647 PID `0x5647` and one `800x800` CSI frame; a live 800x800 MJPEG frame has
also been verified. Diagnostics continues to separate the reported
module/revision from the API-detected sensor. UC-439 revision B is NOT the
Waveshare PCB revision; long-duration endurance remains unverified.

## GET /api/v1/device/capabilities

`data` contains `board_id`, `target` (`esp32p4`) and `capabilities`. Values are
`unsupported`, `unverified`, `disabled`, `not_ready` or `ready`. Base feature
keys, including `camera`, come from the CapabilityRegistry. The current firmware
returns `camera=unverified`, `usb_uvc=unverified`, `ethernet=unverified`,
`http_mjpeg=ready` while persistent capture and the MJPEG server are active, and
`rtsp=ready` only while the network stream service and capture are active. The camera capability stays `unverified` because the physical
PCB revision and sustained-operation requirements are not verified. The OV5647
sensor, one-frame probe result, and active profile are reported separately.

## Media

The embedded WebGUI opens `http://<device-address>:81/stream.mjpg` as a
`multipart/x-mixed-replace` image stream. The camera view displays the active
capture profile and reports an image-load failure as Stream unavailable. Port 80
serves the GUI and read-only status API; port 81 serves media. The development
server is local-only and unauthenticated, not a release transport. No device
address or credentials are embedded in source. RTSP negotiates JPEG RTP/UDP on
`rtsp://<device-address>:8554/cameras/csi-0`. Direct RTP/UDP uses the configured
even receiver port and the SDP download below. Both use payload type 26 and a
90 kHz clock; UDP frames above 256 KiB are dropped and counted. USB UVC remains
unimplemented. Media endpoints are unauthenticated local-development services,
not an authenticated or encrypted release transport.

## Camera Inventory and Configuration

GET `/api/v1/cameras` returns a bounded `data.cameras` array of detected inputs,
their IDs, sensors, capture states, and actual MJPEG/RTSP paths and ports. The
current board has one CSI input (`csi-0`); its other ribbon connector is DSI for
display output. No second camera or simultaneous multi-CSI capture is claimed.

GET `/api/v1/cameras/csi-0` returns `settings`, `modes`, `state`, and
`persistent:false`. Modes are enumerated with `VIDIOC_ENUM_SENSOR_FMT`, not
guessed from a product name. Each mode exposes its driver ID/name, width,
height, sensor FPS, and `available` after the application buffer budget check.
All five modes provided by the resolved OV5647 driver are compiled and selectable:
RAW8 800x1280/50, 800x640/50, 800x800/50, RAW10 1920x1080/30, and binned
1280x960/45 sensor FPS. Default remains 800x800. RGB565 capture buffers are capped
at 5 MiB each; JPEG broker storage remains ten 1 MiB slots in PSRAM. All five
modes passed short capture/decode tests on the current unit. This is the complete
resolved-driver inventory, not a claim that every sensor datasheet mode or
third-party module is supported. A change still requires a frame confirmation.

PATCH `/api/v1/cameras/csi-0/config` requires a camera-session Bearer token and
exactly `mode_id` (integer 0..31), `preview_fps` (1..20, no greater than the sensor
rate), and `jpeg_quality` (1..100). Invalid or over-budget settings return 400;
an operation already pending returns 409. A successful request returns 202.
Poll the GET route for `queued`, `applying`, `applied`, or `failed`. The capture
task stops/reallocates buffers for a mode change; it attempts rollback if the
change fails or no first frame arrives within four seconds. Settings are not
saved to NVS and revert on reboot. Preview FPS is an encoding/publication cap,
not a sensor clock control. Exposure, gain, autofocus and module identification
are not fabricated or exposed as writable controls.

POST `/api/v1/camera/session` accepts a bounded JSON `username`/`password` and
issues a peer-IP-bound, 900-second token only when both `APP_DEV_MODE` and
`APP_DEV_DEFAULT_CREDENTIALS` are enabled. That explicit lab profile permits
`admin/admin`; release/default configuration does not. Login attempts are
limited to one every five seconds. The browser keeps the token in memory only.
DELETE on the same route requires the token, revokes it, and stops its direct UDP.

GET/PUT `/api/v1/cameras/csi-0/udp` reads/configures direct RTP/UDP. PUT requires
the token and exactly `enabled` (boolean) and `port` (even integer 1024..65534).
The receiver is the authenticated request's peer IP, not an arbitrary supplied
destination. Sending expires after 900 seconds or stops on logout/network
restart. Status reports `frames_sent`, `oversized_frames`, `send_errors`, and
`stale_frames`; optional timing fields are `last_frame_age_us`, `max_frame_age_us`,
`last_send_duration_us`, and `max_send_duration_us`. Age is measured from the
producer's encoding-start timestamp to RTP dequeue, not sensor exposure or
glass-to-glass latency. Send duration covers direct-UDP packetization/send calls,
not delivery to the player. The GUI refreshes active UDP diagnostics once per
second with at most one request in flight.
GET `/api/v1/cameras/csi-0/stream.sdp` downloads SDP only for the active receiver's
peer IP; open that file in VLC or with
`ffplay -protocol_whitelist file,udp,rtp csi-0.sdp`. The browser does not decode RTP.

## Lower-Latency Playback

RTP consumes the newest queued frame and discards frames older than 100 ms before
transmission. Direct UDP uses 500 microseconds of bounded pacing per four packets
instead of the previous one-tick pause (10 ms at the active 100 Hz tick). RTSP
also benefits from latest-frame selection but retains its upstream sender pacing.
No loss-free delivery or fixed end-to-end delay is promised.

For the lowest delay, start with direct RTP/SDP, 800x640 or 800x800, and JPEG
quality 40..60. Larger resolutions require more bandwidth; 1080p quality 70
exceeded the 256 KiB JPEG budget in the observed scene. Quality 40 decoded on this
unit but remains scene-dependent; start at 5..10 preview FPS. Lower quality,
resolution, or FPS if oversized/drop counters rise.

Reduce the player's own buffering as well: VLC network caching can start at
50..100 ms; smaller caches may increase visible loss on Wi-Fi. For ffplay:

```sh
ffplay -protocol_whitelist file,udp,rtp -fflags nobuffer -flags low_delay -framedrop -reorder_queue_size 0 csi-0.sdp
```

These are receiver tuning suggestions, not measured glass-to-glass latency.