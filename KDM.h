#pragma once
#include <Windows.h>
#include <cstdint>

namespace KDM {
    // Adjust IOCTL values to match your driver implementation
    #ifndef IOCTL_APB_READ_MEM
    #define IOCTL_APB_READ_MEM      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
    #endif
    #ifndef IOCTL_APB_WRITE_MEM
    #define IOCTL_APB_WRITE_MEM     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
    #endif
    #ifndef IOCTL_APB_GET_BASE
    #define IOCTL_APB_GET_BASE      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
    #endif

    extern HANDLE g_Handle;

    struct ReadRequest {
        ULONG64 ProcessId;
        ULONG64 Address;
        ULONG64 Size;
    };

    struct WriteRequest {
        ULONG64 ProcessId;
        ULONG64 Address;
        ULONG64 Size;
        BYTE Data[4096];
    };

    struct BaseAddressRequest {
        ULONG64 ProcessId;
        ULONG64 BaseAddress;
    };

    bool Init(DWORD ProcessId);
    bool Available();
    ULONG64 GetBaseAddress(DWORD ProcessId);
    bool ReadMemory(DWORD ProcessId, ULONG64 Address, void* Buffer, ULONG64 Size);
    bool WriteMemory(DWORD ProcessId, ULONG64 Address, void* Buffer, ULONG64 Size);
    void Cleanup();
}