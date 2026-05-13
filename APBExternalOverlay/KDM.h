#pragma once
#include <Windows.h>
#include <cstdint>

namespace KDM {
    // IOCTL codes for the driver
    #define IOCTL_APB_READ_MEM      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
    #define IOCTL_APB_WRITE_MEM     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
    #define IOCTL_APB_GET_BASE      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

    // Global driver handle
    extern HANDLE g_Handle;

    // Read request structure
    struct ReadRequest {
        ULONG64 ProcessId;
        ULONG64 Address;
        ULONG64 Size;
    };

    // Write request structure
    struct WriteRequest {
        ULONG64 ProcessId;
        ULONG64 Address;
        ULONG64 Size;
        BYTE Data[4096];
    };

    // Base address request
    struct BaseAddressRequest {
        ULONG64 ProcessId;
        ULONG64 BaseAddress;
    };

    /**
     * Initialize the kernel driver connection
     * @param ProcessId Target process ID
     * @return true if driver initialized successfully
     */
    bool Init(DWORD ProcessId);

    /**
     * Check if driver is available
     * @return true if driver handle is valid
     */
    bool Available();

    /**
     * Get base address of target process module
     * @param ProcessId Target process ID
     * @return Base address or 0 on failure
     */
    ULONG64 GetBaseAddress(DWORD ProcessId);

    /**
     * Read memory from target process
     * @param ProcessId Target process ID
     * @param Address Address to read from
     * @param Buffer Output buffer
     * @param Size Size to read
     * @return true on success
     */
    bool ReadMemory(DWORD ProcessId, ULONG64 Address, void* Buffer, ULONG64 Size);

    /**
     * Write memory to target process
     * @param ProcessId Target process ID
     * @param Address Address to write to
     * @param Buffer Data to write
     * @param Size Size to write
     * @return true on success
     */
    bool WriteMemory(DWORD ProcessId, ULONG64 Address, void* Buffer, ULONG64 Size);

    /**
     * Cleanup driver resources
     */
    void Cleanup();
}