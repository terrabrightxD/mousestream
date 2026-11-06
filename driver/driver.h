/* SPDX-License-Identifier: MIT */
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <ntddmou.h>
#include <kbdmou.h>
#include <wdmsec.h>
#include "../core/queue.h"
/* Private raw device class and discoverable sideband interface. */
static const GUID MS_CLASS = {0xb7db6858,0xddad,0x4c65,{0x98,0x91,0x27,0x33,0x85,0xe8,0xc6,0x6c}};
static const GUID MS_INTERFACE = {0xd614d063,0x761e,0x458d,{0xb2,0x26,0xfd,0xdc,0x13,0x2a,0x9f,0x29}};
typedef struct MS_DEVICE {
    WDFSPINLOCK lock;
    WDFTIMER timer;
    WDFQUEUE control_queue;
    CONNECT_DATA upper;
    ms_queue packets;
    BOOLEAN powered, capturing;
} MS_DEVICE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MS_DEVICE, MsGet)
typedef struct MS_CHILD { WDFDEVICE parent; WDFQUEUE parent_queue; } MS_CHILD;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MS_CHILD, MsChildGet)
typedef struct MS_FILE { BOOLEAN closing; } MS_FILE;
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(MS_FILE, MsFileGet)
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD MsAdd;
EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL MsInternal;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL MsControl;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL MsChildControl;
EVT_WDF_FILE_CLEANUP MsCleanup;
EVT_WDF_DEVICE_D0_ENTRY MsD0Entry;
EVT_WDF_DEVICE_D0_EXIT MsD0Exit;
EVT_WDF_DEVICE_SURPRISE_REMOVAL MsSurprise;
EVT_WDF_TIMER MsTick;
EVT_WDF_REQUEST_COMPLETION_ROUTINE MsConnectComplete;
NTSTATUS MsCreateChild(WDFDEVICE parent);
VOID MsService(PDEVICE_OBJECT object, PMOUSE_INPUT_DATA start,
               PMOUSE_INPUT_DATA end, PULONG consumed);
VOID MsRecoverLocked(MS_DEVICE *d);
