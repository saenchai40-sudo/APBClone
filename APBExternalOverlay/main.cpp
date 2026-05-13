#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include <vector>
#include <Shlwapi.h> 
#pragma comment(lib, "shlwapi.lib")

#include <iostream>
#include <cstdio>

#include "ExternalOverlay.h"
#include "antidebug.h"
#include "obfuscate.h"
#include "utils.h"

void RelaunchAsRandom() {
    wchar_t currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);

    std::wstring originalName = L"APBExternalOverlay.exe";
    const wchar_t* fileName = PathFindFileNameW(currentPath);

    if (fileName && _wcsicmp(fileName, originalName.c_str()) == 0) {
        std::wstring newName = GenerateRandomString(12) + L".exe";
        
        wchar_t newPath[MAX_PATH];
        wcscpy_s(newPath, currentPath);
        PathRemoveFileSpecW(newPath);
        PathAppendW(newPath, newName.c_str());

        if (CopyFileW(currentPath, newPath, FALSE)) {
            STARTUPINFOW si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            if (CreateProcessW(newPath, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                exit(0); 
            }
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);
    std::cout << "Console enabled.\n";

    RelaunchAsRandom();
    std::cout << "Relaunch check complete.\n";


    std::cout << "Initializing overlay...\n";
    ExternalOverlay overlay;
    
    if (!overlay.Initialize(hInstance)) {
        MessageBoxA(NULL, OBFUSCATE("Initialization failed."), OBFUSCATE("Error"), MB_ICONERROR);
        std::cout << "Overlay initialization failed.\n";
        return 1;
    }
    std::cout << "Overlay initialized successfully.\n";
    
    MessageBoxA(NULL, 
        OBFUSCATE("apb.one overlay loaded!\n\nPress INSERT for menu\nNavigation with arrows\nEnter for apply"), 
        OBFUSCATE("credits: gay,seven"), 
        MB_ICONINFORMATION);
    
    std::cout << "Starting overlay main loop...\n";
    overlay.Run();
    std::cout << "Overlay main loop finished.\n";
    
    wchar_t currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);

    std::wstring originalName = L"APBExternalOverlay.exe";
    const wchar_t* fileName = PathFindFileNameW(currentPath);

    return 0;
}
