# MouseStream

An original, experimental Windows KMDF mouse filter and C SDK inspired by the
**intercept → receive → modify/block → forward** workflow of
[Interception](https://github.com/oblitum/Interception).

**Status: development source, not a ready-to-install driver release.** Portable
queue tests are provided; Windows kernel compilation, INF validation, Driver
Verifier, hardware tests, and release signing are separate gates. A successful
SDK workflow does not mean the kernel driver has been validated. There is no
signed `.sys` release and no claim of drop-in Interception compatibility.

## Included

- Kernel filter hooks the documented mouse connect callback, below Mouclass.
- Per-mouse discovery and one exclusive administrator/SYSTEM control handle.
- Receive complete packets: movement, five buttons, vertical/horizontal wheel,
  absolute/relative flags, and extra information.
- Forward a packet unchanged, edit it, or explicitly drop it.
- Send a synthetic packet through the attached mouse class callback.
- Bounded FIFO, sequence-checked decisions, and a 250 ms oldest-packet watchdog.
- Recovery forwards pending **original** packets on timeout, overflow, stop, or
  handle cleanup, subject to upper-stack capacity. No background service required.
- C DLL, pass-through/button-swap example, portable tests, SDK CI, WDK project,
  targeted extension-INF generator, and installation/removal instructions.

## Compatibility

| Area | MouseStream |
|---|---|
| Platform target | Windows 10 1903+ / Windows 11, x64; unverified on hardware |
| Initial device scope | HID mice using MouHid and Mouclass; exact hardware-ID installation |
| Interception DLL/ABI | Different API and protocol; existing apps must be adapted |
| Keyboard support | Not implemented |
| Multiple clients / precedence | Not implemented; one controller per mouse |
| Hardware-ID API | Control interface paths are enumerated; no Interception-style hardware-ID API |
| Filtering | Capture all packets; the application selects which to modify/drop |
| Wait mechanism | SDK polls at 1 ms requested intervals; no real-time latency guarantee |
| Driver signing | No Microsoft release signature supplied |

## Build the SDK and tests

Use a Visual Studio C++ developer shell on Windows with CMake installed:

```powershell
cmake -S . -B build/sdk -A x64
cmake --build build/sdk --config Release
ctest --test-dir build/sdk -C Release --output-on-failure
python -m unittest discover -s tests -p "test_*.py"
```

`mousestream.dll`, its import library, and `passthrough.exe` are written to
`build/sdk/Release`. These are user-mode components and require the separately
built, signed, installed driver. On Linux, omit `-A x64`; only portable tests build.

For the driver, use compatible Visual Studio, Windows SDK and WDK versions:

```powershell
msbuild driver/MouseStream.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

See [build, signing, installation and removal](docs/windows.md) before using the
result. The WDK workflow is manual and needs a configured Windows `wdk` runner;
it does not run on ordinary hosted SDK builds.

## API example

```c
wchar_t path[MS_PATH_CHARS];
if (!ms_device_path(0, path, MS_PATH_CHARS)) return 1;
HANDLE mouse = ms_open(path);
if (mouse == INVALID_HANDLE_VALUE) return 1;
if (!ms_start(mouse)) { ms_close(mouse); return 1; }
ms_event event;
while (ms_receive(mouse, &event, 1000) == 1) {
    /* Edit event.packet here, or pass MS_DROP to suppress this packet. */
    if (!ms_resolve(mouse, &event, MS_FORWARD)) break;
}
ms_close(mouse); /* cleanup disables capture and recovers queued originals */
```

The example above exits on receive timeout; [passthrough.c](examples/passthrough.c)
contains a continuing loop, Ctrl+C handling, and backpressure retries.
Run `passthrough --list`, then `passthrough 0` from an elevated console. Use
`passthrough 0 --swap-buttons` for a left/right button remapping demonstration.
Indices are enumeration positions and may change after reconnecting devices.

## Semantics and limitations

Receive peeks the oldest pending packet; only a successful resolve removes it.
Resolve forwards/drops exactly that sequence. After recovery, late decisions fail
rather than duplicating already-forwarded input. Close, timeout, overflow, and
stop disable capture; an application must explicitly re-arm it. Injection fails
while intercepted packets are pending, preserving their order. Calls on a given
handle must be serialized by the application.

Recovery cannot undo already accepted remaps or drops. Applications that change
button-down events must also handle the matching button-up events. Switching a
remap while a button is held can leave logical button state inconsistent. Pending
input that cannot be delivered before power-down is discarded; unplugging a mouse
also discards its pending queue. Test button state during these transitions.

This is not a security isolation boundary against administrators. There is no
stealth, application impersonation, or guaranteed compatibility with games or
other software that restricts input filters.

Read [architecture and validation](docs/architecture.md) for protocol details,
known risks, and the required hardware test matrix.

## License and provenance

MIT for this repository's original implementation. No Interception source,
installer, driver binary, or proprietary driver code is bundled. Interception's
public API and Microsoft's documented KMDF mouse/sideband patterns informed the
design; their respective rights remain with their authors. See [references](docs/references.md).
