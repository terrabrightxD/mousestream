# Architecture and verification

The parent KMDF filter saves and replaces the documented CONNECT_DATA callback.
Its service callback runs at DISPATCH_LEVEL. A child raw PDO exposes an
administrator/SYSTEM-only interface, allowing user-mode control without opening
the protected mouse stack. Requests are forwarded to a sequential, power-managed
parent queue. The raw PDO is exclusive and has FILE_DEVICE_SECURE_OPEN; its file
cleanup disables capture. A per-file closing flag rejects requests arriving after
cleanup. All IOCTLs use METHOD_BUFFERED and read/write access bits. No caller
addresses, kernel pointers, user callbacks, or executable buffers cross the ABI.

Each parent has a nonpaged, fixed-capacity ring of 256 packets and a spin lock.
The lock serializes physical callbacks, upper-class delivery, commands, watchdog,
and power teardown. No allocation, wait, or user-mode work occurs under it. Upper
class callbacks are invoked at DISPATCH_LEVEL. Driver Verifier must still check
this synchronization and its assumptions with the target mouse stack.

## Packet lifecycle

1. Without capture, physical packets pass to the original class callback.
2. With capture, originals enter the FIFO and count as consumed by the filter.
3. Receive peeks the head and returns its unique sequence and packet.
4. Resolve must name that head; forward delivers the edited packet, drop consumes
   it without delivery. Failed delivery leaves the original queued for recovery.
5. Timeout at 250 ms, overflow, stop, or cleanup disables capture and forwards
   originals in order. A 10 ms periodic timer retries recovery if Mouclass is full.
6. New physical input never overtakes a pending recovery packet. It is reported
   unconsumed to the lower stack while the upper stack cannot accept it.
7. Power-down attempts recovery, then invalidates pending input. Surprise removal
   marks the device unavailable and discards the queue. Resuming does not re-arm.

The watchdog is based on the age of the oldest **unresolved** packet, not polling
activity. Polling cannot extend a decision's lifetime. Packet sequences are not
reset on stop/re-arm, so stale decisions cannot resolve a new packet. Sequence
exhaustion stops enqueueing. This is a defensive bound, not a realistic workload.

Injection delivers one packet only when no intercepted packets are pending. It
uses the selected device's callback and reports backpressure. This version uses
unit ID zero for fresh injected packets; forwarded decisions preserve the source
unit ID. Multi-unit/aggregated devices therefore require further validation.

## IOCTL ABI v1

| Operation | Input | Output |
|---|---|---|
| GET_INFO | none | version, capture state, pending count, watchdog duration |
| START | none | none; busy if recovery packets remain |
| STOP | none | none; recovery may still be draining under backpressure |
| RECEIVE | none | 32-byte sequence + packet; empty maps to ERROR_NO_MORE_ITEMS |
| RESOLVE | 40-byte sequence/action/reserved/packet | none |
| INJECT | 24-byte packet | none |

All fixed-width fields use Windows little-endian native alignment. Version is
checked when the SDK opens a handle. `unit_id`, flags, buttons, wheel data, raw
buttons, X/Y, and extra information correspond to MOUSE_INPUT_DATA. There is no
arbitrary memory read/write IOCTL. Unknown operations and short buffers fail.

## Automated tests

`test_queue.c` executes the **same queue code linked into the driver**. It checks
ABI sizes, FIFO order, 50 ring wraps, overflow, watchdog boundary times, stale and
out-of-order decisions, sequence exhaustion, and invalid packet flags. Python
tests reject unsafe/generic INF IDs and accept concrete HID models. These tests
cannot verify WDF lifetime rules, IOCTL dispatch, PnP, or hardware delivery.

## Required Windows test matrix (not yet executed)

| Test | Expected result |
|---|---|
| Debug + Release WDK build | No warnings/errors |
| InfVerif + catalog/signature verification | Valid targeted package |
| No controlling application | Unchanged movement/buttons/wheel |
| Passthrough at 125/1000/8000 Hz | Ordered packets, no stuck button state |
| Left/right remap | Matching down/up transitions; other fields preserved |
| Explicit drop + edited movement | Only requested suppression/modification |
| End controlling process / close handle | Recovery restores normal input |
| Receive then stall >250 ms | Original delivered; late decision rejected |
| Flood >256 unresolved packets | Recovery without overtaking/duplicate delivery |
| Upper-class backpressure | Retry with no packet duplication |
| Short buffers, invalid IOCTL, bad flags, stale sequence | Defined errors, no crash |
| Non-admin process / second open | Access denied / exclusive-open failure |
| Sleep/resume, hibernate, unplug during every operation | No stale access; capture off |
| Multiple mice, other filters, rapid reconnect | Isolated queues and correct ordering |
| Close while commands are queued | No re-arming by a closed file |
| Absolute/virtual desktop, signed wheel values | Values and flags preserved |
| Driver Verifier: KMDF, I/O, pool, IRQL, deadlock checks | No violations |
| Uninstall/reboot and recovery path | Inbox input restored |

Known limitations include polling latency, no precedence chain, no keyboard API,
no persisted device identity, no automatic repair of user-chosen button remaps,
and no demonstrated HVCI/HLK compatibility. Passing portable tests alone is not a
claim of Windows compatibility or production readiness.
