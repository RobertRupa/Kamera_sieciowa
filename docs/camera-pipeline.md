# Camera pipeline contract

## Current Implementation

`FrameBroker` is an application-level, host-testable queue service. It owns only
metadata; its caller owns contiguous external storage. At construction, storage
must hold ten slots of `maxFrameBytes` each, without arithmetic overflow.
For an eventual 256 KiB per-frame cap this is 2.5 MiB of storage; this is a
proposed budget, not a verified DMA/PSRAM allocation or JPEG quality guarantee.
The caller must keep both the storage and broker alive until all leases and
producer/consumer tasks are stopped. Never destroy the broker while a lease exists.

One producer calls `publish()` with a complete, borrowed JPEG buffer and a
monotonic microsecond timestamp. The buffer must be readable for its declared
length and remain unchanged until `publish()` returns. Publication copies once
into a reserved slot outside the metadata lock. The producer may then return the
capture/encoder buffer to its upstream driver. The method checks only the JPEG
SOI/EOI envelope and byte budget, not the complete JPEG syntax or RFC 2435 format.
Actual encoder output and packetizer validation are separate integration gates.

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

## Pending Adapter Boundary

The intended flow is `esp_video capture -> JPEG encoder -> FrameBroker -> UVC /
HTTP multipart / RTSP RTP-JPEG`. No sink may open the camera independently.
Do not add stub adapters reporting Ready. The current composition root deliberately
does not allocate frame storage or enable outputs because no real capture exists.

Reuse the official esp_video UVC and simple_video_server examples for verified
APIs, not their unrelated board pin maps or plaintext credential examples.
Evaluate Micro-RTSP's POSIX core without Arduino and pin a reviewed commit only
after compilation, JPEG/RFC 2435, TCP/UDP transport and authentication tests.
No packetizer/server or security readiness has been claimed here.