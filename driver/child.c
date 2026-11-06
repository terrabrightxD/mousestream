/* SPDX-License-Identifier: MIT */
#include "driver.h"
VOID MsChildControl(WDFQUEUE queue, WDFREQUEST request, size_t output, size_t input, ULONG code) {
    MS_CHILD *child = MsChildGet(WdfIoQueueGetDevice(queue));
    WDF_REQUEST_FORWARD_OPTIONS options;
    NTSTATUS status;
    UNREFERENCED_PARAMETER(output); UNREFERENCED_PARAMETER(input);
    UNREFERENCED_PARAMETER(code);
    WDF_REQUEST_FORWARD_OPTIONS_INIT(&options);
    status = WdfRequestForwardToParentDeviceIoQueue(request, child->parent_queue, &options);
    if (!NT_SUCCESS(status)) WdfRequestComplete(request, status);
}
VOID MsCleanup(WDFFILEOBJECT file) {
    MS_CHILD *child = MsChildGet(WdfFileObjectGetDevice(file));
    MS_DEVICE *d = MsGet(child->parent);
    WdfSpinLockAcquire(d->lock);
    MsFileGet(file)->closing = TRUE;
    MsRecoverLocked(d);
    WdfSpinLockRelease(d->lock);
}
NTSTATUS MsCreateChild(WDFDEVICE parent) {
    PWDFDEVICE_INIT init = WdfPdoInitAllocate(parent);
    WDFDEVICE child = NULL;
    WDF_OBJECT_ATTRIBUTES attrs;
    WDF_IO_QUEUE_CONFIG queue;
    WDF_FILEOBJECT_CONFIG files;
    WDF_DEVICE_PNP_CAPABILITIES caps;
    NTSTATUS status;
    DECLARE_CONST_UNICODE_STRING(id, L"MouseStream\\Control");
    DECLARE_CONST_UNICODE_STRING(instance, L"0");
    DECLARE_CONST_UNICODE_STRING(text, L"MouseStream mouse control");
    DECLARE_CONST_UNICODE_STRING(location, L"Mouse filter sideband");
    if (!init) return STATUS_INSUFFICIENT_RESOURCES;
    status = WdfPdoInitAssignRawDevice(init, &MS_CLASS);
    if (!NT_SUCCESS(status)) goto fail;
    status = WdfDeviceInitAssignSDDLString(init, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
    if (!NT_SUCCESS(status)) goto fail;
    status = WdfPdoInitAssignDeviceID(init, &id);
    if (!NT_SUCCESS(status)) goto fail;
    status = WdfPdoInitAssignInstanceID(init, &instance);
    if (!NT_SUCCESS(status)) goto fail;
    status = WdfPdoInitAddDeviceText(init, &text, &location, 0x409);
    if (!NT_SUCCESS(status)) goto fail;
    WdfPdoInitSetDefaultLocale(init, 0x409);
    WdfDeviceInitSetExclusive(init, TRUE);
    WdfDeviceInitSetCharacteristics(init, FILE_DEVICE_SECURE_OPEN, FALSE);
    WdfPdoInitAllowForwardingRequestToParent(init);
    WDF_FILEOBJECT_CONFIG_INIT(&files, WDF_NO_EVENT_CALLBACK, WDF_NO_EVENT_CALLBACK, MsCleanup);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, MS_FILE);
    WdfDeviceInitSetFileObjectConfig(init, &files, &attrs);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, MS_CHILD);
    attrs.SynchronizationScope = WdfSynchronizationScopeNone;
    status = WdfDeviceCreate(&init, &attrs, &child);
    if (!NT_SUCCESS(status)) goto fail;
    MsChildGet(child)->parent = parent;
    MsChildGet(child)->parent_queue = MsGet(parent)->control_queue;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue, WdfIoQueueDispatchSequential);
    queue.EvtIoDeviceControl = MsChildControl;
    status = WdfIoQueueCreate(child, &queue, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) goto fail;
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&caps);
    caps.Removable = WdfTrue; caps.SurpriseRemovalOK = WdfTrue;
    /* Instance id is scoped by the unique parent device, not globally. */
    caps.UniqueID = WdfFalse;
    WdfDeviceSetPnpCapabilities(child, &caps);
    status = WdfDeviceCreateDeviceInterface(child, &MS_INTERFACE, NULL);
    if (!NT_SUCCESS(status)) goto fail;
    status = WdfFdoAddStaticChild(parent, child);
    if (NT_SUCCESS(status)) return status;
fail:
    if (init) WdfDeviceInitFree(init);
    if (child) WdfObjectDelete(child);
    return status;
}
