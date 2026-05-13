#include "KDM.h"
#include <stdio.h>
#include <string>

namespace KDM {
    HANDLE g_Handle = INVALID_HANDLE_VALUE;

    bool Init(DWORD ProcessId) {
        if (g_Handle != INVALID_HANDLE_VALUE) {
            return true; // Already initialized
        }

        // Attempt to open the driver device
        // The driver should be loaded as a device with a specific symbolic link
        // Common symbolic link patterns: "\\\\.\\APBDriver", "\\\\.\\KernelBypass", etc.
        const wchar_t* driverNames[] = {
            L"\\\\.\\APBDriver",
            L"\\\\.\\APBBypass",
            L"\\\\.\\KernelBypass",
            L"\\\\.\\DriverBypass"
        };

        for (const auto& driverName : driverNames) {
            g_Handle = CreateFileW(
                driverName,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL
            );

            if (g_Handle != INVALID_HANDLE_VALUE) {
                printf("[+] KDM: Driver initialized successfully via %ws\n", driverName);
                return true;
            }
        }

        printf("[-] KDM: Failed to open driver. Make sure APBDriver.sys is loaded.\n");
        return false;
    }

    bool Available() {
        return g_Handle != INVALID_HANDLE_VALUE;
    }

    ULONG64 GetBaseAddress(DWORD ProcessId) {
        if (g_Handle == INVALID_HANDLE_VALUE) {
            printf("[-] KDM: Driver not initialized\n");
            return 0;
        }

        BaseAddressRequest req = { ProcessId, 0 };
        DWORD bytesReturned = 0;

        BOOL result = DeviceIoControl(
            g_Handle,
            IOCTL_APB_GET_BASE,
            &req,
            sizeof(req),
            &req,
            sizeof(req),
            &bytesReturned,
            NULL
        );

        if (!result) {
            printf("[-] KDM: Failed to get base address. Error: %lu\n", GetLastError());
            return 0;
        }

        printf("[+] KDM: Got base address: 0x%llX\n", req.BaseAddress);
        return req.BaseAddress;
    }

    bool ReadMemory(DWORD ProcessId, ULONG64 Address, void* Buffer, ULONG64 Size) {
        if (g_Handle == INVALID_HANDLE_VALUE) {
            printf("[-] KDM: Driver not initialized\n");
            return false;
        }

        if (Size > 0x1000) {
            // Read in chunks if size is large
            ULONG64 offset = 0;
            while (offset < Size) {
                ULONG64 chunkSize = min(0x1000, Size - offset);
                if (!ReadMemory(ProcessId, Address + offset, (char*)Buffer + offset, chunkSize)) {
                    return false;
                }
                offset += chunkSize;
            }
            return true;
        }

        ReadRequest req = { ProcessId, Address, Size };
        DWORD bytesReturned = 0;

        BOOL result = DeviceIoControl(
            g_Handle,
            IOCTL_APB_READ_MEM,
            &req,
            sizeof(req),
            Buffer,
            (DWORD)Size,
            &bytesReturned,
            NULL
        );

        if (!result) {
            printf("[-] KDM: Failed to read memory at 0x%llX. Error: %lu\n", Address, GetLastError());
            return false;
        }

        return true;
    }

    bool WriteMemory(DWORD ProcessId, ULONG64 Address, void* Buffer, ULONG64 Size) {
        if (g_Handle == INVALID_HANDLE_VALUE) {
            printf("[-] KDM: Driver not initialized\n");
            return false;
        }

        if (Size > sizeof(WriteRequest::Data)) {
            printf("[-] KDM: Write size too large (max: %zu)\n", sizeof(WriteRequest::Data));
            return false;
        }

        WriteRequest req = {};
        req.ProcessId = ProcessId;
        req.Address = Address;
        req.Size = Size;
        memcpy(req.Data, Buffer, Size);

        DWORD bytesReturned = 0;

        BOOL result = DeviceIoControl(
            g_Handle,
            IOCTL_APB_WRITE_MEM,
            &req,
            sizeof(req),
            NULL,
            0,
            &bytesReturned,
            NULL
        );

        if (!result) {
            printf("[-] KDM: Failed to write memory at 0x%llX. Error: %lu\n", Address, GetLastError());
            return false;
        }

        return true;
    }

    void Cleanup() {
        if (g_Handle != INVALID_HANDLE_VALUE) {
            CloseHandle(g_Handle);
            g_Handle = INVALID_HANDLE_VALUE;
            printf("[+] KDM: Driver cleaned up\n");
        }
    }
}