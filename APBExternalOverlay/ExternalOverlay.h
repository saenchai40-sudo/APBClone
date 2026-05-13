#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <map>
#include <string>
#include <d2d1.h>
#include <dwrite.h>
#include <d2d1_1.h>
#include <dwrite_1.h>
#include <dwrite.h>
#include <vector>
#include <mutex>
#include <string>
#include <set>
#include "obfuscate.h"
#include <TlHelp32.h>
#include <thread>
#include <chrono>

//menu

enum class MenuItemType {
    Section,
    Toggle,
    Slider,
    Hotkey,
    Selector
};

struct MenuItem {
    MenuItemType type = MenuItemType::Toggle;
    std::wstring text;
    bool* value = nullptr;    // toggle
    float* floatValue = nullptr; // slider
    float minValue = 0.0f;
    float maxValue = 1.0f;
    int* hotkey = nullptr;   // hotkeybind
    int* selectedIndex = nullptr;
    std::vector<std::wstring> options;
};

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")


struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }
};

struct ModuleInfo {
    uintptr_t baseAddress = 0;
    DWORD moduleSize = 0;
};

class ExternalOverlay {
public:
    struct PlayerData {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        int hp = -1;
        uint8_t teamId = 0;
        int missionId = 0;
        uint8_t faction = 0;
        int rank = 0;
        uintptr_t pawnAddress = 0;
        uintptr_t controllerAddress = 0;
        bool isDead = false;
        uint8_t gender = 1;
        bool found = false;
        bool isVisible = false;
    };

    struct GrenadeData {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float fuseTime = 0.0f;
        uintptr_t address = 0;
        bool found = false;
    };

    struct Camera {
        float x = 0, y = 0, z = 0;
        float yaw = 0, pitch = 0, roll = 0;
    };

    ExternalOverlay();
    ~ExternalOverlay();
    bool Initialize(HINSTANCE hInstance);
    void Run();

private:
    // Signature scanning
    struct Signature {
        std::string pattern;
        int offset;
        Signature() : offset(0) {}
        Signature(const std::string& p, int o) : pattern(p), offset(o) {}
    };
    
    std::map<std::string, uintptr_t> gameAddresses;
    std::map<std::string, Signature> gameSignatures;
    
    void InitializeSignatures() {
        // GNames: Exact spacing, no trailing space
        gameSignatures["GNames"] = Signature("48 8B 0D 39 8E 7B 03", 3);

        // GObjects: Fixed the spacing
        gameSignatures["GObjects"] = Signature("48 8B 0D ? ? ? ? 48 8B 01 FF 50", 3);

        // UWorld: Fixed the spacing
        gameSignatures["UWorld"] = Signature("48 8D 0D ? ? ? ? E8 B5 45 0C 00 48 83", 3);

        // GEngine: Added more bytes for uniqueness and fixed wildcards
        gameSignatures["GEngine"] = Signature("48 8D 0D ? ? ? ? 48 FF 25 E2 13", 3);

        // PawnClass: Your signature with proper spacing
        gameSignatures["PawnClass"] = Signature("48 8D 0D ? ? ? ? 45 33 C0 48 8B D7", 3);
    }

    bool ScanGameSignatures();
    void PrintAddresses();
    
    void RunAimbot(const std::vector<PlayerData>& players, const PlayerData& localPlayer, const Camera& camera, const float* viewMatrix, bool viewMatrixReadSuccess);
    void RunTriggerbot();
    uintptr_t persistentTargetPawn = 0; 
    bool aimingActive = false;
    int aimbotTargetIndex = 0; // 0: Head, 1: Chest, 2: Legs
    std::vector<std::wstring> aimbotTargetOptions;
    int aimbotHotkeyIndex = 0;
    int triggerbotHotkeyIndex = 0;
    std::vector<std::pair<std::wstring, int>> availableHotkeys;

    bool debug = false;
    int debugCrosshairClassId = 0;

    HWND overlayHwnd = nullptr;
    HWND targetHwnd = nullptr;
    ID2D1Factory* d2dFactory = nullptr;
    ID2D1HwndRenderTarget* renderTarget = nullptr;
    IDWriteFactory* writeFactory = nullptr;
    ID2D1SolidColorBrush* textBrush = nullptr;
    ID2D1SolidColorBrush* selectedTextBrush = nullptr;
    ID2D1SolidColorBrush* menuBackgroundBrush = nullptr;
    ID2D1SolidColorBrush* boxBrush = nullptr;
    ID2D1SolidColorBrush* visibleBrush = nullptr;
    
    ID2D1SolidColorBrush* modernMenuBgBrush = nullptr;
    ID2D1SolidColorBrush* modernMenuBorderBrush = nullptr;
    ID2D1SolidColorBrush* modernMenuItemBgBrush = nullptr;
    ID2D1SolidColorBrush* modernMenuItemHoverBrush = nullptr;
    ID2D1SolidColorBrush* modernMenuSectionBrush = nullptr;
    ID2D1SolidColorBrush* modernMenuAccentBrush = nullptr;
    IDWriteTextFormat* smallTextFormat = nullptr;
    IDWriteTextFormat* mediumTextFormat = nullptr;
    IDWriteTextFormat* largeTextFormat = nullptr;
    ID2D1StrokeStyle* textOutlineStyle = nullptr;

    // Debug info
    bool showDebugInfo = false;
    std::wstring debugPlayerPos = L"";
    std::wstring debugCameraPos = L"";
    std::wstring debugViewMatrix = L"";
    std::wstring debugGrenadeInfo = L"";

    bool showMenu = false;
    bool menuVisible = false;
    bool bindingHotkey = false;
    int selectedMenuItem = 0;
    std::vector<MenuItem> menuItems;
    std::wstring randomWindowName;



    bool isTargetFound = false;
    DWORD targetProcessId = 0;
    HANDLE targetProcessHandle;
    uintptr_t gameBaseAddress = 0;
    uintptr_t uWorldAddress = 0;
    uintptr_t localPlayerControllerAddress = 0;
    uintptr_t playerController = 0;
    uintptr_t cachedControllerOffset = 0;
    DWORD64 lastControllerOffsetUpdate = 0;
    uintptr_t lastControllerPtr = 0;
    uintptr_t cachedHudAddress = 0;
    DWORD lastReadError = 0;
    bool viewMatrixReadSuccess = false;
    int worldToScreenSuccessCount = 0;

    // ESP
    std::vector<PlayerData> players;
    std::vector<GrenadeData> grenades;
    std::vector<uintptr_t> cachedPawnAddresses;
    std::vector<uintptr_t> cachedGrenadeAddresses;
    DWORD lastPawnCacheUpdate = 0;
    DWORD lastGrenadeCacheUpdate = 0;
    PlayerData localPlayer;
    Camera camera;
    float viewMatrix[16];
    uintptr_t lastKnownSelfPawn = 0;
    std::map<uintptr_t, float> lastLosValues;

    std::atomic<bool> isRunning;
    std::thread dataUpdateThread;
    std::mutex dataMutex;

    // ESP settings
    bool boxESP = false;
    bool lineESP = false;
    bool skeletonESP = true;
    bool distanceESP = true;
    bool rankESP = true;
    bool grenadeESP = true;
    bool showRadar = false; // Radar toggle
    float radarRange = 50.0f; // Radar range in meters (50m)
    float radarSize = 150.0f; // Radar size in pixels
    float radarX = 10.0f; // Radar X position
    float radarY = 10.0f; // Radar Y position
    D2D1::ColorF radarBackgroundColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.5f); // Radar background color
    D2D1::ColorF radarForegroundColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f); // Radar foreground color
    D2D1::ColorF radarPlayerColor = D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f); // Radar player color
    D2D1::ColorF radarEnemyColor = D2D1::ColorF(0.0f, 1.0f, 0.0f, 1.0f); // Radar enemy color

    // AIMBOT
    bool aimbotEnabled = true;
    bool triggerbotEnabled = false;
    int triggerbotHotkey = 0x02;
    bool showFov = false;
    float fovSize = 6.0f; // slider
    float smoothing = 5.0f; // slider
    int aimbotHotkey = 0x43; // 'C' def

    // FILTERS
    bool lineOfSightCheck = true;
    bool filterMissionOnly = true;
    bool filterTeammates = true;
    bool streamerMode = true;



    const bool filterDeadPlayers = true;
    D2D1::ColorF boxESPColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    D2D1::ColorF lineESPColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    D2D1::ColorF textESPColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    
    static constexpr float ESP_CONVERSION_FACTOR = 100.0f;

    template <typename T>
    T Read(uintptr_t address) {
        T buffer = { };
        // If user-mode is blocked, this request must be sent to a kernel hook
        // or a vulnerable driver to bypass handle stripping.
        if (!ReadProcessMemory(targetProcessHandle, (LPCVOID)address, &buffer, sizeof(T), NULL)) {
            // Fallback or Error handling
        }
        return buffer;
    }

    ModuleInfo GetModuleInfo(const TCHAR* modName);
    uintptr_t FindSignature(const char* signature);
    void Shutdown();
    void FindTargetWindow();
    bool CreateDeviceResources();
    void DiscardDeviceResources();
    void HandleInput();
    void Draw();
    void DrawMenu(int x, int y, const std::vector<MenuItem>& items);
    void ToggleMenu();

    // ESP
    Camera GetLocalCamera();
    PlayerData GetSelfPlayer();
    PlayerData GetPlayerFromController(uintptr_t controller);
    void UpdatePawnCache();
    std::vector<PlayerData> FindAllPlayers();
    // ESP Drawing Functions
    void DrawPlayerESP(const std::vector<PlayerData>& players, const PlayerData& localPlayer, const Camera& camera, const float* viewMatrix, bool viewMatrixReadSuccess, const std::set<uintptr_t>& lookingDirectlyPlayers);
    void DrawBoxESP(const D2D1_POINT_2F& screenPos, float boxWidth, float boxHeight);
    void DrawLineESP(const PlayerData& player, const D2D1_POINT_2F& screenPos);
    void DrawSkeletonESP(const PlayerData& player, const Camera& camera, const float* viewMatrix, const std::set<uintptr_t>& lookingDirectlyPlayers);
    void DrawDistanceESP(const PlayerData& player, const D2D1_POINT_2F& screenPos, float distance);
    void DrawRankESP(const PlayerData& player, const D2D1_POINT_2F& screenPos, float distance);
    void DrawRadar(const std::vector<PlayerData>& playersCopy, const PlayerData& localPlayerCopy, const Camera& cameraCopy); // New function for drawing the radar
    void DrawGrenadeESP(const std::vector<GrenadeData>& grenades, const Camera& camera, const float* viewMatrix, bool viewMatrixReadSuccess);
    std::vector<GrenadeData> FindAllGrenades();
    void UpdateGrenadeCache();

    bool IsPointInFOV(const Vec3& point, const Vec3& camPos, float camYaw, float camPitch, float fovDegrees);
    bool WorldToScreen(float* viewMatrix, const Vec3& worldPos, const Vec3& camPos, D2D1_POINT_2F& out, int width, int height);
    void DataUpdateThread();
    bool ReadViewMatrix(float* outMatrix);
    bool IsBadReadPtr(void* p, size_t size);
    Vec3 GetBoneWorldPosition(uintptr_t pawnAddress, int boneId);
};
