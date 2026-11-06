# Windows development and installation

## Build prerequisites

Use a dedicated Windows test VM with USB mouse passthrough or a spare Windows PC.
Keep a separate keyboard and recovery console available. This development driver
is not validated for a daily-use machine. A bad filter can stop mouse input or
crash Windows; the application watchdog cannot recover a kernel bug.

Install Visual Studio's C++ workload and a compatible SDK/WDK pair using
[Microsoft's WDK instructions](https://learn.microsoft.com/windows-hardware/drivers/download-the-wdk).
The `.vcxproj` requires the `WindowsKernelModeDriver10.0` toolset and targets
KMDF 1.29. Use matching SDK and WDK build numbers. In a developer shell:

```powershell
msbuild driver/MouseStream.vcxproj /m /p:Configuration=Debug /p:Platform=x64
msbuild driver/MouseStream.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

Do not infer build success from the existence of this project file. Record the
compiler version and build log. Output is under `build/driver/x64/<configuration>`.

## Select the mouse and stage a package

In Device Manager, select the intended **Mouse-class HID mouse**, not its USB
parent or a keyboard collection. Under Details, copy a specific Hardware Id such
as `HID\VID_046D&PID_C077`. Verify that its function service is `mouhid` and its
stack uses `mouclass`. The INF targets all devices matching that hardware ID;
it is not restricted to one physical instance. Prefer a unique test mouse model.
The generator rejects generic class IDs, wildcards, and non-HID IDs.

```powershell
python scripts/package.py --hardware-id 'HID\VID_046D&PID_C077' `
  --sys build/driver/x64/Release/MouseStream.sys --out build/package
```

The example ID is illustrative: substitute the real ID of your test mouse.
The generated INF is a device extension and leaves the inbox function driver in
place. It registers an upper device filter. Confirm in WinDbg that MouseStream
is between Mouclass and MouHid and receives `IOCTL_INTERNAL_MOUSE_CONNECT`.
Other filters may change ordering; do not assume a successful installation alone
proves interception works. This package does not modify class-wide UpperFilters.

## Validate and sign

Use the WDK tools on the generated directory:

```powershell
InfVerif.exe /w build/package/MouseStream.inf
Inf2Cat.exe /driver:build/package /os:10_X64
```

Use the OS identifier appropriate to your installed WDK and actual test target
(`Inf2Cat /?` lists supported identifiers). Resolve validation errors before
installation. Sign the SYS first if embedding a signature, then generate and sign
the catalog so the hashes match. Follow Microsoft's
[test-signing process](https://learn.microsoft.com/windows-hardware/drivers/install/test-signing-driver-packages)
on an isolated test system. Keep certificates/private keys outside Git and CI
artifacts. The project does not create certificates or change boot/security policy.
Production distribution requires an appropriate Microsoft-signed package; renaming
an unsigned SYS or borrowing another driver's signature does not provide this.

## Install the validated, signed package

From an elevated console on the test machine:

```powershell
pnputil /add-driver build/package/MouseStream.inf /install
```

Record the resulting published name (`oemNN.inf`) for **this package**. Reboot or
restart the target device when Windows requires it. Inspect Device Manager for
errors, confirm the stack order, then build/run the SDK's pass-through example.
Do not install an unsigned/unvalidated output just because SDK CI passed.

## Remove / recover

Close controlling applications. In an elevated console, remove only the recorded
MouseStream extension package:

```powershell
pnputil /delete-driver oemNN.inf /uninstall
```

Replace `oemNN.inf` with the actual MouseStream package, then reboot if requested.
Do not delete the SYS first or remove `mouclass`, `mouhid`, or unrelated OEM
packages. If mouse input is unavailable, use the keyboard or recovery console.
For a machine that cannot boot, use Windows Recovery Environment, identify the
correct offline Windows volume, and remove the recorded MouseStream OEM package
using DISM's offline `/Remove-Driver` operation. Restore the test VM snapshot if
package identity or recovery state is uncertain.

## Release gate

No signed driver release should be created until both build configurations,
InfVerif, package signing verification, the test matrix, and Driver Verifier pass.
A manually configured `wdk` runner can execute the driver workflow. Do not allow
untrusted pull-request code to execute on a privileged test machine.
