#define _USE_MATH_DEFINES
#include "ExternalOverlay.h"
#include "obfuscate.h"
#include <shlwapi.h>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")
#include "utils.h"
#include <dwmapi.h>
#include <iostream>
#include <psapi.h>
#include <random>
#include <sstream>
#include <iomanip>
#include "KDM.h"
#include <algorithm>
#include <cfloat>
#include "WorldBypass.h"

template<class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}
#include <tchar.h>
#include <string>
#include <cmath>
#include <vector>

#pragma comment(lib, "dwmapi.lib")

// Hexdigits table for signature scanning
static const char* hexdigits =
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\001\002\003\004\005\006\007\010\011\000\000\000\000\000\000"
"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\012\013\014\015\016\017\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
"\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000";

// Custom vector math function
float Vec3Length(const Vec3& v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 Vec3Normalize(const Vec3& v) {
    float len = Vec3Length(v);
    if (len == 0.0f) return Vec3(0, 0, 0);
    return Vec3(v.x / len, v.y / len, v.z / len);
}

float Vec3Dot(const Vec3& v1, const Vec3& v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

ExternalOverlay::ExternalOverlay() {
    
}

ExternalOverlay::~ExternalOverlay() {
    Shutdown();
}

void ExternalOverlay::Run() {
    dataUpdateThread = std::thread(&ExternalOverlay::DataUpdateThread, this);

    MSG msg = { 0 };
    while (isRunning) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                isRunning = false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            HandleInput();
            if (isTargetFound) {
                RECT clientRect;
                GetClientRect(targetHwnd, &clientRect);
                MapWindowPoints(targetHwnd, NULL, (LPPOINT)&clientRect, 2);
                SetWindowPos(overlayHwnd, HWND_TOPMOST, clientRect.left, clientRect.top, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, SWP_NOACTIVATE);
            }
            Draw();
        }
    }
}

bool ExternalOverlay::ScanGameSignatures() {
    if (gameSignatures.empty()) InitializeSignatures();

    bool allFound = true;
    for (auto const& [name, sig] : gameSignatures) {
        uintptr_t address = FindSignature(sig.pattern.c_str());
        if (address) {
            // Read relative offset using the Driver-based Read template
            int32_t relOffset = Read<int32_t>(address + sig.offset);

            // Calculate final VA: InstructionAddr + OffsetLocation + 4 + Displacement
            uintptr_t finalAddress = address + sig.offset + 4 + relOffset;
            gameAddresses[name] = finalAddress;
        }
        else {
            allFound = false;
        }
    }

    // Final check for UWorld - if scan failed, try the GObjects resolve
    if (gameAddresses.find("UWorld") == gameAddresses.end()) {
        printf("[-] UWorld signature not found.\n");
    }

    return allFound;
}

// Update in ExternalOverlay.cpp
void ExternalOverlay::PrintAddresses() {
    if (!targetProcessHandle || !targetProcessId) {
        printf("[-] No target process handle!\n");
        return;
    }

    // FIX: Use the game base address found during initialization, not current process
    uintptr_t imageBase = gameBaseAddress;

    printf("\n=== Signature Scan Results ===\n");
    printf("[*] Process ID: %d\n", targetProcessId);
    printf("[*] Game base: 0x%p\n", (void*)imageBase);

    const char* names[] = { "GEngine", "UWorld", "GNames", "PawnClass", "GObjects" };
    for (const char* name : names) {
        auto it = gameAddresses.find(name);
        if (it != gameAddresses.end()) {
            printf("%-10s -> 0x%I64X\n", name, it->second);
        }
        else {
            printf("%-10s [NOT FOUND]\n", name);
        }
    }
}

bool ExternalOverlay::Initialize(HINSTANCE hInstance) {
    // Randomize window name
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(12, 16);
    randomWindowName = GenerateRandomString(distrib(gen));

    overlayHwnd = nullptr;
    targetHwnd = nullptr;
    d2dFactory = nullptr;
    renderTarget = nullptr;
    writeFactory = nullptr;
    textBrush = nullptr;
    selectedTextBrush = nullptr;
    menuVisible = true;
    selectedMenuItem = 0;
    isTargetFound = false;
    targetProcessId = 0;
    targetProcessHandle = nullptr;
    isRunning = true;
    boxESP = false;
    grenadeESP = true;

    // Initialize menu items
    menuItems.clear();


    aimbotTargetOptions = { OBFUSCATE_W(L"Head"), OBFUSCATE_W(L"Chest"), OBFUSCATE_W(L"Legs") };
    availableHotkeys = {
        {OBFUSCATE_W(L"C"), 'C'},
        {OBFUSCATE_W(L"Mouse1"), VK_LBUTTON},
        {OBFUSCATE_W(L"Mouse2"), VK_RBUTTON},
        {OBFUSCATE_W(L"Mouse5"), VK_XBUTTON2},
        {OBFUSCATE_W(L"E"), 'E'},
        {OBFUSCATE_W(L"F"), 'F'},
        {OBFUSCATE_W(L"Shift"), VK_SHIFT},
        {OBFUSCATE_W(L"L. Alt"), VK_LMENU}
    };


    std::vector<std::wstring> hotkeyOptions;
    for (const auto& key : availableHotkeys) {
        hotkeyOptions.push_back(key.first);
    }


    if (!availableHotkeys.empty()) {
        aimbotHotkey = availableHotkeys[aimbotHotkeyIndex].second;
    }

    // VISUALS
    menuItems.push_back({ MenuItemType::Section, std::wstring(OBFUSCATE_W(L"=== VISUALS ===")), nullptr, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Box ESP")), &boxESP, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Line ESP")), &lineESP, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Skeleton ESP")), &skeletonESP, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Rank ESP")), &rankESP, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Distance ESP")), &distanceESP, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Grenade ESP")), &grenadeESP, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"2D Radar")), &showRadar, nullptr, 0.0f, 0.0f, nullptr });

    // AIMBOT
    menuItems.push_back({ MenuItemType::Section, std::wstring(OBFUSCATE_W(L"=== AIMBOT ===")), nullptr, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Aimbot")), &aimbotEnabled, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Show Fov")), &showFov, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Slider, std::wstring(OBFUSCATE_W(L"Fov size")), nullptr, &fovSize, 1.0f, 360.0f, nullptr });
    menuItems.push_back({ MenuItemType::Slider, std::wstring(OBFUSCATE_W(L"Smoothing")), nullptr, &smoothing, 1.0f, 20.0f, nullptr });
    menuItems.push_back({ MenuItemType::Selector, std::wstring(OBFUSCATE_W(L"Aimbot Hotkey")), nullptr, nullptr, 0.0f, 0.0f, nullptr, &aimbotHotkeyIndex, hotkeyOptions });
    menuItems.push_back({ MenuItemType::Selector, std::wstring(OBFUSCATE_W(L"Aimbot target")), nullptr, nullptr, 0.0f, 0.0f, nullptr, &aimbotTargetIndex, aimbotTargetOptions });

    menuItems.push_back({ MenuItemType::Toggle, L"Triggerbot", &triggerbotEnabled });
    menuItems.push_back({ MenuItemType::Selector, L"Triggerbot Hotkey", nullptr, nullptr, 0.0f, 0.0f, nullptr, &triggerbotHotkeyIndex, hotkeyOptions });

    // FILTERS
    menuItems.push_back({ MenuItemType::Section, std::wstring(OBFUSCATE_W(L"=== FILTERS ===")), nullptr, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"LineofSight")), &lineOfSightCheck, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Disable Mission check")), &filterMissionOnly, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Disable Team check")), &filterTeammates, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Streamer Mode")), &streamerMode, nullptr, 0.0f, 0.0f, nullptr });

    // DEBUG
    menuItems.push_back({ MenuItemType::Section, std::wstring(OBFUSCATE_W(L"=== DEBUG ===")), nullptr, nullptr, 0.0f, 0.0f, nullptr });
    menuItems.push_back({ MenuItemType::Toggle, std::wstring(OBFUSCATE_W(L"Show Debug Info")), &showDebugInfo, nullptr, 0.0f, 0.0f, nullptr });

    FindTargetWindow();
    if (!targetHwnd) return false;

    // --- KERNEL DRIVER INITIALIZATION ---
    printf("[*] Initializing kernel driver bypass...\n");
    if (!KDM::Init(targetProcessId)) {
        printf("[-] KDM Driver not found! Ensure APBDriver.sys is loaded.\n");
        printf("[*] Attempting to continue without kernel driver (limited functionality)...\n");
        // Can continue without driver but with limited capabilities
    }

    // Get base address via kernel driver (more reliable than user-mode)
    uintptr_t baseFromDriver = KDM::GetBaseAddress(targetProcessId);
    if (baseFromDriver) {
        gameBaseAddress = baseFromDriver;
        printf("[+] Base address resolved via kernel driver: 0x%p\n", (void*)gameBaseAddress);
    } else {
        // Fallback to user-mode method
        printf("[*] Falling back to user-mode base address resolution...\n");
        ModuleInfo modInfo = GetModuleInfo(L"APB.exe");
        if (!modInfo.baseAddress) {
            printf("[-] Failed to resolve APB.exe base address!\n");
            return false;
        }
        gameBaseAddress = modInfo.baseAddress;
        printf("[+] Base address resolved via user-mode: 0x%p\n", (void*)gameBaseAddress);
    }
    isTargetFound = true;
    // ----------------------------------

    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = randomWindowName.c_str();
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    if (!RegisterClassEx(&wc)) return false;

    RECT targetRect = { 0, 0, 1920, 1080 };
    if (isTargetFound) {
        GetClientRect(targetHwnd, &targetRect);
        MapWindowPoints(targetHwnd, NULL, (LPPOINT)&targetRect, 2);
    }

    overlayHwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
        randomWindowName.c_str(), randomWindowName.c_str(),
        WS_POPUP,
        targetRect.left, targetRect.top, targetRect.right - targetRect.left, targetRect.bottom - targetRect.top,
        NULL, NULL, hInstance, NULL
    );

    if (!overlayHwnd) return false;

    SetLayeredWindowAttributes(overlayHwnd, 0, 255, LWA_ALPHA);
    MARGINS margins = { -1 };
    DwmExtendFrameIntoClientArea(overlayHwnd, &margins);

    if (streamerMode) SetWindowDisplayAffinity(overlayHwnd, WDA_EXCLUDEFROMCAPTURE);

    ShowWindow(overlayHwnd, SW_SHOW);

    if (!CreateDeviceResources()) {
        MessageBoxA(NULL, OBFUSCATE("Failed to create Direct2D resources."), OBFUSCATE("Error"), MB_ICONERROR);
        return false;
    }

    InitializeSignatures();

    // Execute the Kernel-Mode Signature Scan
    bool scanSuccess = ScanGameSignatures();
    PrintAddresses();

    if (!scanSuccess) {
        printf("[-] Failed to find all required signatures!\n");
    }

    return true;
}

void ExternalOverlay::Shutdown() {
    isRunning = false;
    if (dataUpdateThread.joinable()) {
        dataUpdateThread.join();
    }
    DiscardDeviceResources();
    if (d2dFactory) { d2dFactory->Release(); d2dFactory = nullptr; }
    if (writeFactory) { writeFactory->Release(); writeFactory = nullptr; }
    if (overlayHwnd) { DestroyWindow(overlayHwnd); overlayHwnd = nullptr; }
    if (targetProcessHandle) { CloseHandle(targetProcessHandle); targetProcessHandle = nullptr; }
}

ModuleInfo ExternalOverlay::GetModuleInfo(const TCHAR* modName) {
    ModuleInfo modInfo = {};
    std::wcout << L"Searching for module: " << modName << std::endl;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, targetProcessId);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 modEntry;
        modEntry.dwSize = sizeof(modEntry);
        if (Module32First(hSnap, &modEntry)) {
            do {
                if (!_tcsicmp(modEntry.szModule, modName)) {
                    modInfo.baseAddress = (uintptr_t)modEntry.modBaseAddr;
                    modInfo.moduleSize = modEntry.modBaseSize;
                    std::cout << "Module found. Base Address: 0x" << std::hex << modInfo.baseAddress << ", Size: " << std::dec << modInfo.moduleSize << std::endl;
                    break;
                }
            } while (Module32Next(hSnap, &modEntry));
        }
    }
    CloseHandle(hSnap);
    if (modInfo.baseAddress == 0) {
        std::cout << "Module not found." << std::endl;
    }
    return modInfo;
}

static uint8_t GetByte(const char* hex)
{
    return static_cast<uint8_t>((hexdigits[hex[0]] << 4) | (hexdigits[hex[1]]));
}

uintptr_t ExternalOverlay::FindSignature(const char* signature) {
    if (!targetProcessId || !gameBaseAddress) return 0;

    DWORD modSize = 0x6000000;
    std::vector<BYTE> moduleData(modSize);

    // Use kernel driver for memory reading if available
    if (KDM::Available()) {
        printf("[*] Reading module data via kernel driver...\n");
        for (DWORD i = 0; i < modSize; i += 4096) {
            SIZE_T bytesRead = 0;
            if (!KDM::ReadMemory(targetProcessId, gameBaseAddress + i, &moduleData[i], 4096)) {
                printf("[-] Failed to read chunk at offset 0x%X\n", i);
                break;
            }
        }
    } else {
        // Fallback to user-mode method
        printf("[*] Reading module data via user-mode API...\n");
        for (DWORD i = 0; i < modSize; i += 4096) {
            SIZE_T bytesRead = 0;
            if (!ReadProcessMemory(targetProcessHandle, (LPCVOID)(gameBaseAddress + i), &moduleData[i], 4096, &bytesRead)) {
                if (bytesRead == 0) break;
            }
        }
    }

    std::vector<BYTE> pattern;
    std::vector<BYTE> mask;
    std::istringstream iss(signature);
    std::string bStr;
    while (iss >> bStr) {
        if (bStr == "?") {
            pattern.push_back(0); mask.push_back(0);
        }
        else {
            pattern.push_back(static_cast<BYTE>(strtoul(bStr.c_str(), nullptr, 16)));
            mask.push_back(0xFF);
        }
    }

    for (size_t i = 0; i < moduleData.size() - pattern.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); ++j) {
            if (mask[j] != 0 && moduleData[i + j] != pattern[j]) {
                match = false; break;
            }
        }
        if (match) {
            printf("[+] Signature found at offset: 0x%zX\n", i);
            return gameBaseAddress + i;
        }
    }
    return 0;
}

void ExternalOverlay::FindTargetWindow() {
    std::cout << "Searching for target window 'APB Reloaded'..." << std::endl;
    targetHwnd = FindWindowW(NULL, L"APB Reloaded");

    if (targetHwnd) {
        std::cout << "Window found. Handle: 0x" << std::hex << (uintptr_t)targetHwnd << std::dec << std::endl;
        GetWindowThreadProcessId(targetHwnd, &targetProcessId);

        if (targetProcessId) {
            std::cout << "Process ID found: " << targetProcessId << std::endl;
            targetProcessHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetProcessId);

            if (targetProcessHandle) {
                std::cout << "Process context acquired." << std::endl;
            }
            else {
                std::cout << "[-] Failed to acquire process context. Error: " << GetLastError() << std::endl;
            }
        }
    }
    else {
        std::cout << "[-] Target window not found." << std::endl;
    }
}

bool ExternalOverlay::CreateDeviceResources() {
    if (renderTarget) return true;

    HRESULT hr = S_OK;
    if (!d2dFactory) {
        hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        if (FAILED(hr)) return false;
    }
    if (!writeFactory) {
        hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&writeFactory));
        if (FAILED(hr)) return false;
    }

    IDWriteTextFormat* textFormat = nullptr;
    hr = writeFactory->CreateTextFormat(L"Arial", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"", &textFormat);
    if (FAILED(hr)) return false;

    RECT rc;
    GetClientRect(overlayHwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);

    hr = d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(overlayHwnd, size),
        &renderTarget
    );
    if (FAILED(hr)) {
        textFormat->Release();
        return false;
    }

    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &textBrush);
    if (FAILED(hr)) {
        textFormat->Release();
        return false;
    }

    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 0.55f, 0.0f, 1.0f), &selectedTextBrush);
    if (FAILED(hr)) {
        textFormat->Release();
        return false;
    }

    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.5f), &menuBackgroundBrush);
    if (FAILED(hr)) {
        textFormat->Release();
        return false;
    }

    hr = renderTarget->CreateSolidColorBrush(boxESPColor, &boxBrush);
    if (FAILED(hr)) {
        textFormat->Release();
        return false;
    }

    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LimeGreen), &visibleBrush);
    if (FAILED(hr)) return false;

    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.09f, 0.11f, 0.16f, 0.95f), &modernMenuBgBrush);
    if (FAILED(hr)) return false;
    
    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.27f, 0.31f, 0.38f, 0.8f), &modernMenuBorderBrush);
    if (FAILED(hr)) return false;
    
    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.12f, 0.14f, 0.19f, 0.9f), &modernMenuItemBgBrush);
    if (FAILED(hr)) return false;
    
    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.18f, 0.21f, 0.27f, 0.8f), &modernMenuItemHoverBrush);
    if (FAILED(hr)) return false;
    
    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.44f, 0.74f, 1.0f, 1.0f), &modernMenuSectionBrush);
    if (FAILED(hr)) return false;
    
    hr = renderTarget->CreateSolidColorBrush(D2D1::ColorF(0.61f, 0.85f, 0.42f, 1.0f), &modernMenuAccentBrush);
    if (FAILED(hr)) return false;

    hr = writeFactory->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        10.0f,
        L"en-us",
        &smallTextFormat
    );
    if (FAILED(hr)) {
        textFormat->Release();
        return false;
    }

    D2D1_STROKE_STYLE_PROPERTIES outlineProperties = {};
    outlineProperties.startCap = D2D1_CAP_STYLE_ROUND;
    outlineProperties.endCap = D2D1_CAP_STYLE_ROUND;
    outlineProperties.dashCap = D2D1_CAP_STYLE_ROUND;
    outlineProperties.lineJoin = D2D1_LINE_JOIN_ROUND;
    outlineProperties.miterLimit = 1.0f;
    outlineProperties.dashStyle = D2D1_DASH_STYLE_SOLID;
    outlineProperties.dashOffset = 0.0f;

    hr = d2dFactory->CreateStrokeStyle(
        outlineProperties,
        nullptr,
        0,
        &textOutlineStyle
    );
    if (FAILED(hr)) {
        textFormat->Release();
        if (smallTextFormat) smallTextFormat->Release();
        return false;
    }

    textFormat->Release();
    return true;
}

void ExternalOverlay::DiscardDeviceResources() {
    if (textBrush) { textBrush->Release(); textBrush = nullptr; }
    if (selectedTextBrush) { selectedTextBrush->Release(); selectedTextBrush = nullptr; }
    if (menuBackgroundBrush) { menuBackgroundBrush->Release(); menuBackgroundBrush = nullptr; }
    if (boxBrush) { boxBrush->Release(); boxBrush = nullptr; }
    if (visibleBrush) { visibleBrush->Release(); visibleBrush = nullptr; }
    if (modernMenuBgBrush) { modernMenuBgBrush->Release(); modernMenuBgBrush = nullptr; }
    if (modernMenuBorderBrush) { modernMenuBorderBrush->Release(); modernMenuBorderBrush = nullptr; }
    if (modernMenuItemBgBrush) { modernMenuItemBgBrush->Release(); modernMenuItemBgBrush = nullptr; }
    if (modernMenuItemHoverBrush) { modernMenuItemHoverBrush->Release(); modernMenuItemHoverBrush = nullptr; }
    if (modernMenuSectionBrush) { modernMenuSectionBrush->Release(); modernMenuSectionBrush = nullptr; }
    if (modernMenuAccentBrush) { modernMenuAccentBrush->Release(); modernMenuAccentBrush = nullptr; }
    if (smallTextFormat) {
        smallTextFormat->Release();
        smallTextFormat = nullptr;
    }
    if (textOutlineStyle) {
        textOutlineStyle->Release();
        textOutlineStyle = nullptr;
    }
    if (renderTarget) { renderTarget->Release(); renderTarget = nullptr; }
    if (writeFactory) { writeFactory->Release(); writeFactory = nullptr; }
    if (d2dFactory) { d2dFactory->Release(); d2dFactory = nullptr; }
}

void ExternalOverlay::HandleInput() {
    RunTriggerbot();

    static bool bindingHotkey = false;
    if (GetAsyncKeyState(VK_INSERT) & 1) {
        ToggleMenu();
    }
    if (!menuVisible) return;


    auto findNextSelectable = [&](int dir) -> int {
        int idx = selectedMenuItem;
        for (size_t i = 0; i < menuItems.size(); ++i) {
            idx = (idx + dir + menuItems.size()) % menuItems.size();
            if (menuItems[idx].type != MenuItemType::Section)
                return idx;
        }
        return selectedMenuItem;
    };

    if (GetAsyncKeyState(VK_UP) & 1) {
        selectedMenuItem = findNextSelectable(-1);
    }
    if (GetAsyncKeyState(VK_DOWN) & 1) {
        selectedMenuItem = findNextSelectable(1);
    }
    if (menuItems[selectedMenuItem].type == MenuItemType::Toggle && (GetAsyncKeyState(VK_RETURN) & 1 || GetAsyncKeyState(0x45) & 1)) {
        if (menuItems[selectedMenuItem].value) {
            *menuItems[selectedMenuItem].value = !(*menuItems[selectedMenuItem].value);
            // Handle streamer mode change
            if (menuItems[selectedMenuItem].value == &streamerMode) {
                SetWindowDisplayAffinity(overlayHwnd, streamerMode ? 0x00000011 : 0x00000000);
            }
        }
    }
    if (menuItems[selectedMenuItem].type == MenuItemType::Slider) {
        if (GetAsyncKeyState(VK_LEFT) & 1) {
            if (menuItems[selectedMenuItem].floatValue) {
                *menuItems[selectedMenuItem].floatValue -= 1.0f;
                if (*menuItems[selectedMenuItem].floatValue < menuItems[selectedMenuItem].minValue)
                    *menuItems[selectedMenuItem].floatValue = menuItems[selectedMenuItem].minValue;
            }
        }
        if (GetAsyncKeyState(VK_RIGHT) & 1) {
            if (menuItems[selectedMenuItem].floatValue) {
                *menuItems[selectedMenuItem].floatValue += 1.0f;
                if (*menuItems[selectedMenuItem].floatValue > menuItems[selectedMenuItem].maxValue)
                    *menuItems[selectedMenuItem].floatValue = menuItems[selectedMenuItem].maxValue;
            }
        }
    }
    if (menuItems[selectedMenuItem].type == MenuItemType::Selector) {
        bool changed = false;
        if (GetAsyncKeyState(VK_RIGHT) & 1) {
            if (menuItems[selectedMenuItem].selectedIndex && !menuItems[selectedMenuItem].options.empty()) {
                *menuItems[selectedMenuItem].selectedIndex = (*menuItems[selectedMenuItem].selectedIndex + 1) % menuItems[selectedMenuItem].options.size();
                changed = true;
            }
        }
        if (GetAsyncKeyState(VK_LEFT) & 1) {
            if (menuItems[selectedMenuItem].selectedIndex && !menuItems[selectedMenuItem].options.empty()) {
                *menuItems[selectedMenuItem].selectedIndex = (*menuItems[selectedMenuItem].selectedIndex - 1 + menuItems[selectedMenuItem].options.size()) % menuItems[selectedMenuItem].options.size();
                changed = true;
            }
        }


        if (changed && menuItems[selectedMenuItem].selectedIndex == &aimbotHotkeyIndex) {
            aimbotHotkey = availableHotkeys[*menuItems[selectedMenuItem].selectedIndex].second;
        }
    }
}

void ExternalOverlay::Draw() {
    if (!CreateDeviceResources()) {
        return;
    }

    std::vector<PlayerData> players_copy;
    PlayerData localPlayer_copy;
    std::vector<GrenadeData> grenades_copy;
    std::wstring debugPlayerPos_copy, debugCameraPos_copy, debugViewMatrix_copy;

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        players_copy = players;
        localPlayer_copy = localPlayer;
        grenades_copy = grenades;
        
        if (showDebugInfo) {
            debugPlayerPos_copy = this->debugPlayerPos;
            // debugCameraPos_copy is now handled locally in Draw
        }
    }


    Camera camera_copy = this->GetLocalCamera();
    float viewMatrix_copy[16];
    bool viewMatrixReadSuccess_copy = this->ReadViewMatrix(viewMatrix_copy);


    if (viewMatrixReadSuccess_copy) {
        camera_copy.yaw = atan2f(viewMatrix_copy[2], viewMatrix_copy[0]) * (180.0f / 3.1415926535f);
        if (camera_copy.yaw < 0.0f) {
            camera_copy.yaw += 360.0f;
        }
        camera_copy.pitch = atan2f(viewMatrix_copy[9], sqrtf(viewMatrix_copy[8] * viewMatrix_copy[8] + viewMatrix_copy[10] * viewMatrix_copy[10])) * (180.0f / 3.1415926535f);

        if (showDebugInfo) {
            std::wstringstream cam_ss;
            cam_ss << std::fixed << std::setprecision(2);
            cam_ss << L"Camera: " << camera_copy.x << L", " << camera_copy.y << L", " << camera_copy.z;
            debugCameraPos_copy = cam_ss.str();
        }
    
        if (showDebugInfo) {
            std::wstringstream ss;
            ss << std::fixed << std::setprecision(2);
            ss << L"View Matrix: ";
            for (int i = 0; i < 16; i++) {
                ss << viewMatrix_copy[i];
                if (i < 15) ss << L", ";
                if ((i + 1) % 4 == 0 && i < 15) ss << L"\n";
            }
            debugViewMatrix_copy = ss.str();
        }
    }

    // --- Rendering ---
    renderTarget->BeginDraw();
    renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    if (menuVisible) {
        DrawMenu(180, 10, menuItems);
    }

    if (isTargetFound) {
        // Draw radar if enabled
        if (showRadar) {
            DrawRadar(players_copy, localPlayer_copy, camera_copy);
        }

        // --- "Looking At You" Warning System ---
        std::set<uintptr_t> lookingDirectlyPlayers;
        std::set<uintptr_t> lookingAroundPlayers;

        if (smallTextFormat && boxBrush) { // Check for the correct text format

            Vec3 localPlayerHeadPos = GetBoneWorldPosition(localPlayer_copy.pawnAddress, 6); // Head bone

            if ((localPlayerHeadPos.x != 0 || localPlayerHeadPos.y != 0 || localPlayerHeadPos.z != 0) && localPlayer_copy.pawnAddress != 0) {
                for (const auto& enemyPlayer : players_copy) {
                    // --- Start of filtering logic ---
                    if (enemyPlayer.pawnAddress == 0 || enemyPlayer.pawnAddress == localPlayer_copy.pawnAddress) {
                        continue; // Skip invalid or self
                    }

                    if (enemyPlayer.isDead) {
                        continue;
                    }

                    // Apply mission and teammate filters (only if we are alive and have data)
                    if (localPlayer_copy.found && !localPlayer_copy.isDead) {
                        // Mission filter
                        if (filterMissionOnly && enemyPlayer.missionId != localPlayer_copy.missionId) {
                            continue;
                        }
                        // Teammate filter (by faction)
                        if (filterTeammates && enemyPlayer.faction == localPlayer_copy.faction) {
                            continue;
                        }
                    }


                    // Convert enemy's rotation from game units to degrees for FOV check
                    int yawInt = Read<int>(enemyPlayer.pawnAddress + 0x890);
                    int pitchInt = Read<int>(enemyPlayer.pawnAddress + 0x88C);
                    float enemyYaw = (static_cast<float>(yawInt) * 360.0f / 65536.0f);
                    float enemyPitch = (static_cast<float>(pitchInt) * 360.0f / 65536.0f);

                    Vec3 enemyHeadPos = GetBoneWorldPosition(enemyPlayer.pawnAddress, 6); 
                    if (enemyHeadPos.x == 0 && enemyHeadPos.y == 0 && enemyHeadPos.z == 0) {
                        continue; 
                    }

                    if (IsPointInFOV(localPlayerHeadPos, enemyHeadPos, enemyYaw, enemyPitch, 7.5f)) {
                        lookingDirectlyPlayers.insert(enemyPlayer.pawnAddress);
                    } 
                    else if (IsPointInFOV(localPlayerHeadPos, enemyHeadPos, enemyYaw, enemyPitch, 90.0f)) {
                        lookingAroundPlayers.insert(enemyPlayer.pawnAddress);
                    }
                }
            }

            wchar_t warningText[128];
            D2D1_COLOR_F textColor;
            bool shouldDraw = false;

            if (lookingDirectlyPlayers.size() > 0) {
                swprintf_s(warningText, L"%zu Player(s) Looking Directly at you", lookingDirectlyPlayers.size());
                textColor = D2D1::ColorF(D2D1::ColorF::Red);
                shouldDraw = true;
            } else if (lookingAroundPlayers.size() > 0) {
                swprintf_s(warningText, L"%zu Player(s) Looking around you", lookingAroundPlayers.size());
                textColor = D2D1::ColorF(D2D1::ColorF::White);
                shouldDraw = true;
            }


            DrawPlayerESP(players_copy, localPlayer_copy, camera_copy, viewMatrix_copy, viewMatrixReadSuccess_copy, lookingDirectlyPlayers);


            if (grenadeESP) {
                DrawGrenadeESP(grenades_copy, camera_copy, viewMatrix_copy, viewMatrixReadSuccess_copy);
            }


            if (shouldDraw) {
                D2D1_SIZE_F screenSize = renderTarget->GetSize();
                D2D1_RECT_F textRect = D2D1::RectF(0, screenSize.height - 60, screenSize.width, screenSize.height - 30);
                boxBrush->SetColor(textColor);
                renderTarget->DrawTextW(
                    warningText,
                    static_cast<UINT32>(wcslen(warningText)),
                    smallTextFormat,
                    &textRect,
                    boxBrush
                );
            }
        }

        // FOV Circle
        if (showFov && renderTarget && boxBrush) {
            RECT rc; GetClientRect(overlayHwnd, &rc);
            float width = static_cast<float>(rc.right - rc.left);
            float height = static_cast<float>(rc.bottom - rc.top);
            float cx = width / 2.0f, cy = height / 2.0f;
            float radius = tanf((fovSize * 0.5f) * (3.1415926535f / 180.0f)) * (width / 2.0f);
            boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
            renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), boxBrush, 2.0f);
        }

        // Aimbot FOV Check
        RunAimbot(players_copy, localPlayer_copy, camera_copy, viewMatrix_copy, viewMatrixReadSuccess_copy);

        // Debug Info
        if (showDebugInfo && renderTarget && textBrush && smallTextFormat) {
            float yPos = 50.0f;
            const float lineHeight = 20.0f;

            if (!debugPlayerPos_copy.empty()) {
                renderTarget->DrawTextW(debugPlayerPos_copy.c_str(), (UINT32)debugPlayerPos_copy.length(), smallTextFormat, D2D1::RectF(10.0f, yPos, 800.0f, yPos + lineHeight), textBrush);
                yPos += lineHeight;
            }

            if (!debugCameraPos_copy.empty()) {
                renderTarget->DrawTextW(debugCameraPos_copy.c_str(), (UINT32)debugCameraPos_copy.length(), smallTextFormat, D2D1::RectF(10.0f, yPos, 800.0f, yPos + lineHeight), textBrush);
                yPos += lineHeight;
            }

            wchar_t crosshairBuffer[128];
            swprintf_s(crosshairBuffer, L"Crosshair ClassID: %d", debugCrosshairClassId);
            renderTarget->DrawTextW(crosshairBuffer, (UINT32)wcslen(crosshairBuffer), smallTextFormat, D2D1::RectF(10.0f, yPos, 800.0f, yPos + lineHeight), textBrush);
            yPos += lineHeight;

            if (!debugGrenadeInfo.empty()) {
                renderTarget->DrawTextW(debugGrenadeInfo.c_str(), (UINT32)debugGrenadeInfo.length(), smallTextFormat, D2D1::RectF(10.0f, yPos, 600.0f, yPos + lineHeight), textBrush);
                yPos += lineHeight;
            }

            if (!debugViewMatrix_copy.empty()) {
                std::wistringstream iss(debugViewMatrix_copy);
                std::wstring line;
                while (std::getline(iss, line)) {
                    if (!line.empty()) {
                        renderTarget->DrawTextW(line.c_str(), (UINT32)line.length(), smallTextFormat, D2D1::RectF(10.0f, yPos, 1000.0f, yPos + lineHeight), textBrush);
                        yPos += lineHeight;
                    }
                }
            }
        }
    }

    HRESULT hr = renderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
    }
}

Vec3 ExternalOverlay::GetBoneWorldPosition(uintptr_t pawnAddress, int boneId) {
    if (!targetProcessHandle || pawnAddress == 0) return { 0, 0, 0 };

    uintptr_t mesh = Read<uintptr_t>(pawnAddress + 0x4E8);
    if (!mesh) return { 0, 0, 0 };

    float componentToWorld[16];
    if (!ReadProcessMemory(targetProcessHandle, (LPCVOID)(mesh + 0xA0), &componentToWorld, sizeof(componentToWorld), NULL)) {
        return { 0, 0, 0 };
    }

    uintptr_t boneBaseMatrix = Read<uintptr_t>(mesh + 0x2BC);
    if (!boneBaseMatrix) return { 0, 0, 0 };

    float boneMatrix[16];
    if (!ReadProcessMemory(targetProcessHandle, (LPCVOID)(boneBaseMatrix + boneId * 64), &boneMatrix, sizeof(boneMatrix), NULL)) {
        return { 0, 0, 0 };
    }

    Vec3 bonePos(boneMatrix[12], boneMatrix[13], boneMatrix[14]);

    Vec3 worldPos(
        bonePos.x * componentToWorld[0] + bonePos.y * componentToWorld[4] + bonePos.z * componentToWorld[8] + componentToWorld[12],
        bonePos.x * componentToWorld[1] + bonePos.y * componentToWorld[5] + bonePos.z * componentToWorld[9] + componentToWorld[13],
        bonePos.x * componentToWorld[2] + bonePos.y * componentToWorld[6] + bonePos.z * componentToWorld[10] + componentToWorld[14]
    );
    return worldPos;
}

void ExternalOverlay::RunTriggerbot() {

    debugCrosshairClassId = 0;


    int currentTriggerbotHotkey = availableHotkeys[triggerbotHotkeyIndex].second;

    if (!triggerbotEnabled || (GetAsyncKeyState(currentTriggerbotHotkey) & 0x8000) == 0) {
        return;
    }


    uintptr_t localPlayerController = this->localPlayerControllerAddress;
    if (!localPlayerController) { debugCrosshairClassId = -3; return; }

    uintptr_t crosshairTarget = Read<uintptr_t>(localPlayerController + 0x910);
    if (!crosshairTarget || IsBadReadPtr((void*)crosshairTarget, 0xBD0 + sizeof(float))) { 
        debugCrosshairClassId = -4; // No target or bad pointer
        return;
    }


    uintptr_t uclass = Read<uintptr_t>(crosshairTarget + 0x18);
    if (!uclass || IsBadReadPtr((void*)uclass, 0x24)) {
        debugCrosshairClassId = -5; 
        return;
    }


    int classId = Read<int>(uclass + 0x20);
    debugCrosshairClassId = classId; 


    if (classId == 415) {
        uintptr_t localPawnAddress = 0;
        {
            
            std::lock_guard<std::mutex> lock(this->dataMutex);
            localPawnAddress = this->localPlayer.pawnAddress;
        }

        if (localPawnAddress == 0) return; // Don't fire if local pawn is not valid


        float crosshairSpread = Read<float>(localPawnAddress + 0xBD0);

        if (crosshairSpread <= 1.0f) {

            INPUT input = { 0 };
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));


            Sleep(10);

            ZeroMemory(&input, sizeof(INPUT));
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
        }
    }
}

void ExternalOverlay::RunAimbot(const std::vector<PlayerData>& players, const PlayerData& localPlayer, const Camera& camera, const float* viewMatrix, bool viewMatrixReadSuccess) {
    if (!this->aimbotEnabled || this->aimbotHotkey == 0 || !viewMatrixReadSuccess) {
        this->aimingActive = false; 
        return;
    }

    bool hotkeyDown = (GetAsyncKeyState(this->aimbotHotkey) & 0x8000);
    if (!hotkeyDown) {
        this->persistentTargetPawn = 0; 
        this->aimingActive = false;
        return;
    }

    if (players.empty() || !localPlayer.found || localPlayer.isDead) {
        this->aimingActive = false;
        return;
    }

    RECT rc;
    GetClientRect(this->overlayHwnd, &rc);
    int width = rc.right - rc.left;
    int height = rc.bottom - rc.top;
    float cx = static_cast<float>(width) / 2.0f;
    float cy = static_cast<float>(height) / 2.0f;

    const PlayerData* target = nullptr;


    if (this->persistentTargetPawn != 0) {
        bool targetStillValid = false;
        for (const auto& p : players) {
            if (p.pawnAddress == this->persistentTargetPawn) {
              
                bool isEnemy = (p.faction != localPlayer.faction);
                
               
                bool isMissionTarget = true;
                if (this->filterMissionOnly) {
                    isMissionTarget = (localPlayer.missionId != 0 && p.missionId != 0 && localPlayer.missionId == p.missionId);
                }
                
                if (!p.isDead && isEnemy && isMissionTarget) {
                    target = &p;
                    targetStillValid = true;
                }
                break;
            }
        }
        if (!targetStillValid) {
            this->persistentTargetPawn = 0; 
        }
    }


    if (this->persistentTargetPawn == 0) {
        float closestDist = FLT_MAX;
        const PlayerData* bestTarget = nullptr;

        for (const auto& p : players) {
            if (!p.found || p.isDead || p.pawnAddress == localPlayer.pawnAddress) continue;

            // Standard filters
            bool isEnemy = (p.faction != localPlayer.faction);
            

            bool isMissionTarget = true;
            if (this->filterMissionOnly) {

                isMissionTarget = (localPlayer.missionId != 0 && p.missionId != 0 && localPlayer.missionId == p.missionId);
            }
            
            if (!isEnemy || !isMissionTarget) continue;

            // Line of Sight filter
            if (this->lineOfSightCheck && !p.isVisible) {
                continue;
            }

            // FOV filter
            D2D1_POINT_2F screen;
            Vec3 camPos(camera.x, camera.y, camera.z);
            if (!this->WorldToScreen((float*)viewMatrix, Vec3(p.x, p.y, p.z), camPos, screen, width, height)) continue;

            float dx_fov = screen.x - cx;
            float dy_fov = screen.y - cy;
            float dist_fov = sqrtf(dx_fov * dx_fov + dy_fov * dy_fov);
            float maxFovPx = tanf((this->fovSize * 0.5f) * (3.1415926535f / 180.0f)) * (static_cast<float>(width) / 2.0f);

            if (dist_fov > maxFovPx) {
                continue; // Skip if not in FOV
            }

            // Distance check
            float dx_world = localPlayer.x - p.x;
            float dy_world = localPlayer.y - p.y;
            float dz_world = localPlayer.z - p.z;
            float worldDistance = sqrtf(dx_world * dx_world + dy_world * dy_world + dz_world * dz_world);

            if (worldDistance < closestDist) {
                closestDist = worldDistance;
                bestTarget = &p;
            }
        }
        
        target = bestTarget;
        if (target) {
            this->persistentTargetPawn = target->pawnAddress; 
        }
    }

    if (!target) {
        this->aimingActive = false;
        return;
    }

    this->aimingActive = true;

    // --- Aiming Logic ---
    int boneId = 6; // Head
    if (this->aimbotTargetIndex == 1) boneId = 4; // Chest
    else if (this->aimbotTargetIndex == 2) boneId = 133; // Legs

    Vec3 targetPos = this->GetBoneWorldPosition(target->pawnAddress, boneId);
    if (targetPos.x == 0 && targetPos.y == 0 && targetPos.z == 0) {
        targetPos = Vec3(target->x, target->y, target->z);
    }

    D2D1_POINT_2F targetScreen;
    Vec3 camPos(camera.x, camera.y, camera.z);
    if (!this->WorldToScreen((float*)viewMatrix, targetPos, camPos, targetScreen, width, height)) return;

    float dx = targetScreen.x - cx;
    float dy = targetScreen.y - cy;
    float moveX = dx / this->smoothing;
    float moveY = dy / this->smoothing;

    if (fabs(moveX) < 1.0f && fabs(moveY) < 1.0f) return;

    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dx = static_cast<LONG>(moveX);
    input.mi.dy = static_cast<LONG>(moveY);
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

void ExternalOverlay::DrawMenu(int x, int y, const std::vector<MenuItem>& items) {
    if (items.empty() || !modernMenuBgBrush) return;


    RECT screenRect;
    GetClientRect(overlayHwnd, &screenRect);
    float screenWidth = static_cast<float>(screenRect.right - screenRect.left);
    float screenHeight = static_cast<float>(screenRect.bottom - screenRect.top);
    

    float menuWidth = min(320.0f, screenWidth * 0.25f); 
    float itemHeight = 28.0f;
    float sectionHeight = 32.0f;
    float padding = 8.0f;
    float menuHeight = padding;
    

    for (const auto& item : items) {
        menuHeight += (item.type == MenuItemType::Section) ? sectionHeight : itemHeight;
        menuHeight += 2.0f; 
    }
    menuHeight += padding + 45.0f; 
    

    if (y + menuHeight > screenHeight) {
        y = static_cast<int>(screenHeight - menuHeight - 10.0f);
    }


    IDWriteTextFormat* titleFormat = nullptr;
    IDWriteTextFormat* itemFormat = nullptr;
    IDWriteTextFormat* sectionFormat = nullptr;
    
    writeFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &itemFormat);
    writeFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &sectionFormat);
    writeFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &titleFormat);
    
    if (!itemFormat || !sectionFormat || !titleFormat) {
        if (itemFormat) itemFormat->Release();
        if (sectionFormat) sectionFormat->Release();
        if (titleFormat) titleFormat->Release();
        return;
    }


    D2D1_RECT_F backgroundRect = D2D1::RectF(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x) + menuWidth, static_cast<float>(y) + menuHeight);
    renderTarget->FillRectangle(&backgroundRect, modernMenuBgBrush);
    

    renderTarget->DrawRectangle(&backgroundRect, modernMenuBorderBrush, 3.0f);
    

    D2D1_RECT_F innerBorder = D2D1::RectF(backgroundRect.left + 2, backgroundRect.top + 2, backgroundRect.right - 2, backgroundRect.bottom - 2);
    renderTarget->DrawRectangle(&innerBorder, modernMenuBorderBrush, 1.5f);

    D2D1_RECT_F titleRect = D2D1::RectF(static_cast<float>(x) + padding + 3.0f, static_cast<float>(y) + 8.0f, static_cast<float>(x) + menuWidth - padding - 3.0f, static_cast<float>(y) + 27.0f);
    renderTarget->DrawTextW(L"apb.one", 7, titleFormat, titleRect, modernMenuSectionBrush);
    
    D2D1_RECT_F titleUnderline = D2D1::RectF(static_cast<float>(x) + padding + 3.0f, static_cast<float>(y) + 30.0f, static_cast<float>(x) + menuWidth - padding - 3.0f, static_cast<float>(y) + 31.0f);
    renderTarget->FillRectangle(&titleUnderline, modernMenuBorderBrush);

    float currentY = static_cast<float>(y) + 37.0f;
    
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        bool isSelected = (static_cast<int>(i) == selectedMenuItem);
        float itemTop = currentY;
        float itemBottom = currentY + ((item.type == MenuItemType::Section) ? sectionHeight : itemHeight);
        
        if (item.type == MenuItemType::Section) {
            D2D1_RECT_F sectionBg = D2D1::RectF(static_cast<float>(x) + 11.0f, itemTop - 2.0f, static_cast<float>(x) + menuWidth - 11.0f, itemBottom + 2.0f);
            renderTarget->FillRectangle(&sectionBg, modernMenuItemBgBrush);
            
            D2D1_RECT_F sectionRect = D2D1::RectF(static_cast<float>(x) + padding + 3.0f, itemTop + 8.0f, static_cast<float>(x) + menuWidth - padding - 3.0f, itemBottom - 8.0f);
            renderTarget->DrawTextW(item.text.c_str(), (UINT32)item.text.length(), sectionFormat, sectionRect, modernMenuSectionBrush);
            
            D2D1_RECT_F accentLine = D2D1::RectF(static_cast<float>(x) + padding + 3.0f, itemBottom - 6.0f, static_cast<float>(x) + menuWidth - padding - 3.0f, itemBottom - 4.0f);
            renderTarget->FillRectangle(&accentLine, modernMenuSectionBrush);
            
            currentY = itemBottom + 4.0f;
            continue;
        }

        D2D1_RECT_F itemBg = D2D1::RectF(static_cast<float>(x) + 11.0f, itemTop, static_cast<float>(x) + menuWidth - 11.0f, itemBottom);
        
        if (isSelected) {

            renderTarget->FillRectangle(&itemBg, modernMenuItemHoverBrush);
            renderTarget->DrawRectangle(&itemBg, modernMenuAccentBrush, 2.0f);
            

            D2D1_RECT_F indicator = D2D1::RectF(static_cast<float>(x) + 7.0f, itemTop + 8.0f, static_cast<float>(x) + 11.0f, itemBottom - 8.0f);
            renderTarget->FillRectangle(&indicator, modernMenuAccentBrush);
        } else {
            renderTarget->FillRectangle(&itemBg, modernMenuItemBgBrush);
        }

        D2D1_RECT_F textRect = D2D1::RectF(static_cast<float>(x) + padding + 11.0f, itemTop + 8.0f, static_cast<float>(x) + menuWidth - padding - 11.0f, itemBottom - 8.0f);
        ID2D1SolidColorBrush* textColor = isSelected ? modernMenuAccentBrush : textBrush;

        if (item.type == MenuItemType::Toggle) {

            std::wstring toggleText = item.text;
            renderTarget->DrawTextW(toggleText.c_str(), (UINT32)toggleText.length(), itemFormat, textRect, textColor);
            
            float switchX = static_cast<float>(x) + menuWidth - 45.0f;
            float switchY = itemTop + 8.0f;
            float switchWidth = 30.0f;
            float switchHeight = 12.0f;
            
            D2D1_RECT_F switchBg = D2D1::RectF(switchX, switchY, switchX + switchWidth, switchY + switchHeight);
            bool isOn = item.value && *item.value;
            
            renderTarget->FillRectangle(&switchBg, isOn ? modernMenuAccentBrush : modernMenuItemBgBrush);
            renderTarget->DrawRectangle(&switchBg, modernMenuBorderBrush, 1.0f);
            
            // Switch thumb
            float thumbX = isOn ? switchX + switchWidth - 10.0f : switchX + 2.0f;
            D2D1_ELLIPSE thumb = D2D1::Ellipse(D2D1::Point2F(thumbX + 4.0f, switchY + 6.0f), 4.0f, 4.0f);
            renderTarget->FillEllipse(&thumb, textBrush);
            
        } else if (item.type == MenuItemType::Slider) {
            wchar_t valueBuf[32];
            swprintf_s(valueBuf, L"%.1f", item.floatValue ? *item.floatValue : 0.0f);
            std::wstring sliderText = item.text;
            renderTarget->DrawTextW(sliderText.c_str(), (UINT32)sliderText.length(), itemFormat, textRect, textColor);
            
            D2D1_RECT_F valueRect = D2D1::RectF(static_cast<float>(x) + menuWidth - 50.0f, itemTop + 6.0f, static_cast<float>(x) + menuWidth - 8.0f, itemBottom - 6.0f);
            renderTarget->DrawTextW(valueBuf, (UINT32)wcslen(valueBuf), itemFormat, valueRect, modernMenuAccentBrush);
            
            float sliderX = static_cast<float>(x) + menuWidth - 100.0f;
            float sliderY = itemTop + 12.0f;
            float sliderWidth = 40.0f;
            float sliderHeight = 3.0f;
            
            D2D1_RECT_F sliderBg = D2D1::RectF(sliderX, sliderY, sliderX + sliderWidth, sliderY + sliderHeight);
            renderTarget->FillRectangle(&sliderBg, modernMenuItemBgBrush);
            
            float progress = (item.floatValue && item.maxValue > item.minValue) ? 
                ((*item.floatValue - item.minValue) / (item.maxValue - item.minValue)) : 0.0f;
            D2D1_RECT_F sliderProgress = D2D1::RectF(sliderX, sliderY, sliderX + (sliderWidth * progress), sliderY + sliderHeight);
            renderTarget->FillRectangle(&sliderProgress, modernMenuAccentBrush);
            
            float thumbX = sliderX + (sliderWidth * progress);
            D2D1_ELLIPSE sliderThumb = D2D1::Ellipse(D2D1::Point2F(thumbX, sliderY + 1.5f), 4.0f, 4.0f);
            renderTarget->FillEllipse(&sliderThumb, textBrush);
            renderTarget->DrawEllipse(&sliderThumb, modernMenuAccentBrush, 1.0f);
            
        } else if (item.type == MenuItemType::Selector) {
            std::wstring selectorText = item.text;
            renderTarget->DrawTextW(selectorText.c_str(), (UINT32)selectorText.length(), itemFormat, textRect, textColor);
            
            if (item.selectedIndex && !item.options.empty()) {
                int currentIndex = *item.selectedIndex;
                if (currentIndex >= 0 && static_cast<size_t>(currentIndex) < item.options.size()) {
                    D2D1_RECT_F selectionRect = D2D1::RectF(static_cast<float>(x) + menuWidth - 90.0f, itemTop + 6.0f, static_cast<float>(x) + menuWidth - 8.0f, itemBottom - 6.0f);
                    std::wstring selectionText = L"< " + item.options[currentIndex] + L" >";
                    renderTarget->DrawTextW(selectionText.c_str(), (UINT32)selectionText.length(), itemFormat, selectionRect, modernMenuAccentBrush);
                }
            }
        }
        
        currentY = itemBottom + 2.0f;
    }
}

void ExternalOverlay::ToggleMenu() {
    menuVisible = !menuVisible;
}

void ExternalOverlay::DataUpdateThread() {
    static uintptr_t lastGameInstance = 0;
    static Vec3 lastCameraPos = {0, 0, 0};
    
    while (this->isRunning) {
        if (this->isTargetFound) {
            // Map change detection
            if (uWorldAddress) {
                uintptr_t currentGameInstance = Read<uintptr_t>(uWorldAddress + 0x3AC);
                
                if (currentGameInstance != lastGameInstance && lastGameInstance != 0) {
                    cachedHudAddress = 0; // Invalidate HUD cache
                    lastPawnCacheUpdate = 0; // Force pawn cache update
                    lastGrenadeCacheUpdate = 0; // Force grenade cache update
                    lastGameInstance = currentGameInstance;
                } else if (lastGameInstance == 0) {
                    lastGameInstance = currentGameInstance;
                }
            }
            
            Camera currentCamera = GetLocalCamera();
            if (lastCameraPos.x != 0 || lastCameraPos.y != 0 || lastCameraPos.z != 0) {
                float distanceJump = static_cast<float>(sqrt(pow(currentCamera.x - lastCameraPos.x, 2) + 
                                        pow(currentCamera.y - lastCameraPos.y, 2) + 
                                        pow(currentCamera.z - lastCameraPos.z, 2)));

                if (distanceJump > 1000.0f) {
                    cachedHudAddress = 0; 
                }
            }
            lastCameraPos = {currentCamera.x, currentCamera.y, currentCamera.z};
            

            if (GetTickCount() - lastPawnCacheUpdate > 1000) { 
                UpdatePawnCache();
            }


            if (GetTickCount() - lastGrenadeCacheUpdate > 2000) { 
                UpdateGrenadeCache();
            }

            auto newPlayers = this->FindAllPlayers();
            auto newGrenades = this->FindAllGrenades();
            auto newSelfPlayer = this->GetSelfPlayer();


            std::lock_guard<std::mutex> lock(this->dataMutex);
            players = newPlayers;
            localPlayer = newSelfPlayer;
            this->localPlayerControllerAddress = newSelfPlayer.controllerAddress; 
            grenades = newGrenades;


            if (showDebugInfo) {
                std::wstringstream ss;
                ss << std::fixed << std::setprecision(2);
                ss << L"Player: " << newSelfPlayer.x << L", " << newSelfPlayer.y << L", " << newSelfPlayer.z;
                debugPlayerPos = ss.str();



                std::wstringstream gss;
                gss << L"Grenades cached: " << cachedGrenadeAddresses.size() << L" | visible: " << newGrenades.size();
                debugGrenadeInfo = gss.str();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

ExternalOverlay::Camera ExternalOverlay::GetLocalCamera() {
    Camera cam{};
    if (!uWorldAddress) return cam;

    uintptr_t gameInstance = Read<uintptr_t>(uWorldAddress + 0x3AC);
    if (!gameInstance) return cam;

    uintptr_t cameraObject = 0;
    uintptr_t cam_offsets[] = { 0x58, 0x60 };
    for (uintptr_t offset : cam_offsets) {
        cameraObject = Read<uintptr_t>(gameInstance + offset);
        if (cameraObject) {
            uintptr_t uclass = Read<uintptr_t>(cameraObject + 0x18);
            if (uclass) {
                int uclassid = Read<int>(uclass + 0x20);
                if (uclassid == 339) {  
                    if (!IsBadReadPtr((void*)cameraObject, 0x164 + sizeof(float))) {
                        break;  
                    }
                }
            }
        }
        cameraObject = 0;  
    }

    if (cameraObject) {
        char camBuffer[0x168];
        if (ReadProcessMemory(targetProcessHandle, (LPCVOID)cameraObject, camBuffer, sizeof(camBuffer), NULL)) {
            cam.x = *reinterpret_cast<float*>(camBuffer + 0x15C);
            cam.y = *reinterpret_cast<float*>(camBuffer + 0x160);
            cam.z = *reinterpret_cast<float*>(camBuffer + 0x164);
        }
    }

    return cam;
}

ExternalOverlay::PlayerData ExternalOverlay::GetPlayerFromController(uintptr_t controller) {
    PlayerData pd{};
    if (!controller) return pd;

    uintptr_t pawn = Read<uintptr_t>(controller + 0x308);
    if (!pawn || IsBadReadPtr((void*)pawn, 0xE00)) return pd;

    char pawnBuffer[0xE00];
    if (ReadProcessMemory(targetProcessHandle, (LPCVOID)pawn, pawnBuffer, sizeof(pawnBuffer), NULL)) {
        pd.pawnAddress = pawn;
        pd.x = *reinterpret_cast<float*>(pawnBuffer + 0x15C);
        pd.y = *reinterpret_cast<float*>(pawnBuffer + 0x160);
        pd.z = *reinterpret_cast<float*>(pawnBuffer + 0x164);
        pd.hp = *reinterpret_cast<int*>(pawnBuffer + 0x42C);
        pd.teamId = *reinterpret_cast<uint8_t*>(pawnBuffer + 0xCF0);
        pd.missionId = *reinterpret_cast<int*>(pawnBuffer + 0xDE4);
        pd.faction = *reinterpret_cast<uint8_t*>(pawnBuffer + 0x7FB);
        pd.rank = *reinterpret_cast<int*>(pawnBuffer + 0xCB8);
        pd.isDead = (*reinterpret_cast<uint8_t*>(pawnBuffer + 0x6FC) != 0);
        pd.gender = *reinterpret_cast<uint8_t*>(pawnBuffer + 0x7FC);
        pd.found = true;
    }
    return pd;
}

ExternalOverlay::PlayerData ExternalOverlay::GetSelfPlayer() {
    DWORD64 currentTime = GetTickCount64();
    bool needToFindNewOffset = false;

    if (cachedControllerOffset == 0 || (currentTime - lastControllerOffsetUpdate) >= 5000) { 
        needToFindNewOffset = true;
    }

    uintptr_t controllerPtr = 0;

    if (needToFindNewOffset || lastControllerPtr == 0) {
        if (!uWorldAddress) return {};

        uintptr_t gameInstance = Read<uintptr_t>(uWorldAddress + 0x3AC);
        if (!gameInstance) return {};

        const int numOffsets = 3;
        uintptr_t offsetsToCheck[numOffsets] = { 0x98, 0xA0, 0xA8 };
        bool foundValidController = false;

        if (cachedControllerOffset != 0) {
            controllerPtr = Read<uintptr_t>(gameInstance + cachedControllerOffset);
            if (controllerPtr && !IsBadReadPtr((void*)controllerPtr, 0x20 + 4)) {
                uintptr_t uclass = Read<uintptr_t>(controllerPtr + 0x18);
                if (uclass && !IsBadReadPtr((void*)uclass, 0x20 + 4)) {
                    if (Read<int>(uclass + 0x20) == 395) {
                        foundValidController = true;
                    }
                }
            }
        }

        if (!foundValidController) {
            for (int i = 0; i < numOffsets; i++) {
                controllerPtr = Read<uintptr_t>(gameInstance + offsetsToCheck[i]);
                if (!controllerPtr || IsBadReadPtr((void*)controllerPtr, 0x20 + 4)) continue;

                uintptr_t uclass = Read<uintptr_t>(controllerPtr + 0x18);
                if (!uclass || IsBadReadPtr((void*)uclass, 0x20 + 4)) continue;

                if (Read<int>(uclass + 0x20) == 395) {
                    cachedControllerOffset = offsetsToCheck[i];
                    foundValidController = true;
                    break;
                }
            }
        }

        if (foundValidController) {
            lastControllerOffsetUpdate = currentTime;
        } else {
            cachedControllerOffset = 0;
            lastControllerPtr = 0;
            return {};
        }
        lastControllerPtr = controllerPtr;
    } else {
        controllerPtr = lastControllerPtr;
    }

    if (controllerPtr) {
        PlayerData pd = GetPlayerFromController(controllerPtr);
        pd.controllerAddress = controllerPtr; 
        if (pd.found && !pd.isDead) {
            lastKnownSelfPawn = pd.pawnAddress;
        }
        return pd;
    }

    return {};
}

void ExternalOverlay::UpdatePawnCache() {
    if (!uWorldAddress) return;

    uintptr_t gameInstance = Read<uintptr_t>(uWorldAddress + 0x3AC);
    if (!gameInstance) return;

    std::vector<uintptr_t> newPawnAddresses;
    for (uintptr_t offset = 0x400; offset < 0x2500; offset += 0x8) {
        uintptr_t pawn = Read<uintptr_t>(gameInstance + offset);
        if (!pawn || IsBadReadPtr((void*)pawn, 0x100)) {
            continue;
        }

        uintptr_t uclass = Read<uintptr_t>(pawn + 0x18);
        if (!uclass || IsBadReadPtr((void*)uclass, 0x20 + sizeof(int))) {
            continue;
        }

        if (Read<int>(uclass + 0x20) == 415) {
            newPawnAddresses.push_back(pawn);
        }
    }

    std::lock_guard<std::mutex> lock(dataMutex);
    cachedPawnAddresses = newPawnAddresses;
    lastPawnCacheUpdate = GetTickCount();
}

std::vector<ExternalOverlay::PlayerData> ExternalOverlay::FindAllPlayers() {
    std::vector<PlayerData> foundPlayers;
    std::vector<uintptr_t> currentPawns;

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        if (cachedPawnAddresses.empty()) return foundPlayers;
        currentPawns = cachedPawnAddresses;
    }

    char pawnBuffer[0xE00];
    for (uintptr_t pawn : currentPawns) {
        if (ReadProcessMemory(targetProcessHandle, (LPCVOID)pawn, pawnBuffer, sizeof(pawnBuffer), NULL)) {
            PlayerData pd = {};
            pd.pawnAddress = pawn;
            pd.x = *reinterpret_cast<float*>(pawnBuffer + 0x15C);
            pd.y = *reinterpret_cast<float*>(pawnBuffer + 0x160);
            pd.z = *reinterpret_cast<float*>(pawnBuffer + 0x164);
            pd.hp = *reinterpret_cast<int*>(pawnBuffer + 0x42C);
            pd.teamId = *reinterpret_cast<uint8_t*>(pawnBuffer + 0xCF0);
            pd.missionId = *reinterpret_cast<int*>(pawnBuffer + 0xDE4);
            pd.faction = *reinterpret_cast<uint8_t*>(pawnBuffer + 0x7FB);
            pd.rank = *reinterpret_cast<int*>(pawnBuffer + 0xCB8);
            pd.isDead = (*reinterpret_cast<uint8_t*>(pawnBuffer + 0x6FC) != 0);
            pd.gender = *reinterpret_cast<uint8_t*>(pawnBuffer + 0x7FC);
            pd.found = true;

            // Line of Sight Check
            float currentLosValue = *reinterpret_cast<float*>(pawnBuffer + 0x108);
            if (lastLosValues.find(pawn) != lastLosValues.end()) {
                if (lastLosValues[pawn] != currentLosValue) {
                    pd.isVisible = true;
                }
                else {
                    pd.isVisible = false;
                }
            } else {
                pd.isVisible = false; 
            }
            lastLosValues[pawn] = currentLosValue; // Update map

            foundPlayers.push_back(pd);
        }
    }

    std::map<uintptr_t, float> newLosValues;
    for (uintptr_t pawn : currentPawns) {
        if (lastLosValues.count(pawn)) {
            newLosValues[pawn] = lastLosValues[pawn];
        }
    }
    lastLosValues.swap(newLosValues);
    
    return foundPlayers;
}

bool ExternalOverlay::ReadViewMatrix(float* outMatrix) {
    if (cachedHudAddress) {
        if (!IsBadReadPtr((void*)(cachedHudAddress + 0x680), sizeof(float) * 16)) {
            if (ReadProcessMemory(targetProcessHandle, (LPCVOID)(cachedHudAddress + 0x680), outMatrix, sizeof(float) * 16, NULL)) {

                bool matrixValid = true;
                for (int i = 0; i < 16; i++) {
                    if (!isfinite(outMatrix[i]) || fabs(outMatrix[i]) > 1000000.0f) {
                        matrixValid = false;
                        break;
                    }
                }
                
                if (matrixValid) {
                    return true; 
                } else {
                    cachedHudAddress = 0; 
                }
            }
        }
        cachedHudAddress = 0;
    }

    if (!uWorldAddress) return false;
    uintptr_t gameInstance = Read<uintptr_t>(uWorldAddress + 0x3AC);
    if (!gameInstance) return false;

    for (uintptr_t offset = 0x0; offset <= 0x1000; offset += 0x8) {
        uintptr_t potentialHUD = Read<uintptr_t>(gameInstance + offset);
        if (!potentialHUD || IsBadReadPtr((void*)potentialHUD, 0x18 + sizeof(uintptr_t))) {
            continue;
        }

        uintptr_t uclass = Read<uintptr_t>(potentialHUD + 0x18);
        if (!uclass || IsBadReadPtr((void*)uclass, 0x20 + sizeof(int))) {
            continue;
        }

        if (Read<int>(uclass + 0x20) == 915) { 
            if (ReadProcessMemory(targetProcessHandle, (LPCVOID)(potentialHUD + 0x680), outMatrix, sizeof(float) * 16, NULL)) {

                cachedHudAddress = potentialHUD;
                return true;
            }
        }
    }

    return false;
}

float GetAngleBetweenVectors(const Vec3& v1, const Vec3& v2) {
    float dot = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    float mag1 = sqrtf(v1.x * v1.x + v1.y * v1.y + v1.z * v1.z);
    float mag2 = sqrtf(v2.x * v2.x + v2.y * v2.y + v2.z * v2.z);
    if (mag1 == 0.0f || mag2 == 0.0f) return 0.0f;
    float angleRad = acosf(dot / (mag1 * mag2));
    return angleRad * (180.0f / 3.1415926535f);
}

bool ExternalOverlay::IsPointInFOV(const Vec3& point, const Vec3& camPos, float camYaw, float camPitch, float fovDegrees) {
    Vec3 toPoint = point - camPos;

    float yawRad = camYaw * (3.1415926535f / 180.0f);
    float pitchRad = camPitch * (3.1415926535f / 180.0f);
    Vec3 forward;
    forward.x = cosf(yawRad) * cosf(pitchRad);
    forward.y = sinf(yawRad) * cosf(pitchRad);
    forward.z = sinf(pitchRad);

    float angle = GetAngleBetweenVectors(forward, toPoint);
    return (angle <= fovDegrees * 0.5f);
}

bool ExternalOverlay::WorldToScreen(float* viewMatrix, const Vec3& worldPos, const Vec3& camPos, D2D1_POINT_2F& out, int width, int height) {

    Vec3 direction = worldPos - camPos;


    Vec3 transformedDir;
    transformedDir.x = direction.x * viewMatrix[0] + direction.y * viewMatrix[4] + direction.z * viewMatrix[8] + viewMatrix[12];
    transformedDir.y = direction.x * viewMatrix[1] + direction.y * viewMatrix[5] + direction.z * viewMatrix[9] + viewMatrix[13];
    transformedDir.z = direction.x * viewMatrix[2] + direction.y * viewMatrix[6] + direction.z * viewMatrix[10] + viewMatrix[14];


    if (transformedDir.z <= 0.1f) return false;


    out.x = (transformedDir.x / transformedDir.z) * (width / 2.0f) + (width / 2.0f);
    out.y = -(transformedDir.y / transformedDir.z) * (height / 2.0f) + (height / 2.0f);

    return true;
}

void ExternalOverlay::DrawDistanceESP(const PlayerData& player, const D2D1_POINT_2F& screenPos, float distance) {
    if (!smallTextFormat || !boxBrush) return;


    wchar_t distanceText[32];
    swprintf_s(distanceText, L"%.1fm", distance);
    

    D2D1_RECT_F textRect = D2D1::RectF(
        screenPos.x - 50.0f,
        screenPos.y + 25.0f,  
        screenPos.x + 50.0f,
        screenPos.y + 45.0f
    );
    

    smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    

    D2D1_COLOR_F originalColor = boxBrush->GetColor();
    

    boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black, 1.0f));
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            D2D1_RECT_F outlineRect = D2D1::RectF(
                textRect.left + i,
                textRect.top + j,
                textRect.right + i,
                textRect.bottom + j
            );
            renderTarget->DrawTextW(
                distanceText,
                wcslen(distanceText),
                smallTextFormat,
                outlineRect,
                boxBrush
            );
        }
    }
    
    boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
    renderTarget->DrawTextW(
        distanceText,
        wcslen(distanceText),
        smallTextFormat,
        textRect,
        boxBrush
    );
    
    boxBrush->SetColor(originalColor);
}

void ExternalOverlay::DrawRankESP(const PlayerData& player, const D2D1_POINT_2F& screenPos, float distance) {
    if (!smallTextFormat || !boxBrush) return;

    wchar_t rankText[32];
    swprintf_s(rankText, L"R: %d", player.rank);
    
    D2D1_RECT_F textRect = D2D1::RectF(
        screenPos.x - 50.0f,
        screenPos.y - 45.0f,  
        screenPos.x + 50.0f,
        screenPos.y - 25.0f
    );
    

    smallTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    smallTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_FAR);
    

    D2D1_COLOR_F originalColor = boxBrush->GetColor();
    

    boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Black, 1.0f));
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (i == 0 && j == 0) continue;
            D2D1_RECT_F outlineRect = D2D1::RectF(
                textRect.left + i,
                textRect.top + j,
                textRect.right + i,
                textRect.bottom + j
            );
            renderTarget->DrawTextW(
                rankText,
                wcslen(rankText),
                smallTextFormat,
                outlineRect,
                boxBrush
            );
        }
    }
    

    boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 1.0f));
    renderTarget->DrawTextW(
        rankText,
        wcslen(rankText),
        smallTextFormat,
        textRect,
        boxBrush
    );
    

    boxBrush->SetColor(originalColor);
}

void ExternalOverlay::DrawSkeletonESP(const PlayerData& player, const Camera& camera, const float* viewMatrix, const std::set<uintptr_t>& lookingDirectlyPlayers) {

    if (player.pawnAddress != 0) {

        int pitchInt = Read<int>(player.pawnAddress + 0x88C);
        int yawInt = Read<int>(player.pawnAddress + 0x890);


        float pitch = (static_cast<float>(pitchInt) * 360.0f / 65536.0f) * (static_cast<float>(M_PI) / 180.0f);
        float yaw = (static_cast<float>(yawInt) * 360.0f / 65536.0f) * (static_cast<float>(M_PI) / 180.0f);


        Vec3 headPos = GetBoneWorldPosition(player.pawnAddress, 6);

        if (headPos.x != 0 || headPos.y != 0 || headPos.z != 0) {

            Vec3 forward;
            forward.x = cos(yaw) * cos(pitch);
            forward.y = sin(yaw) * cos(pitch);
            forward.z = sin(pitch);


            float lineLength = 100.0f;
            Vec3 endPoint = {
                headPos.x + forward.x * lineLength,
                headPos.y + forward.y * lineLength,
                headPos.z + forward.z * lineLength
            };


            D2D1_POINT_2F headScreen, endScreen;
            Vec3 camPos = { camera.x, camera.y, camera.z };
            int width = static_cast<int>(renderTarget->GetSize().width);
            int height = static_cast<int>(renderTarget->GetSize().height);

            if (WorldToScreen((float*)viewMatrix, headPos, camPos, headScreen, width, height) &&
                WorldToScreen((float*)viewMatrix, endPoint, camPos, endScreen, width, height)) {
                if (boxBrush) {
                   
                    if (lookingDirectlyPlayers.count(player.pawnAddress)) {
                        boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Red));
                    } else {
                        boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
                    }
                    renderTarget->DrawLine(headScreen, endScreen, boxBrush, 2.5f);
                }
            }
        }
    }

    if (!player.pawnAddress || !viewMatrix) {
        return;
    }


    D2D1_COLOR_F skeletonColor;
    if (lookingDirectlyPlayers.count(player.pawnAddress)) {
        skeletonColor = D2D1::ColorF(D2D1::ColorF::Red); // Red if looking directly
    } else if (lineOfSightCheck && player.isVisible) {
        skeletonColor = D2D1::ColorF(D2D1::ColorF::Green); // Green if visible
    } else {
        skeletonColor = D2D1::ColorF(D2D1::ColorF::White); // White if not visible
    }
    boxBrush->SetColor(skeletonColor);


    struct BoneConnection {
        int from;
        int to;
    };

    std::vector<BoneConnection> connections;
    
    // Add connections based on gender (1 = male, 2 = female)
    if (player.gender == 2) { // Female
        // Bal kar (bal kézfej [99] -> bal könyök [98] -> nyak [5])
        connections.push_back({99, 98}); // kézfej->könyök
        connections.push_back({98, 5});  // könyök->nyak
        // Jobb kar (jobb kézfej [72] ->jobb könyök [71] -> nyak [5])
        connections.push_back({72, 71}); // kézfej->könyök
        connections.push_back({71, 5});  // könyök->nyak
        // Gerinc/fej (fejközép [6] -> nyak [5] -> testközép [4] -> has [3] -> csípő [0])
        connections.push_back({6, 5});   // fejközép->nyak
        connections.push_back({5, 4});   // nyak->testközép
        connections.push_back({4, 3});   // testközép->has
        connections.push_back({3, 0});   // has->csípő
        // Bal láb (bal lábfej [128] -> boka [127] -> térd [126] -> csípő [0])
        connections.push_back({128, 127}); // lábfej->boka
        connections.push_back({127, 126}); // boka->térd
        connections.push_back({126, 0});   // térd->csípő
        // Jobb láb (jobb lábfej [135] -> boka [134] -> térd [133] -> csípő [0])
        connections.push_back({135, 134}); // lábfej->boka
        connections.push_back({134, 133}); // boka->térd
        connections.push_back({133, 0});   // térd->csípő
    } else { // Male
        // Bal kar (bal kézfej [86] -> bal könyök [87] -> nyak [5])
        connections.push_back({86, 87}); // kézfej->könyök
        connections.push_back({87, 5});  // könyök->nyak
        // Jobb kar (jobb kézfej [94] -> jobb könyök [91] -> nyak [5])
        connections.push_back({94, 91}); // kézfej->könyök
        connections.push_back({91, 5});  // könyök->nyak
        // Gerinc/fej (fejközép [6] -> nyak [5] -> testközép [4] -> has [3] -> csípő [0])
        connections.push_back({6, 5});   // fejközép->nyak
        connections.push_back({5, 4});   // nyak->testközép
        connections.push_back({4, 3});   // testközép->has
        connections.push_back({3, 0});   // has->csípő
        // Bal láb (bal lábfej [127] -> boka [126] -> térd [125] -> csípő [0])
        connections.push_back({127, 126}); // lábfej->boka
        connections.push_back({126, 125}); // boka->térd
        connections.push_back({125, 0});   // térd->csípő
        // Jobb láb (jobb lábfej [134] -> boka [133] -> térd [132] -> csípő [0])
        connections.push_back({134, 133}); // lábfej->boka
        connections.push_back({133, 132}); // boka->térd
        connections.push_back({132, 0});   // térd->csípő
    }


    D2D1_POINT_2F headScreenPos;
    bool headVisible = false;


    Vec3 headWorldPos = GetBoneWorldPosition(player.pawnAddress, 6);
    if (headWorldPos.x != 0 || headWorldPos.y != 0 || headWorldPos.z != 0) {

        RECT rect;
        GetClientRect(overlayHwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;


        Vec3 camPos = { camera.x, camera.y, camera.z };
        headVisible = WorldToScreen((float*)viewMatrix, headWorldPos, camPos, headScreenPos, width, height);
    }


    for (const auto& conn : connections) {
        Vec3 bone1Pos = GetBoneWorldPosition(player.pawnAddress, conn.from);
        Vec3 bone2Pos = GetBoneWorldPosition(player.pawnAddress, conn.to);

        if (bone1Pos.x == 0 && bone1Pos.y == 0 && bone1Pos.z == 0) continue;
        if (bone2Pos.x == 0 && bone2Pos.y == 0 && bone2Pos.z == 0) continue;
        

        D2D1_POINT_2F screenPos1, screenPos2;
        Vec3 camPos(camera.x, camera.y, camera.z);
        
        RECT rect;
        GetClientRect(overlayHwnd, &rect);
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;
        
        if (WorldToScreen((float*)viewMatrix, bone1Pos, camPos, screenPos1, width, height) &&
            WorldToScreen((float*)viewMatrix, bone2Pos, camPos, screenPos2, width, height)) {
            renderTarget->DrawLine(
                D2D1::Point2F(screenPos1.x, screenPos1.y),
                D2D1::Point2F(screenPos2.x, screenPos2.y),
                boxBrush,
                1.5f     
            );
        }
        
  
        if ((conn.from == 6 || conn.to == 6) && headVisible) {
 
            float distanceToPlayer = sqrtf(
                (player.x - camera.x) * (player.x - camera.x) +
                (player.y - camera.y) * (player.y - camera.y) +
                (player.z - camera.z) * (player.z - camera.z)
            );

           
            float baseRadius = 15.0f;
            float minRadius = 2.0f;  
            float maxRadius = 30.0f;  
            

            float clampedDistance = max(1.0f, min(distanceToPlayer, 100.0f));
            float radius = baseRadius * (1.0f / (clampedDistance * 0.1f));
            

            radius = max(minRadius, min(maxRadius, radius));
            
  
            D2D1_COLOR_F originalColor = boxBrush->GetColor();

            boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White, 0.7f));
            

            D2D1_ELLIPSE headCircle = D2D1::Ellipse(
                D2D1::Point2F(headScreenPos.x, headScreenPos.y),
                radius,
                radius
            );
            

            renderTarget->DrawEllipse(headCircle, boxBrush, 1.5f);
            

            boxBrush->SetColor(originalColor);
        }
    }
}

void ExternalOverlay::DrawRadar(const std::vector<PlayerData>& players, const PlayerData& localPlayer, const Camera& camera) {
    if (!showRadar || !renderTarget || !boxBrush) return;
    

    D2D1_COLOR_F originalColor = boxBrush->GetColor();
    

    RECT rect;
    GetClientRect(overlayHwnd, &rect);
    float windowWidth = static_cast<float>(rect.right - rect.left);
    float windowHeight = static_cast<float>(rect.bottom - rect.top);
    
    const float radarCenter = radarSize * 0.5f;
    const float radarMarginX = 20.0f;      
    const float radarMarginY = 200.0f;     
    const float radarX = windowWidth - radarSize - radarMarginX; 
    const float radarY = windowHeight - radarSize - radarMarginY; 
    
    // Draw radar background
    D2D1_RECT_F radarRect = D2D1::RectF(
        radarX,
        radarY,
        radarX + radarSize,
        radarY + radarSize
    );
    

    boxBrush->SetColor(radarBackgroundColor);
    renderTarget->FillRectangle(radarRect, boxBrush);
    

    boxBrush->SetColor(radarForegroundColor);
    renderTarget->DrawRectangle(radarRect, boxBrush, 1.0f);
    

    renderTarget->DrawLine(
        D2D1::Point2F(radarX + radarCenter, radarY),
        D2D1::Point2F(radarX + radarCenter, radarY + radarSize),
        boxBrush, 1.0f
    );
    
    renderTarget->DrawLine(
        D2D1::Point2F(radarX, radarY + radarCenter),
        D2D1::Point2F(radarX + radarSize, radarY + radarCenter),
        boxBrush, 1.0f
    );
    

    float yawRad = (camera.yaw - 90.0f) * (3.14159f / 180.0f);
    float dirLength = radarCenter - 5.0f;
    float dirX = cosf(yawRad) * dirLength;
    float dirY = sinf(yawRad) * dirLength;
    
    renderTarget->DrawLine(
        D2D1::Point2F(radarX + radarCenter, radarY + radarCenter),
        D2D1::Point2F(radarX + radarCenter + dirX, radarY + radarCenter + dirY),
        boxBrush, 1.5f
    );
    

    for (const auto& player : players) {
        if (!player.found || player.isDead) continue;
        

        if (localPlayer.found && player.pawnAddress == localPlayer.pawnAddress) {
            continue;
        }
        

        if (localPlayer.found) {
            if (filterMissionOnly && player.missionId != localPlayer.missionId) {
                continue;
            }
            
           
            if (filterTeammates) {
                
                if (player.faction == localPlayer.faction) {
                    continue; 
                }
            } else {

                if (filterMissionOnly && player.missionId != localPlayer.missionId) {
                    continue;
                }
            }
        }
        

        float dx = (player.x - camera.x) * 0.01f; 
        float dy = (player.y - camera.y) * 0.01f;
        float distance = sqrtf(dx * dx + dy * dy);
        
        // Skip if too far
        if (distance > radarRange) continue;
        

        float angle = atan2f(dy, dx) - (camera.yaw * (3.14159f / 180.0f));
        angle = -angle; // Invert Y axis
        

        float scale = (radarCenter - 5.0f) / radarRange;
        float radarPosX = radarX + radarCenter + cosf(angle) * (distance * scale);
        float radarPosY = radarY + radarCenter - sinf(angle) * (distance * scale);
        

        boxBrush->SetColor(radarEnemyColor);
        

        float squareSize = 4.0f;
        D2D1_RECT_F playerRect = D2D1::RectF(
            radarPosX - squareSize/2,
            radarPosY - squareSize/2,
            radarPosX + squareSize/2,
            radarPosY + squareSize/2
        );
        boxBrush->SetColor(D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f)); // Red for enemies
        renderTarget->FillRectangle(playerRect, boxBrush);
        

        wchar_t distanceText[16];
        swprintf_s(distanceText, L"%.0fm", distance);
        

        IDWriteTextLayout* textLayout = nullptr;
        if (SUCCEEDED(writeFactory->CreateTextLayout(
            distanceText,
            (UINT32)wcslen(distanceText),
            smallTextFormat,
            50.0f,  // Max width
            20.0f,  // Max height
            &textLayout
        ))) {

            DWRITE_TEXT_METRICS textMetrics;
            textLayout->GetMetrics(&textMetrics);
            
         
            D2D1_POINT_2F textPos = D2D1::Point2F(
                radarPosX - textMetrics.width/2,  
                radarPosY + squareSize/2 + 1.0f    
            );
            
         
            textPos.x = floorf(textPos.x + 0.5f);
            textPos.y = floorf(textPos.y + 0.5f);
            

            boxBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f));
            renderTarget->DrawTextLayout(
                textPos,
                textLayout,
                boxBrush
            );
            
            textLayout->Release();
        }
    }
    
    float selfSize = 5.0f;
    D2D1_RECT_F selfRect = D2D1::RectF(
        radarX + radarCenter - selfSize/2,
        radarY + radarCenter - selfSize/2,
        radarX + radarCenter + selfSize/2,
        radarY + radarCenter + selfSize/2
    );
    boxBrush->SetColor(D2D1::ColorF(0.0f, 1.0f, 0.0f, 1.0f)); 
    renderTarget->FillRectangle(selfRect, boxBrush);
    
    boxBrush->SetColor(originalColor);
}

void ExternalOverlay::DrawPlayerESP(const std::vector<PlayerData>& players, const PlayerData& localPlayer, const Camera& camera, const float* viewMatrix, bool viewMatrixReadSuccess, const std::set<uintptr_t>& lookingDirectlyPlayers) {
  
    if (!boxESP && !lineESP && !distanceESP && !rankESP && !skeletonESP) return;

    if (!viewMatrixReadSuccess) {
        return;
    }

    worldToScreenSuccessCount = 0;

    RECT rect;
    GetClientRect(overlayHwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    for (const auto& player : players) {
        if (!player.found) continue;


        uintptr_t selfPawnAddr = localPlayer.found ? localPlayer.pawnAddress : lastKnownSelfPawn;


        if (player.pawnAddress == 0) {
            continue; 
        }

        if (player.isDead) {
            continue;
        }


        if (localPlayer.found && !localPlayer.isDead) {
         
            if (filterMissionOnly && player.missionId != localPlayer.missionId) {
                continue;
            }
            
            if (filterTeammates && player.faction == localPlayer.faction) {
                continue;
            }
        }
       

        D2D1_POINT_2F screenPos;
        Vec3 playerPos = { player.x, player.y, player.z };
        Vec3 camPos = { camera.x, camera.y, camera.z };

        if (WorldToScreen((float*)viewMatrix, playerPos, camPos, screenPos, width, height)) {
            worldToScreenSuccessCount++;
            float dx = player.x - camera.x;
            float dy = player.y - camera.y;
            float dz = player.z - camera.z;
            float distance = sqrtf(dx * dx + dy * dy + dz * dz) / ESP_CONVERSION_FACTOR;
            if (distance < 2.0f) continue;


            float boxHeight = 150.0f / (distance * 0.1f);
            float boxWidth = boxHeight * 0.6f;

            // Draw skeleton ESP if enabled
            if (skeletonESP) {
                DrawSkeletonESP(player, camera, viewMatrix, lookingDirectlyPlayers);
            }
            

            if (boxESP) {
                if (boxBrush) boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
                D2D1_RECT_F espRect = D2D1::RectF(
                    screenPos.x - boxWidth / 2.0f,
                    screenPos.y - boxHeight / 2.0f,
                    screenPos.x + boxWidth / 2.0f,
                    screenPos.y + boxHeight / 2.0f
                );
                renderTarget->DrawRectangle(&espRect, boxBrush, 1.5f);
            }

            // ESP Distance
            if (distanceESP && smallTextFormat) {
                DrawDistanceESP(player, screenPos, distance);
            }
            
            // ESP Rank
            if (rankESP && smallTextFormat) {
                DrawRankESP(player, screenPos, distance);
            }
            
            // Draw line ESP if enabled
            if (lineESP) {
                if (boxBrush) boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::White));
                
                D2D1_COLOR_F originalColor = boxBrush->GetColor();
                
                boxBrush->SetColor(lineESPColor);
                
                D2D1_SIZE_F screenSize = renderTarget->GetSize();
                D2D1_POINT_2F center = { screenSize.width / 2.0f, screenSize.height };
                
                static ID2D1StrokeStyle* lineStrokeStyle = nullptr;
                if (!lineStrokeStyle) {
                    ID2D1Factory* factory = nullptr;
                    renderTarget->GetFactory(&factory);
                    if (factory) {
                        factory->CreateStrokeStyle(
                            D2D1::StrokeStyleProperties(
                                D2D1_CAP_STYLE_ROUND,
                                D2D1_CAP_STYLE_ROUND,
                                D2D1_CAP_STYLE_ROUND,
                                D2D1_LINE_JOIN_ROUND,
                                1.0f,
                                D2D1_DASH_STYLE_SOLID,
                                0.0f
                            ),
                            nullptr,
                            0,
                            &lineStrokeStyle
                        );
                        factory->Release();
                    }
                }
                
                if (lineStrokeStyle) {
                    renderTarget->DrawLine(
                        center,
                        screenPos,
                        boxBrush,
                        1.5f,
                        lineStrokeStyle
                    );
                } else {
                    renderTarget->DrawLine(center, screenPos, boxBrush, 1.5f);
                }
                
                boxBrush->SetColor(originalColor);
            }
        }
    }
}

// -------------------- Grenade ESP Support --------------------

void ExternalOverlay::UpdateGrenadeCache() {
    if (!uWorldAddress) return;

    uintptr_t gameInstance = Read<uintptr_t>(uWorldAddress + 0x3AC);
    if (!gameInstance) return;

    std::vector<uintptr_t> newGrenadeAddresses;

    for (uintptr_t offset = 0x200; offset < 0x10000; offset += 0x8) { 
        uintptr_t projectileObject = Read<uintptr_t>(gameInstance + offset);
        if (!projectileObject) continue;

        int objectId = Read<int>(projectileObject + 0x24);
        if (objectId == 11445) {
            newGrenadeAddresses.push_back(projectileObject);
        }
    }

    std::lock_guard<std::mutex> lock(dataMutex);
    cachedGrenadeAddresses = newGrenadeAddresses;
    lastGrenadeCacheUpdate = GetTickCount();
}

std::vector<ExternalOverlay::GrenadeData> ExternalOverlay::FindAllGrenades() {
    std::vector<GrenadeData> found;
    std::vector<uintptr_t> curr;
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        curr = cachedGrenadeAddresses;
    }

    char buffer[0x500];
    for (uintptr_t grenade : curr) {
        if (!ReadProcessMemory(targetProcessHandle, (LPCVOID)grenade, buffer, sizeof(buffer), NULL)) {
            continue;
        }
        GrenadeData gd{};
        gd.address = grenade;
        gd.x = *reinterpret_cast<float*>(buffer + 0x15C);
        gd.y = *reinterpret_cast<float*>(buffer + 0x160);
        gd.z = *reinterpret_cast<float*>(buffer + 0x164);
        gd.fuseTime = *reinterpret_cast<float*>(buffer + 0x390);

        if (gd.x == 0.0f && gd.y == 0.0f && gd.z == 0.0f) {
            continue;
        }

        gd.found = true;
        found.push_back(gd);
    }
    return found;
}

void ExternalOverlay::DrawGrenadeESP(const std::vector<GrenadeData>& grenades, const Camera& camera, const float* viewMatrix, bool viewMatrixReadSuccess) {
    if (!viewMatrixReadSuccess || grenades.empty()) return;

    RECT rect; GetClientRect(overlayHwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    if (!boxBrush || !smallTextFormat) return;

    for (const auto& grenade : grenades) {
        if (!grenade.found) continue;
        D2D1_POINT_2F screenPos;
        Vec3 gPos{ grenade.x, grenade.y, grenade.z };
        Vec3 camPos{ camera.x, camera.y, camera.z };
        if (!WorldToScreen((float*)viewMatrix, gPos, camPos, screenPos, width, height)) {
            continue;
        }

        float dx = grenade.x - camera.x;
        float dy = grenade.y - camera.y;
        float dz = grenade.z - camera.z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz) / ESP_CONVERSION_FACTOR;
        if (dist < 1.0f) dist = 1.0f;

        float boxSize = 12.0f / dist;
        D2D1_RECT_F espBox = D2D1::RectF(
            screenPos.x - boxSize / 2,
            screenPos.y - boxSize / 2,
            screenPos.x + boxSize / 2,
            screenPos.y + boxSize / 2
        );

        boxBrush->SetColor(D2D1::ColorF(D2D1::ColorF::Orange));
        renderTarget->DrawRectangle(&espBox, boxBrush, 1.5f);

        wchar_t fuseTxt[32];
        swprintf_s(fuseTxt, L"%.1f s", grenade.fuseTime);
        D2D1_RECT_F txtRect = { espBox.right + 2, espBox.top, espBox.right + 60, espBox.bottom };
        renderTarget->DrawTextW(fuseTxt, (UINT32)wcslen(fuseTxt), smallTextFormat, txtRect, boxBrush);
    }
}

bool ExternalOverlay::IsBadReadPtr(void* p, size_t size) {
    if (!p) {
        return true;
    }
    MEMORY_BASIC_INFORMATION mbi = {0};
    if (VirtualQueryEx(targetProcessHandle, p, &mbi, sizeof(mbi)) == 0) {
        return true;
    }

    if (mbi.State != MEM_COMMIT) {
        return true;
    }

    if (mbi.Protect == PAGE_NOACCESS || (mbi.Protect & PAGE_GUARD)) {
        return true;
    }

    if (mbi.RegionSize < size) {
        return true;
    }

    return false;
}
