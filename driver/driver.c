/* SPDX-License-Identifier: MIT */
#include "driver.h"
C_ASSERT(sizeof(ms_packet) == sizeof(MOUSE_INPUT_DATA));
C_ASSERT(FIELD_OFFSET(ms_packet, x) == FIELD_OFFSET(MOUSE_INPUT_DATA, LastX));
C_ASSERT(sizeof(ms_event) == 32);
C_ASSERT(sizeof(ms_decision) == 40);
#pragma warning(disable:4055 4152) /* CONNECT_DATA is a documented PVOID callback. */
static uint64_t MsNow(VOID) { return KeQueryInterruptTime() / 10000; }
/* Called under our spin lock at DISPATCH_LEVEL. Serializes physical and injected
   input with power teardown. The upper mouse class callback must not reenter us. */
static BOOLEAN MsDeliverLocked(MS_DEVICE *d, const ms_packet *p) {
    MOUSE_INPUT_DATA native;
    ULONG consumed = 0;
    if (!d->powered || !d->upper.ClassService) return FALSE;
    RtlCopyMemory(&native, p, sizeof(native));
    ((PSERVICE_CALLBACK_ROUTINE)d->upper.ClassService)(
        d->upper.ClassDeviceObject, &native, &native + 1, &consumed);
    return consumed == 1;
}
VOID MsRecoverLocked(MS_DEVICE *d) {
    const ms_slot *s;
    d->capturing = FALSE;
    while ((s = msq_front(&d->packets)) != NULL) {
        if (!MsDeliverLocked(d, &s->event.packet)) break;
        msq_pop(&d->packets, s->event.sequence);
    }
}
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry) {
    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, MsAdd);
    return WdfDriverCreate(driver, registry, WDF_NO_OBJECT_ATTRIBUTES, &config, WDF_NO_HANDLE);
}
NTSTATUS MsAdd(WDFDRIVER driver, PWDFDEVICE_INIT init) {
    WDFDEVICE device;
    WDF_OBJECT_ATTRIBUTES attrs;
    WDF_IO_QUEUE_CONFIG queue;
    WDF_PNPPOWER_EVENT_CALLBACKS power;
    WDF_TIMER_CONFIG timer;
    NTSTATUS status;
    MS_DEVICE *d;
    UNREFERENCED_PARAMETER(driver);
    WdfFdoInitSetFilter(init);
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&power);
    power.EvtDeviceD0Entry = MsD0Entry;
    power.EvtDeviceD0Exit = MsD0Exit;
    power.EvtDeviceSurpriseRemoval = MsSurprise;
    WdfDeviceInitSetPnpPowerEventCallbacks(init, &power);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attrs, MS_DEVICE);
    attrs.SynchronizationScope = WdfSynchronizationScopeNone;
    status = WdfDeviceCreate(&init, &attrs, &device);
    if (!NT_SUCCESS(status)) return status;
    d = MsGet(device);
    msq_init(&d->packets);
    WDF_OBJECT_ATTRIBUTES_INIT(&attrs); attrs.ParentObject = device;
    status = WdfSpinLockCreate(&attrs, &d->lock);
    if (!NT_SUCCESS(status)) return status;
    /* Parallel default dispatch avoids blocking internal mouse connect traffic. */
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queue, WdfIoQueueDispatchParallel);
    queue.EvtIoInternalDeviceControl = MsInternal;
    status = WdfIoQueueCreate(device, &queue, WDF_NO_OBJECT_ATTRIBUTES, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) return status;
    WDF_IO_QUEUE_CONFIG_INIT(&queue, WdfIoQueueDispatchSequential);
    queue.EvtIoDeviceControl = MsControl;
    queue.PowerManaged = WdfTrue;
    status = WdfIoQueueCreate(device, &queue, WDF_NO_OBJECT_ATTRIBUTES, &d->control_queue);
    if (!NT_SUCCESS(status)) return status;
    WDF_TIMER_CONFIG_INIT_PERIODIC(&timer, MsTick, 10);
    timer.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&attrs); attrs.ParentObject = device;
    status = WdfTimerCreate(&timer, &attrs, &d->timer);
    if (!NT_SUCCESS(status)) return status;
    return MsCreateChild(device);
}
NTSTATUS MsD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE previous) {
    MS_DEVICE *d = MsGet(device);
    UNREFERENCED_PARAMETER(previous);
    WdfSpinLockAcquire(d->lock); d->powered = TRUE; WdfSpinLockRelease(d->lock);
    WdfTimerStart(d->timer, WDF_REL_TIMEOUT_IN_MS(10));
    return STATUS_SUCCESS;
}
NTSTATUS MsD0Exit(WDFDEVICE device, WDF_POWER_DEVICE_STATE target) {
    MS_DEVICE *d = MsGet(device);
    UNREFERENCED_PARAMETER(target);
    WdfTimerStop(d->timer, TRUE);
    WdfSpinLockAcquire(d->lock);
    MsRecoverLocked(d);
    d->powered = FALSE;
    /* If Mouclass cannot accept pending input, it cannot survive power teardown. */
    msq_clear(&d->packets);
    WdfSpinLockRelease(d->lock);
    return STATUS_SUCCESS;
}
VOID MsSurprise(WDFDEVICE device) {
    MS_DEVICE *d = MsGet(device);
    WdfSpinLockAcquire(d->lock);
    d->powered = d->capturing = FALSE;
    msq_clear(&d->packets);
    WdfSpinLockRelease(d->lock);
}
VOID MsTick(WDFTIMER timer) {
    MS_DEVICE *d = MsGet((WDFDEVICE)WdfTimerGetParentObject(timer));
    WdfSpinLockAcquire(d->lock);
    if (d->powered && (!d->capturing || msq_expired(&d->packets, MsNow())))
        MsRecoverLocked(d);
    WdfSpinLockRelease(d->lock);
}
VOID MsConnectComplete(WDFREQUEST request, WDFIOTARGET target,
                       PWDF_REQUEST_COMPLETION_PARAMS params, WDFCONTEXT context) {
    MS_DEVICE *d = (MS_DEVICE *)context;
    UNREFERENCED_PARAMETER(target);
    if (!NT_SUCCESS(params->IoStatus.Status)) {
        WdfSpinLockAcquire(d->lock);
        RtlZeroMemory(&d->upper, sizeof(d->upper));
        WdfSpinLockRelease(d->lock);
    }
    WdfRequestCompleteWithInformation(request, params->IoStatus.Status, params->IoStatus.Information);
}
VOID MsInternal(WDFQUEUE queue, WDFREQUEST request, size_t output, size_t input, ULONG code) {
    WDFDEVICE device = WdfIoQueueGetDevice(queue);
    MS_DEVICE *d = MsGet(device);
    NTSTATUS status;
    PCONNECT_DATA connection;
    WDF_REQUEST_SEND_OPTIONS options;
    UNREFERENCED_PARAMETER(output); UNREFERENCED_PARAMETER(input);
    if (code == IOCTL_INTERNAL_MOUSE_DISCONNECT) {
        WdfRequestComplete(request, STATUS_NOT_IMPLEMENTED); return;
    }
    if (code == IOCTL_INTERNAL_MOUSE_CONNECT) {
        status = WdfRequestRetrieveInputBuffer(request, sizeof(*connection), (PVOID *)&connection, NULL);
        if (!NT_SUCCESS(status)) { WdfRequestComplete(request, status); return; }
        WdfSpinLockAcquire(d->lock);
        if (d->upper.ClassService || !connection->ClassService || !connection->ClassDeviceObject) {
            WdfSpinLockRelease(d->lock);
            WdfRequestComplete(request, STATUS_SHARING_VIOLATION); return;
        }
        d->upper = *connection;
        connection->ClassDeviceObject = WdfDeviceWdmGetDeviceObject(device);
        connection->ClassService = (PVOID)MsService;
        WdfSpinLockRelease(d->lock);
        WdfRequestFormatRequestUsingCurrentType(request);
        WdfRequestSetCompletionRoutine(request, MsConnectComplete, d);
        if (!WdfRequestSend(request, WdfDeviceGetIoTarget(device), NULL)) {
            status = WdfRequestGetStatus(request);
            WdfSpinLockAcquire(d->lock);
            *connection = d->upper;
            RtlZeroMemory(&d->upper, sizeof(d->upper));
            WdfSpinLockRelease(d->lock);
            WdfRequestComplete(request, status);
        }
        return;
    }
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    if (!WdfRequestSend(request, WdfDeviceGetIoTarget(device), &options))
        WdfRequestComplete(request, WdfRequestGetStatus(request));
}
VOID MsService(PDEVICE_OBJECT object, PMOUSE_INPUT_DATA start,
               PMOUSE_INPUT_DATA end, PULONG consumed) {
    MS_DEVICE *d = MsGet(WdfWdmDeviceGetWdfDeviceHandle(object));
    PMOUSE_INPUT_DATA p;
    ms_packet packet;
    *consumed = 0;
    WdfSpinLockAcquire(d->lock);
    if (!d->powered) { WdfSpinLockRelease(d->lock); return; }
    if (msq_expired(&d->packets, MsNow())) MsRecoverLocked(d);
    for (p = start; p < end; ++p) {
        RtlCopyMemory(&packet, p, sizeof(packet));
        if (d->capturing && msq_push(&d->packets, &packet, MsNow())) {
            ++*consumed; continue;
        }
        MsRecoverLocked(d);
        /* No overtaking: let the lower driver retry if upper queue is full. */
        if (d->packets.count || !MsDeliverLocked(d, &packet)) break;
        ++*consumed;
    }
    WdfSpinLockRelease(d->lock);
}
VOID MsControl(WDFQUEUE queue, WDFREQUEST request, size_t output, size_t input, ULONG code) {
    MS_DEVICE *d = MsGet(WdfIoQueueGetDevice(queue));
    NTSTATUS status = STATUS_SUCCESS;
    PVOID buffer = NULL;
    size_t required = 0, written = 0;
    BOOLEAN is_output = FALSE;
    const ms_slot *head;
    ms_decision decision;
    ms_packet packet;
    UNREFERENCED_PARAMETER(output); UNREFERENCED_PARAMETER(input);
    switch (code) {
    case MS_GET_INFO: required = sizeof(ms_info); is_output = TRUE; break;
    case MS_RECEIVE: required = sizeof(ms_event); is_output = TRUE; break;
    case MS_RESOLVE: required = sizeof(ms_decision); break;
    case MS_INJECT: required = sizeof(ms_packet); break;
    case MS_START: case MS_STOP: break;
    default: WdfRequestComplete(request, STATUS_INVALID_DEVICE_REQUEST); return;
    }
    if (required) {
        status = is_output ? WdfRequestRetrieveOutputBuffer(request, required, &buffer, NULL)
                           : WdfRequestRetrieveInputBuffer(request, required, &buffer, NULL);
        if (!NT_SUCCESS(status)) { WdfRequestComplete(request, status); return; }
    }
    WdfSpinLockAcquire(d->lock);
    if (!WdfRequestGetFileObject(request) || MsFileGet(WdfRequestGetFileObject(request))->closing) {
        status = STATUS_FILE_CLOSED; goto done;
    }
    if (!d->powered || !d->upper.ClassService) { status = STATUS_DEVICE_NOT_READY; goto done; }
    if (msq_expired(&d->packets, MsNow())) MsRecoverLocked(d);
    switch (code) {
    case MS_GET_INFO: {
        ms_info info = {MS_VERSION, d->capturing, d->packets.count, MS_TIMEOUT_MS};
        RtlCopyMemory(buffer, &info, sizeof(info)); written = sizeof(info); break;
    }
    case MS_START:
        if (d->packets.count) status = STATUS_DEVICE_BUSY;
        else d->capturing = TRUE;
        break;
    case MS_STOP: MsRecoverLocked(d); break;
    case MS_RECEIVE:
        if (!d->capturing) { status = STATUS_DEVICE_NOT_READY; break; }
        head = msq_front(&d->packets);
        if (!head) { status = STATUS_NO_MORE_ENTRIES; break; }
        RtlCopyMemory(buffer, &head->event, sizeof(ms_event)); written = sizeof(ms_event); break;
    case MS_RESOLVE:
        RtlCopyMemory(&decision, buffer, sizeof(decision));
        head = msq_front(&d->packets);
        if (!d->capturing || !head || head->event.sequence != decision.sequence) {
            status = STATUS_NOT_FOUND; break;
        }
        if (decision.reserved || decision.action > MS_DROP ||
            (decision.action == MS_FORWARD && !ms_packet_valid(&decision.packet))) {
            status = STATUS_INVALID_PARAMETER; break;
        }
        /* Users cannot spoof another device's unit id. */
        decision.packet.unit_id = head->event.packet.unit_id;
        if (decision.action == MS_FORWARD && !MsDeliverLocked(d, &decision.packet)) {
            status = STATUS_DEVICE_BUSY; break;
        }
        msq_pop(&d->packets, decision.sequence); break;
    case MS_INJECT:
        RtlCopyMemory(&packet, buffer, sizeof(packet));
        packet.unit_id = 0;
        if (!ms_packet_valid(&packet)) status = STATUS_INVALID_PARAMETER;
        else if (d->packets.count || !MsDeliverLocked(d, &packet)) status = STATUS_DEVICE_BUSY;
        break;
    }
done:
    WdfSpinLockRelease(d->lock);
    WdfRequestCompleteWithInformation(request, status, written);
}
