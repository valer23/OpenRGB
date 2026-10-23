/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "WindowsHidTransfer.h"
#ifdef _WIN32
#include <cstring>
#include <vector>

struct WindowsHidTransfer::Operation
{
    Api api;
    HANDLE event = nullptr;
    HANDLE device = INVALID_HANDLE_VALUE;
    OVERLAPPED overlapped = {};
    std::vector<unsigned char> buffer;
    explicit Operation(Api calls) : api(calls) {}
    ~Operation()
    {
        if(device != INVALID_HANDLE_VALUE) { api.close(device); }
        if(event) { api.close(event); }
    }
};

WindowsHidTransfer::WindowsHidTransfer(Api calls) : api(calls) {}
WindowsHidTransfer::~WindowsHidTransfer()
{
    if(!Ready())
    {
        // A broken driver can leave cancellation pending indefinitely. Retain
        // this one operation until process exit rather than free driver-owned
        // storage or hang OpenRGB shutdown. No further handle may be opened.
        pending.release();
    }
}

bool WindowsHidTransfer::Ready()
{
    if(!pending) { return true; }
    if(api.wait(pending->event, 0) != WAIT_OBJECT_0) { return false; }
    pending.reset();
    return true;
}

int WindowsHidTransfer::Transfer(HANDLE& device, bool writing, unsigned char* buffer, DWORD size, DWORD timeout)
{
    if(size > 64) { return -1; }
    return TransferAt(device, writing, buffer, size, timeout, 0);
}

int WindowsHidTransfer::ReadAt(HANDLE& file, unsigned char* buffer, DWORD size, DWORD timeout, uint64_t offset)
{
    if(size > 65536) { return -1; }
    return TransferAt(file, false, buffer, size, timeout, offset);
}

int WindowsHidTransfer::TransferAt(HANDLE& device, bool writing, unsigned char* buffer, DWORD size, DWORD timeout, uint64_t offset)
{
    if(!Ready() || device == INVALID_HANDLE_VALUE || !buffer) { return -1; }
    auto operation = std::make_unique<Operation>(api);
    operation->event = api.create_event(nullptr, TRUE, FALSE, nullptr);
    if(!operation->event) { return -1; }
    operation->overlapped.hEvent = operation->event;
    operation->overlapped.Offset = static_cast<DWORD>(offset);
    operation->overlapped.OffsetHigh = static_cast<DWORD>(offset >> 32);
    operation->buffer.resize(size);
    if(writing) { std::memcpy(operation->buffer.data(), buffer, size); }
    DWORD transferred = 0;
    const BOOL immediate = writing ?
        api.write(device, operation->buffer.data(), size, &transferred, &operation->overlapped) :
        api.read(device, operation->buffer.data(), size, &transferred, &operation->overlapped);
    if(!immediate)
    {
        if(api.last_error() != ERROR_IO_PENDING) { return -1; }
        if(api.wait(operation->event, timeout) != WAIT_OBJECT_0)
        {
            api.cancel(device, &operation->overlapped);
            if(api.wait(operation->event, 100) != WAIT_OBJECT_0)
            {
                operation->device = device;
                device = INVALID_HANDLE_VALUE;
                pending = std::move(operation);
                return -1;
            }
            return 0; // Timed out, even if completion raced cancellation.
        }
        if(!api.result(device, &operation->overlapped, &transferred, FALSE)) { return -1; }
    }
    if(transferred > size) { return -1; }
    if(!writing) { std::memcpy(buffer, operation->buffer.data(), transferred); }
    return static_cast<int>(transferred);
}
#endif
