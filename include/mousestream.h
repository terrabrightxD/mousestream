/* SPDX-License-Identifier: MIT */
#ifndef MOUSESTREAM_H
#define MOUSESTREAM_H
#include <windows.h>
#include "ms_protocol.h"
#ifdef MS_BUILD
#define MS_API __declspec(dllexport)
#else
#define MS_API __declspec(dllimport)
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* Return BOOL, with details in GetLastError(), unless stated otherwise.
   One controlling handle per physical mouse. Calls on a handle must be serialized. */
MS_API BOOL ms_device_path(DWORD index, wchar_t *path, DWORD capacity);
MS_API HANDLE ms_open(const wchar_t *path); /* INVALID_HANDLE_VALUE on failure */
MS_API void ms_close(HANDLE device);
MS_API BOOL ms_get_info(HANDLE device, ms_info *info);
MS_API BOOL ms_start(HANDLE device);
MS_API BOOL ms_stop(HANDLE device);
/* Returns 1=event, 0=timeout, -1=error. Receiving does not remove the event. */
MS_API int ms_receive(HANDLE device, ms_event *event, DWORD timeout_ms);
/* Resolve the oldest event exactly once. FALSE with ERROR_BUSY permits retry;
   ERROR_NOT_FOUND means the watchdog already recovered it: never inject it again. */
MS_API BOOL ms_resolve(HANDLE device, const ms_event *event, DWORD action);
MS_API BOOL ms_inject(HANDLE device, const ms_packet *packet);
#ifdef __cplusplus
}
#endif
#endif
