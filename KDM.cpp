#include "KDM.h"
#include <windows.h>
#include <cstdio>
#include <string>
#include <algorithm>

namespace KDM {
    HANDLE g_Handle = INVALID_HANDLE_VALUE;

    bool Init(DWORD /*ProcessId*/) {
        if (g_Handle != INVALID_HANDLE_VALUE) return true;
        const wchar_t* driverNames[] = {
            L"\\\\.\\APBDriver",
            L"\\\\.\\APBBypass",
            L"\\\\.\\KernelBypass",
            L"\\\\.\\DriverBypass"
        };
        for (const auto& name : driverNames) {
            g_Handle = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (g_Handle != INVALID_HANDLE_VALUE) {
                std::printf("[+] KDM: opened driver %ls\n", name);
                return true;
            }
        }
        g_Handle = INVALID_HANDLE_VALUE;
        std::printf("[-] KDM: failed to open driver device\n");
        return false;
    }

    bool Available() {
        return g_Handle != INVALID_HANDLE_VALUE;
    }

    ULONG64 GetBaseAddress(DWORD ProcessId) {
        if (!Available()) return 0;
        BaseAddressRequest req = { ProcessId, 0 };
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(g_Handle, IOCTL_APB_GET_BASE, &req, sizeof(req), &req, sizeof(req), &bytesReturned, NULL);
        if (!ok) return 0;
        return req.BaseAddress;
    }

    bool ReadMemory(DWORD ProcessId, ULONG64 Address, void* Buffer, ULONG64 Size) {
        if (!Available()) return false;
        if (Size == 0) return true;
        // read in chunks
        const ULONG64 CHUNK = 4096;
        ULONG64 offset = 0;
        while (offset < Size) {
            ULONG64 chunk = std::min(CHUNK, Size - offset);
            ReadRequest r{ ProcessId, Address + offset, chunk };
            DWORD bytesReturned = 0;
            BOOL ok = DeviceIoControl(g_Handle, IOCTL_APB_READ_MEM, &r, sizeof(r), (BYTE*)Buffer + offset, (DWORD)chunk, &bytesReturned, NULL);
            if (!ok || bytesReturned == 0) return false;
            offset += chunk;
        }
        return true;
    }

    bool WriteMemory(DWORD ProcessId, ULONG64 Address, void* Buffer, ULONG64 Size) {
        if (!Available()) return false;
        if (Size == 0) return true;
        if (Size > sizeof(WriteRequest::Data)) return false;
        WriteRequest req = {};
        req.ProcessId = ProcessId;
        req.Address = Address;
        req.Size = Size;
        memcpy(req.Data, Buffer, (size_t)Size);
        DWORD bytesReturned = 0;
        BOOL ok = DeviceIoControl(g_Handle, IOCTL_APB_WRITE_MEM, &req, sizeof(req), NULL, 0, &bytesReturned, NULL);
        return ok != FALSE;
    }

    void Cleanup() {
        if (g_Handle != INVALID_HANDLE_VALUE) {
            CloseHandle(g_Handle);
            g_Handle = INVALID_HANDLE_VALUE;
        }
    }
}