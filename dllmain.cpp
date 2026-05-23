#include <windows.h>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm> // Required for string lowercasing

#include "Include\MinHook.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ========================================================================
// 1. MEMORY ADDRESSES & GLOBALS
// ========================================================================

int* ActiveCursorIndex = (int*)0x00be2c2c;

typedef void* (__cdecl* GetHardwareBuffer_t)();
GetHardwareBuffer_t GetHardwareBuffer = (GetHardwareBuffer_t)0x0058b5b0;
typedef void(__cdecl* PushToScreen_t)();
PushToScreen_t PushToScreen = (PushToScreen_t)0x0058b5c0;
typedef void(__cdecl* RenderCursor_t)();
RenderCursor_t RenderCursor_Original = nullptr;

typedef uint32_t(__fastcall* LookupCursor_t)(const char*);
LookupCursor_t LookupCursor_Original = nullptr;

typedef HCURSOR(WINAPI* SetCursor_t)(HCURSOR);
SetCursor_t SetCursor_Original = nullptr;

// ========================================================================
// 2. MODULAR DATA STRUCTURES
// ========================================================================

// Maps a lowercase cursor name to a unique integer index 
std::unordered_map<std::string, int> CursorNameToIndexMap;

// Maps the unique integer index to the actual pixel/hardware data
std::unordered_map<int, std::vector<uint32_t>> CustomCursorPixels;
std::unordered_map<int, HCURSOR> CustomHardwareCursors;

// The starting index for custom cursors (Bypassing 1.12.1  0-41 limit)
int NextAvailableIndex = 42;

// ========================================================================
// 3. UTILITIES & IMAGE LOADING
// ========================================================================

void Log(const std::string& message) {
    // Writes to the native WoW Logs folder
    std::ofstream logFile("Logs\\OmniCursor.log", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << message << "\n";
        logFile.close();
    }
}

std::vector<uint32_t> LoadCursorFromPNG(const std::string& filepath) {
    std::vector<uint32_t> pixelData;
    int width, height, channels;
    unsigned char* img = stbi_load(filepath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (img != nullptr && width == 32 && height == 32) {
        pixelData.resize(1024, 0);
        for (int i = 0; i < 1024; ++i) {
            int offset = i * 4;
            pixelData[i] = (img[offset + 3] << 24) | (img[offset + 0] << 16) | (img[offset + 1] << 8) | img[offset + 2];
        }
        stbi_image_free(img);
    }
    return pixelData;
}

HCURSOR CreateAlphaCursor(const std::vector<uint32_t>& pixels) {
    HDC hdc = GetDC(NULL);
    BITMAPV5HEADER bi = { 0 };
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = 32; bi.bV5Height = -32; bi.bV5Planes = 1; bi.bV5BitCount = 32; bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000; bi.bV5GreenMask = 0x0000FF00; bi.bV5BlueMask = 0x000000FF; bi.bV5AlphaMask = 0xFF000000;

    void* lpBits;
    HBITMAP hColor = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &lpBits, NULL, 0);
    if (hColor && lpBits) memcpy(lpBits, pixels.data(), 1024 * 4);

    HBITMAP hMask = CreateBitmap(32, 32, 1, 1, NULL);
    ICONINFO ii = { 0 };
    ii.fIcon = FALSE; ii.xHotspot = 0; ii.yHotspot = 0; ii.hbmMask = hMask; ii.hbmColor = hColor;
    HCURSOR hCur = CreateIconIndirect(&ii);

    DeleteObject(hColor); DeleteObject(hMask); ReleaseDC(NULL, hdc);
    return hCur;
}

// ========================================================================
// 4. HOOKS
// ========================================================================

uint32_t __fastcall LookupCursor_Hook(const char* cursorName) {
    if (cursorName != nullptr) {
        // Convert the requested name to lowercase for case-insensitive matching
        std::string nameLower = cursorName;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        // If we loaded a PNG with this name, return our custom index
        if (CursorNameToIndexMap.find(nameLower) != CursorNameToIndexMap.end()) {
            return CursorNameToIndexMap[nameLower];
        }
    }
    return LookupCursor_Original(cursorName);
}

HCURSOR WINAPI SetCursor_Hook(HCURSOR hCursor) {
    int currentIndex = *ActiveCursorIndex;
    if (currentIndex >= 42 && CustomHardwareCursors.find(currentIndex) != CustomHardwareCursors.end()) {
        return SetCursor_Original(CustomHardwareCursors[currentIndex]);
    }
    return SetCursor_Original(hCursor);
}

void __cdecl RenderCursor_Hook() {
    int currentIndex = *ActiveCursorIndex;
    if (currentIndex >= 42 && CustomCursorPixels.find(currentIndex) != CustomCursorPixels.end()) {
        uint32_t* hardwareBuffer = (uint32_t*)GetHardwareBuffer();
        if (hardwareBuffer != nullptr) {
            const std::vector<uint32_t>& pixels = CustomCursorPixels[currentIndex];
            for (int i = 0; i < 1024; ++i) hardwareBuffer[i] = pixels[i];
            PushToScreen();
        }
        return;
    }
    RenderCursor_Original();
}

// ========================================================================
// 5. MODULAR DIRECTORY SCANNER & MAIN THREAD
// ========================================================================

DWORD WINAPI MainThread(LPVOID lpReserved) {
    Sleep(2000);
    Log("--- OMNICURSOR DLL INITIALIZING ---");

    if (MH_Initialize() != MH_OK) return FALSE;

    // Scan the folder for all .png files
    WIN32_FIND_DATAA findFileData;
    HANDLE hFind = FindFirstFileA("Data\\Interface\\Cursor\\*.png", &findFileData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string fileName = findFileData.cFileName;

            // Strip the ".png" to get the cursor name
            std::string cursorName = fileName.substr(0, fileName.find_last_of("."));

            // Convert to lowercase
            std::transform(cursorName.begin(), cursorName.end(), cursorName.begin(), ::tolower);

            // Load the pixels
            std::string fullPath = "Data\\Interface\\Cursor\\" + fileName;
            std::vector<uint32_t> pixels = LoadCursorFromPNG(fullPath);

            if (!pixels.empty()) {
                // Register the new modular cursor
                CustomCursorPixels[NextAvailableIndex] = pixels;
                CustomHardwareCursors[NextAvailableIndex] = CreateAlphaCursor(pixels);
                CursorNameToIndexMap[cursorName] = NextAvailableIndex;

                Log("Loaded modular cursor: '" + cursorName + "' -> Assigned Index: " + std::to_string(NextAvailableIndex));
                NextAvailableIndex++;
            }
            else {
                Log("WARNING: Failed to parse 32x32 pixels from: " + fileName);
            }

        } while (FindNextFileA(hFind, &findFileData));
        FindClose(hFind);
    }
    else {
        Log("WARNING: No .png files found in Data/Interface/Cursor/");
    }

    MH_CreateHook((LPVOID)0x00523d40, &LookupCursor_Hook, reinterpret_cast<LPVOID*>(&LookupCursor_Original));
    MH_CreateHook((LPVOID)0x00523790, &RenderCursor_Hook, reinterpret_cast<LPVOID*>(&RenderCursor_Original));
    MH_CreateHook(&SetCursor, &SetCursor_Hook, reinterpret_cast<LPVOID*>(&SetCursor_Original));

    MH_EnableHook(MH_ALL_HOOKS);
    Log("SUCCESS: OmniCursor loaded.");

    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        // Clear out the old log file on a fresh boot
        std::ofstream logFile("Logs\\OmniCursor.log", std::ios_base::trunc);
        logFile.close();

        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}