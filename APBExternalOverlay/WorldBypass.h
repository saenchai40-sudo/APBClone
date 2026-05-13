#pragma once
#include <windows.h>
#include <cstdint>
#include <optional>
#include <mutex>
#include "KDM.h"
#include <algorithm>
#include <cfloat>

namespace KDM {
    constexpr DWORD IOCTL_APB_READ_MEM = 0x00222000UL;
    constexpr DWORD IOCTL_APB_GET_BASE = 0x00222004UL;

#pragma pack(push, 1)
    struct ReadRequest { uint64_t ProcessId; uint64_t Address; uint64_t Size; };
    struct BaseRequest { uint64_t ProcessId; uint64_t BaseAddress; };
#pragma pack(pop)

    class APBReader {
    public:
        explicit APBReader(uint32_t pid = 0) noexcept
            : handle_(INVALID_HANDLE_VALUE), pid_(pid), lastError_(ERROR_SUCCESS) {}

        ~APBReader() noexcept {
            std::lock_guard<std::mutex> lk(mutex_);
            if (handle_ != INVALID_HANDLE_VALUE) {
                CloseHandle(handle_);
                handle_ = INVALID_HANDLE_VALUE;
            }
        }

        // Initialize or reinitialize with pid. Returns true on success.
        bool Init(uint32_t pid) noexcept {
            std::lock_guard<std::mutex> lk(mutex_);
            pid_ = pid;
            if (handle_ != INVALID_HANDLE_VALUE) {
                CloseHandle(handle_);
                handle_ = INVALID_HANDLE_VALUE;
            }

            handle_ = CreateFileW(L"\\\\.\\APBReader",
                                   GENERIC_READ | GENERIC_WRITE,
                                   0, nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);

            if (handle_ == INVALID_HANDLE_VALUE) {
                lastError_ = GetLastError();
                return false;
            }

            lastError_ = ERROR_SUCCESS;
            return true;
        }

        bool IsAvailable() const noexcept {
            std::lock_guard<std::mutex> lk(mutex_);
            return handle_ != INVALID_HANDLE_VALUE;
        }

        // Returns base address on success, std::nullopt on failure.
        std::optional<uintptr_t> GetBaseAddress(uint32_t pid) noexcept {
            std::lock_guard<std::mutex> lk(mutex_);
            if (handle_ == INVALID_HANDLE_VALUE) {
                lastError_ = ERROR_INVALID_HANDLE;
                return std::nullopt;
            }

            BaseRequest req{ static_cast<uint64_t>(pid), 0 };
            DWORD returned = 0;
            BOOL ok = DeviceIoControl(handle_, IOCTL_APB_GET_BASE,
                                      &req, static_cast<DWORD>(sizeof(req)),
                                      &req, static_cast<DWORD>(sizeof(req)),
                                      &returned, nullptr);
            if (!ok) {
                lastError_ = GetLastError();
                return std::nullopt;
            }

            lastError_ = ERROR_SUCCESS;
            return reinterpret_cast<uintptr_t>(req.BaseAddress);
        }

        // Read typed value from target process. Returns std::nullopt on failure.
        template<typename T>
        std::optional<T> Read(uintptr_t addr) noexcept {
            std::lock_guard<std::mutex> lk(mutex_);
            if (handle_ == INVALID_HANDLE_VALUE) {
                lastError_ = ERROR_INVALID_HANDLE;
                return std::nullopt;
            }

            ReadRequest req{
                static_cast<uint64_t>(pid_),
                static_cast<uint64_t>(addr),
                static_cast<uint64_t>(sizeof(T))
            };

            T result{};
            DWORD returned = 0;
            BOOL ok = DeviceIoControl(handle_, IOCTL_APB_READ_MEM,
                                      &req, static_cast<DWORD>(sizeof(req)),
                                      &result, static_cast<DWORD>(sizeof(T)),
                                      &returned, nullptr);

            if (!ok || returned < sizeof(T)) {
                lastError_ = GetLastError();
                return std::nullopt;
            }

            lastError_ = ERROR_SUCCESS;
            return result;
        }

        DWORD LastError() const noexcept {
            std::lock_guard<std::mutex> lk(mutex_);
            return lastError_;
        }

    private:
        mutable std::mutex mutex_;
        HANDLE handle_;
        uint32_t pid_;
        mutable DWORD lastError_;
    };
}

namespace Bypass {
    using NtRVMFunc = NTSTATUS(__stdcall*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);

    // ResolveWorld now takes a reference to a reader; returns std::optional<uintptr_t>.
    inline std::optional<uintptr_t> ResolveWorld(KDM::APBReader& reader, uintptr_t moduleBase) noexcept {
        // GWorld IDA relative = 0x3D34968
        constexpr uintptr_t GWorldOffset = 0x3D34968ULL;
        auto val = reader.Read<uintptr_t>(moduleBase + GWorldOffset);
        if (!val) return std::nullopt;
        return *val;
    }
}