/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cstdint>
#include <memory>

// A timed-out HID operation must keep its OVERLAPPED, buffer and device alive
// until cancellation completes. Never wait indefinitely on the GUI/unload path.
class WindowsHidTransfer
{
public:
    struct Api
    {
        decltype(&CreateEventW) create_event = &CreateEventW;
        decltype(&ReadFile) read = &ReadFile;
        decltype(&WriteFile) write = &WriteFile;
        decltype(&GetLastError) last_error = &GetLastError;
        decltype(&WaitForSingleObject) wait = &WaitForSingleObject;
        decltype(&CancelIoEx) cancel = &CancelIoEx;
        decltype(&GetOverlappedResult) result = &GetOverlappedResult;
        decltype(&CloseHandle) close = &CloseHandle;
    };
    explicit WindowsHidTransfer(Api api);
    ~WindowsHidTransfer();
    WindowsHidTransfer(const WindowsHidTransfer&) = delete;
    WindowsHidTransfer& operator=(const WindowsHidTransfer&) = delete;
    bool Ready();
    int Transfer(HANDLE& device, bool write, unsigned char* buffer, DWORD size, DWORD timeout);
    int ReadAt(HANDLE& file, unsigned char* buffer, DWORD size, DWORD timeout, uint64_t offset);
private:
    int TransferAt(HANDLE& device, bool writing, unsigned char* buffer, DWORD size, DWORD timeout, uint64_t offset);
    struct Operation;
    Api api;
    std::unique_ptr<Operation> pending;
};
#endif
