/* SPDX-License-Identifier: MIT */
#include "mousestream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static volatile LONG stopped;
static BOOL WINAPI stop_handler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        InterlockedExchange(&stopped, 1); return TRUE;
    }
    return FALSE;
}
int main(int argc, char **argv) {
    wchar_t path[MS_PATH_CHARS];
    HANDLE device;
    ms_event event;
    DWORD index = 0;
    int result, exit_code = 0;
    BOOL swap = FALSE;
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "--list") == 0)) {
        while (ms_device_path(index, path, MS_PATH_CHARS)) {
            wprintf(L"%lu: %ls\n", index++, path);
        }
        if (GetLastError() != ERROR_NO_MORE_ITEMS) {
            fprintf(stderr, "Enumeration failed: %lu\n", GetLastError()); return 1;
        }
        puts("Usage: passthrough <index> [--swap-buttons] (administrator)"); return 0;
    }
    {
        char *end;
        unsigned long parsed = strtoul(argv[1], &end, 10);
        if (!argv[1][0] || *end || parsed > 65535 || argc > 3 ||
            (argc == 3 && strcmp(argv[2], "--swap-buttons") != 0)) return 2;
        index = (DWORD)parsed;
    }
    swap = argc == 3;
    if (!ms_device_path(index, path, MS_PATH_CHARS)) {
        fprintf(stderr, "Cannot find mouse: %lu\n", GetLastError()); return 1;
    }
    device = ms_open(path);
    if (device == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open mouse: %lu\n", GetLastError()); return 1;
    }
    SetConsoleCtrlHandler(stop_handler, TRUE);
    if (!ms_start(device)) {
        fprintf(stderr, "Cannot start: %lu\n", GetLastError()); ms_close(device); return 1;
    }
    puts("Capturing. Ctrl+C exits. Do not pause this process in a debugger.");
    while (!InterlockedCompareExchange(&stopped, 0, 0)) {
        result = ms_receive(device, &event, 50);
        if (result == 0) continue;
        if (result < 0) { fprintf(stderr, "Receive failed: %lu\n", GetLastError()); exit_code = 1; break; }
        if (swap) {
            uint16_t b = event.packet.buttons;
            event.packet.buttons = (uint16_t)((b & ~15u) | ((b & 3u) << 2) | ((b & 12u) >> 2));
        }
        /* Retry the same decision on upper-stack backpressure; never inject a copy. */
        while (!ms_resolve(device, &event, MS_FORWARD)) {
            DWORD error = GetLastError();
            if (error != ERROR_BUSY || InterlockedCompareExchange(&stopped, 0, 0)) {
                fprintf(stderr, "Resolve failed: %lu\n", error); exit_code = 1; goto done;
            }
            Sleep(1);
        }
    }
done:
    ms_stop(device); ms_close(device); return exit_code;
}
