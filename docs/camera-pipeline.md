# Camera pipeline contract

## Current Implementation

`FrameBroker` is an application-level, host-testable queue service. It owns only
metadata; its caller owns contiguous external storage. The current preview
allocates ten 1 MiB slots (10 MiB total) in PSRAM, without arithmetic overflow.
This bounds JPEG frame storage but does not guarantee every possible image fits;
over-budget output is rejected. The caller keeps storage and broker alive until
all leases and producer/consumer tasks are stopped. Never destroy the broker
while a lease exists.

One producer calls `publish()` with a complete, borrowed JPEG buffer and a
monotonic microsecond timestamp. The buffer must be readable for its declared
length and remain unchanged until `publish()` returns. Publication copies once
into a reserved slot outside the metadata lock. The producer may then return the
capture/encoder buffer to its upstream driver. The method checks only the JPEG
SOI/EOI envelope and byte budget, not the complete JPEG syntax or RFC 2435 format.
The current device uses the P4 hardware JPEG encoder for local multipart MJPEG;
RTSP/RTP packetizer validation is a separate integration gate.

Three outputs (`Usb`, `Http`, `Rtsp`) each have an independently enabled FIFO of
depth two. Full queues discard their own oldest reference. Disabling an output
clears its queued references but does not invalidate an already borrowed frame.
Publishing with no subscribers releases the temporary slot and returns Published.
Pool exhaustion returns a typed error instead of allocating extra memory or waiting.

`tryTake()` uses a nonblocking mutex attempt; an empty lease can mean no frame or
brief contention. A consumer should retry on its next task iteration or frame
interval, not busy-loop inside a USB callback. `FrameLease` is move-only and keeps
its immutable bytes alive until reset/destruction. A consumer must retain the
lease until the transport has finished reading those bytes, not merely queued a
pointer. USB callback ownership must be adapted to the actual UVC return callback.

No copying, transport I/O or user callbacks occur under the metadata lock.
`setEnabled`, `stats`, lease release and producer metadata updates take a short
mutex lock; they are task-context methods, NOT ISR-safe or hard real-time. Release
leases in the USB worker if the underlying callback context cannot take a mutex.
At integration, cap in-flight leases to one per output. Multiple producers are
not part of the contract; ordered timestamps depend on one capture task.

## Shared Video Profile

`ProfilePolicy` borrows an immutable, driver-confirmed list of up to 32 formats
for its entire lifetime. An empty or unknown list returns Unverified, not a
default supported resolution. Validation rejects zero dimensions/FPS and JPEG
quality outside 1..100. Only listed width/height/FPS combinations are accepted.
The integration must provide profiles whose JPEG quality control is supported by
the actual encoder; quality is not a claim about sensor control support.

During active USB, a changed profile, including quality, returns Busy. Idempotent
requests are accepted. USB negotiation must match the shared profile; it cannot
silently create a separate capture configuration. Map Busy to HTTP 409 and
Unverified to HTTP 503. The future controller must serialize configuration and
USB lifecycle changes in one owning task to avoid check-then-update races.

## Raspberry Pi CSI Module Compatibility

The supported sensor drivers and hardware-verification status are tracked in
the [Raspberry Pi CSI camera compatibility matrix](camera-compatibility.md).
An upstream sensor driver being compiled does not mean the attached module or
the board's CSI wiring has been validated.

## Active Preview and Pending Outputs

The verified OV5647 preview flow is `esp_video MIPI-CSI/ISP capture (RGB565) ->
P4 hardware JPEG encoder -> FrameBroker -> HTTP multipart MJPEG`. The development
boot probe captures one frame and deinitializes; after the setup AP is ready, the
application starts a persistent 800x800 capture. Encoding/publication runs only
while an HTTP or network subscriber is connected and is capped at 20 FPS. The stream is on
port 81 at `/stream.mjpg`; the status API and embedded WebGUI remain on port 80.
The HTTP consumer holds a `FrameLease` until all response chunks for that JPEG
have been sent. No sink may open the camera independently.

HTTP MJPEG is reported Ready only when persistent capture and the stream server
are active and a capture frame has arrived. `camera` capability remains
Unverified until board-level camera wiring and sustained-operation criteria are
verified. USB UVC remains unimplemented; do not report it Ready or add stub adapters.

Reuse the official esp_video UVC and simple_video_server examples for verified
APIs, not their unrelated board pin maps or plaintext credential examples.
`CameraNetworkStream` consumes the broker's independently bounded `Rtsp` queue
for RTSP-negotiated RTP/UDP and direct RTP/SDP. Managed `espp/rtsp` 1.4.2 supplies
the RTSP engine, JPEG parser and RTP serializer. The integration removes optional
APP/COM metadata from a bounded 4 KiB header before parsing, sends only JPEG scan
bytes, and preserves dynamic quantization tables. It does not modify managed
component sources. Micro-RTSP was rejected because of unbounded parser reads.
The network worker has a 12 KiB stack, a 256 KiB encoded-frame cap, 1000-byte scan
fragments, nonblocking direct UDP sends and a fixed platform limit of 16 sockets.
Oversized frames are counted, not allocated beyond the cap. No RTP consumer opens
the sensor. Capture/HTTP continue independently when an RTP frame is dropped.

The network queue uses atomic `tryTakeLatest()` rather than replaying its FIFO:
older references are released and counted without changing the HTTP queue or
invalidating leases. Frames older than 100 ms at dequeue are dropped. Direct UDP
yields and paces 500 microseconds every four packets, not a full RTOS tick;
socket backpressure still drops the remainder of that frame rather than retrying
old data. Timing counters describe sender work only. This does not alter the
upstream RTSP library's own per-session packet pacing or player jitter buffers.

Sensor configuration is a single queued command owned by the capture task.
Read-only snapshots and requests use a short mutex; driver work and network I/O
run outside it. A mode change stops CSI, releases/makes V4L2 buffers, and requires
a first frame before Applied; failure attempts to restore the previous mode.
Mode enumeration and resource validation are separate from physical board readiness.
All five upstream OV5647 formats are enabled, including RAW10 and portrait.
Capture buffers allow at most 5 MiB each and dimensions up to 1920 per axis;
the 32 MiB PSRAM profile is required. The default 800x800 mode is preserved.