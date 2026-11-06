/* SPDX-License-Identifier: MIT */
#define MS_BUILD
#include "../include/mousestream.h"
#include <setupapi.h>
#include <stdlib.h>
#include <wchar.h>
static const GUID MS_INTERFACE = {0xd614d063,0x761e,0x458d,{0xb2,0x26,0xfd,0xdc,0x13,0x2a,0x9f,0x29}};
static BOOL ms_call(HANDLE device, DWORD code, const void *input, DWORD in_size, void *output, DWORD out_size) {
    DWORD got = 0;
    if (!DeviceIoControl(device, code, (void *)input, in_size, output, out_size, &got, NULL)) return FALSE;
    if (out_size && got != out_size) { SetLastError(ERROR_INVALID_DATA); return FALSE; }
    return TRUE;
}
BOOL ms_device_path(DWORD index, wchar_t *path, DWORD capacity) {
    HDEVINFO set;
    SP_DEVICE_INTERFACE_DATA iface;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail = NULL;
    DWORD needed = 0, error = ERROR_SUCCESS;
    BOOL ok = FALSE;
    if (!path || !capacity) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    set = SetupDiGetClassDevsW(&MS_INTERFACE, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return FALSE;
    ZeroMemory(&iface, sizeof(iface)); iface.cbSize = sizeof(iface);
    if (!SetupDiEnumDeviceInterfaces(set, NULL, &MS_INTERFACE, index, &iface)) { error = GetLastError(); goto done; }
    SetupDiGetDeviceInterfaceDetailW(set, &iface, NULL, 0, &needed, NULL);
    error = GetLastError();
    if (error != ERROR_INSUFFICIENT_BUFFER || !needed) goto done;
    detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)malloc(needed);
    if (!detail) { error = ERROR_NOT_ENOUGH_MEMORY; goto done; }
    detail->cbSize = sizeof(*detail);
    if (!SetupDiGetDeviceInterfaceDetailW(set, &iface, detail, needed, NULL, NULL)) { error = GetLastError(); goto done; }
    if (wcslen(detail->DevicePath) >= capacity) { error = ERROR_INSUFFICIENT_BUFFER; goto done; }
    wcscpy_s(path, capacity, detail->DevicePath); ok = TRUE;
done:
    free(detail); SetupDiDestroyDeviceInfoList(set);
    if (!ok) SetLastError(error);
    return ok;
}
HANDLE ms_open(const wchar_t *path) {
    HANDLE handle;
    ms_info info;
    DWORD error;
    if (!path) { SetLastError(ERROR_INVALID_PARAMETER); return INVALID_HANDLE_VALUE; }
    handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE) return handle;
    if (!ms_get_info(handle, &info)) {
        error = GetLastError(); CloseHandle(handle); SetLastError(error); return INVALID_HANDLE_VALUE;
    }
    if (info.version != MS_VERSION) {
        CloseHandle(handle); SetLastError(ERROR_REVISION_MISMATCH); return INVALID_HANDLE_VALUE;
    }
    return handle;
}
void ms_close(HANDLE device) { if (device != INVALID_HANDLE_VALUE && device) CloseHandle(device); }
BOOL ms_get_info(HANDLE device, ms_info *info) {
    if (!info) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    return ms_call(device, MS_GET_INFO, NULL, 0, info, sizeof(*info));
}
BOOL ms_start(HANDLE device) { return ms_call(device, MS_START, NULL, 0, NULL, 0); }
BOOL ms_stop(HANDLE device) { return ms_call(device, MS_STOP, NULL, 0, NULL, 0); }
int ms_receive(HANDLE device, ms_event *event, DWORD timeout_ms) {
    ULONGLONG start = GetTickCount64();
    if (!event) { SetLastError(ERROR_INVALID_PARAMETER); return -1; }
    for (;;) {
        if (ms_call(device, MS_RECEIVE, NULL, 0, event, sizeof(*event))) return 1;
        if (GetLastError() != ERROR_NO_MORE_ITEMS) return -1;
        if (timeout_ms != INFINITE && GetTickCount64() - start >= timeout_ms) {
            SetLastError(ERROR_TIMEOUT); return 0;
        }
        Sleep(1);
    }
}
BOOL ms_resolve(HANDLE device, const ms_event *event, DWORD action) {
    ms_decision decision;
    if (!event || action > MS_DROP) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    ZeroMemory(&decision, sizeof(decision));
    decision.sequence = event->sequence; decision.action = action; decision.packet = event->packet;
    return ms_call(device, MS_RESOLVE, &decision, sizeof(decision), NULL, 0);
}
BOOL ms_inject(HANDLE device, const ms_packet *packet) {
    if (!packet) { SetLastError(ERROR_INVALID_PARAMETER); return FALSE; }
    return ms_call(device, MS_INJECT, packet, sizeof(*packet), NULL, 0);
}
