# Design references

- [Interception public repository](https://github.com/oblitum/Interception): workflow and public API reference. Its public source does not include the proprietary kernel driver implementation.
- [Microsoft Moufiltr sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/mouse-input-wdf-filter-driver-moufiltr/): documented mouse filter architecture.
- [IOCTL_INTERNAL_MOUSE_CONNECT](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/kbdmou/ni-kbdmou-ioctl_internal_mouse_connect): callback connection contract.
- [Microsoft Kbfiltr raw PDO sample](https://github.com/microsoft/Windows-driver-samples/blob/main/input/kbfiltr/sys/rawpdo.c): sideband communication pattern.
- [INF AddFilter](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/inf-addfilter-directive): declarative filter registration.
- [Filter ordering](https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/device-filter-driver-ordering): placement constraints with other filters.
- [WDK setup](https://learn.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk): build environment.
- [Test signing](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/test-signing-driver-packages): driver package signing.

Original implementation; these links do not imply endorsement or certification.
