#ifndef ANTIDEBUG_H
#define ANTIDEBUG_H

#include <Windows.h>

namespace AntiAnalysis {
    inline bool IsDebuggerPresentCheck() {
        return IsDebuggerPresent();
    }

    inline bool IsRunningInVM() {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\ACPI\\DSDT\\VBOX__", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Oracle\\VirtualBox Guest Additions", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return true;
        }
        return false;
    }

    inline void PerformChecks() {
        if (IsDebuggerPresentCheck() || IsRunningInVM()) {
            // Silently exit
            exit(0);
        }
    }
}

#endif // ANTIDEBUG_H
