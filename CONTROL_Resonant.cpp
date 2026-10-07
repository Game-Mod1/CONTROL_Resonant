#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <commctrl.h>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define ID_BTN_ATTACH     1001
#define ID_CHK_GODMODE    1002
#define ID_CHK_HEALTH     1003
#define ID_CHK_FLY        1004
#define ID_CHK_DASHES     1005
#define ID_CHK_ENERGY     1006
#define ID_CHK_DAMAGE     1007
#define ID_CHK_STUN       1008
#define ID_CHK_SOURCE     1009
#define ID_CHK_UPGRADE    1010
#define ID_CHK_MATERIALS  1011
#define ID_BTN_TELEPORT   1012
#define ID_BTN_WAYPOINT   1013
#define ID_EDIT_SPEED     1014
#define ID_BTN_SPEED      1015
#define ID_STATUS         1016
#define ID_TIMER          2001

HWND g_hwnd = NULL;
HWND g_hStatus = NULL;
HANDLE g_hProcess = NULL;
DWORD g_dwPID = 0;
uintptr_t g_modBase = 0;
size_t g_modSize = 0;

bool g_god = false, g_health = false, g_fly = false, g_dashes = false;
bool g_energy = false, g_damage = false, g_stun = false, g_source = false;
bool g_upgrade = false, g_materials = false;
float g_speed = 1.0f;

uintptr_t addr_god = 0, addr_health = 0, addr_fly = 0, addr_dashes = 0;
uintptr_t addr_energy = 0, addr_dmg = 0, addr_stun = 0, addr_source = 0;
uintptr_t addr_upgrade = 0, addr_location = 0, addr_waypoint = 0, addr_player = 0;
uintptr_t addr_checkhit = 0, addr_currencies = 0, addr_itemedit = 0;

struct Pattern {
    const char* name;
    const BYTE* bytes;
    const char* mask;
    uintptr_t* result;
};

BYTE pat_god[]     = {0xC5,0x00,0x10,0x00,0x10,0xC5,0x00,0x11,0x00,0x00,0xC5,0xF9,0x7E,0xC8,0x85,0xC0,0x0F,0x84,0x00,0x00,0x00,0x00};
char  msk_god[]    = "x?x?x?x??xxxxx??xxxx";
BYTE pat_health[]  = {0xC5,0xFA,0x10,0x80,0x00,0x00,0x00,0x00,0xC5,0xF8,0x2F,0x00,0x76,0x00,0x00,0x83,0x00,0x00,0x00,0x74};
char  msk_health[] = "xxxx????xxx?x??x??x?";
BYTE pat_fly[]     = {0xC5,0xF8,0x11,0x8B,0x00,0x00,0x00,0x00,0x0F,0xB6,0x00,0x24,0x00,0x00,0x00,0x00,0x88,0x00,0x00,0x00,0x00,0x00};
char  msk_fly[]    = "xxxx????xx?x????x????x";
BYTE pat_dashes[]  = {0x43,0x80,0x7C,0xA8,0x08,0x00,0x74,0x00,0xFF,0x43,0x34};
char  msk_dashes[] = "xxxxxxx?xxx";
BYTE pat_energy[]  = {0xC5,0xFA,0x10,0x41,0x0C,0xC5,0xFA,0x5E,0x00,0x08,0x00,0x00,0x00,0x10,0x00,0xC5,0x00,0x5D};
char  msk_energy[] = "xxxxxxxx?x???x?x?x";
BYTE pat_dmg[]     = {0xC4,0xC1,0x7A,0x2A,0x44,0x24,0x04};
char  msk_dmg[]    = "xxxxxxx";
BYTE pat_stun[]    = {0xC5,0xFA,0x10,0x43,0x08,0xC5,0xFA,0x5C,0xD1};
char  msk_stun[]   = "xxxxxxxxx";
BYTE pat_source[]  = {0x8B,0x51,0x04,0x00,0x8B,0x00,0x8B,0x09};
char  msk_source[] = "xxx?x?xx";
BYTE pat_upgrade[] = {0x8B,0x43,0x04,0x89,0x84,0x24,0x00,0x00,0x00,0x00};
char  msk_upgrade[]= "xxxxxx????";
BYTE pat_location[]= {0xC5,0xFA,0x10,0x80,0x00,0x00,0x00,0x00,0xC5,0xF8,0x2F};
char  msk_location[]="xxxx????xxx";
BYTE pat_waypoint[]= {0x4C,0x8B,0x00,0x00,0xC4,0xA1,0x48,0x5C,0x4C,0x0F,0x10};
char  msk_waypoint[]="xx??xxxxxxx";
BYTE pat_player[]  = {0xC5,0xF0,0x57,0x00,0xC5,0xF2,0x2A,0x49,0x04,0xC5,0xF2,0x59,0x01,0xC5,0xF2,0x5C};
char  msk_player[] = "xxx?xxxxxxxxxxxx";
BYTE pat_checkhit[]= {0x48,0xC1,0x00,0x00,0x0F,0xB6,0x54,0x01,0x30,0xF6,0xC2,0x10,0x74};
char  msk_checkhit[]="xx??xxxxxxxxx";
BYTE pat_curr[]    = {0x33,0xD2,0x8B,0x0C,0xD9,0xE8,0x00,0x00,0x00,0x00,0xEB};
char  msk_curr[]   = "xxxxxx????x";
BYTE pat_item[]    = {0xC5,0x00,0x2A,0x00,0xE8};
char  msk_item[]   = "x?x?x";

bool MatchPattern(const BYTE* data, const BYTE* pat, const char* mask, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (mask[i] == 'x' && data[i] != pat[i]) return false;
    }
    return true;
}

uintptr_t PatternScan(HANDLE hProc, uintptr_t start, size_t size, const BYTE* pat, const char* mask) {
    size_t plen = strlen(mask);
    const size_t CHUNK = 0x10000;
    std::vector<BYTE> buf(CHUNK + plen);
    for (size_t offset = 0; offset < size; offset += CHUNK) {
        size_t toRead = (std::min)(CHUNK + plen, size - offset);
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(hProc, (LPCVOID)(start + offset), buf.data(), toRead, &bytesRead) || bytesRead < plen)
            continue;
        for (size_t i = 0; i + plen <= bytesRead; i++) {
            if (MatchPattern(buf.data() + i, pat, mask, plen))
                return start + offset + i;
        }
    }
    return 0;
}

DWORD FindProcess(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = {sizeof(pe)};
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

uintptr_t GetModuleBase(DWORD pid, const wchar_t* modName, size_t* outSize) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me = {sizeof(me)};
    uintptr_t base = 0;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, modName) == 0) {
                base = (uintptr_t)me.modBaseAddr;
                if (outSize) *outSize = me.modBaseSize;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return base;
}

bool WriteBytes(uintptr_t addr, const BYTE* data, size_t len) {
    if (!g_hProcess || !addr) return false;
    DWORD old;
    VirtualProtectEx(g_hProcess, (LPVOID)addr, len, PAGE_EXECUTE_READWRITE, &old);
    SIZE_T written = 0;
    BOOL ok = WriteProcessMemory(g_hProcess, (LPVOID)addr, data, len, &written);
    VirtualProtectEx(g_hProcess, (LPVOID)addr, len, old, &old);
    return ok && written == len;
}

bool NopBytes(uintptr_t addr, size_t len) {
    std::vector<BYTE> nops(len, 0x90);
    return WriteBytes(addr, nops.data(), len);
}

bool ReadMem(uintptr_t addr, void* buf, size_t len) {
    SIZE_T r = 0;
    return ReadProcessMemory(g_hProcess, (LPCVOID)addr, buf, len, &r) && r == len;
}

bool WriteMem(uintptr_t addr, const void* buf, size_t len) {
    return WriteBytes(addr, (const BYTE*)buf, len);
}

void SetStatus(const wchar_t* msg) {
    if (g_hStatus) SetWindowTextW(g_hStatus, msg);
}

void ApplyGodMode(bool on) {
    if (!addr_god) return;
    if (on) {
        BYTE patch[] = {0x90,0x90,0x90,0x90,0x90,0x90};
        WriteBytes(addr_god + 0x0E, patch, 6);
    } else {
        BYTE orig[] = {0x0F,0x84};
        WriteBytes(addr_god + 0x0E, orig, 2);
    }
}

void ApplyHealth(bool on) {
    if (!addr_health) return;
    if (on) {
        BYTE patch[] = {0x90,0x90};
        WriteBytes(addr_health + 0x0B, patch, 2);
    }
}

void ApplyFly(bool on) {
    if (!addr_fly) return;
    if (on) {
        BYTE patch[] = {0xC6,0x00,0x01};
        WriteBytes(addr_fly + 0x08, patch, 3);
    }
}

void ApplyDashes(bool on) {
    if (!addr_dashes) return;
    if (on) {
        BYTE patch[] = {0x90,0x90};
        WriteBytes(addr_dashes + 0x06, patch, 2);
    } else {
        BYTE orig[] = {0x74};
        WriteBytes(addr_dashes + 0x06, orig, 1);
    }
}

void ApplyEnergy(bool on) {
    if (!addr_energy) return;
    if (on) {
        BYTE patch[] = {0x90,0x90,0x90,0x90};
        WriteBytes(addr_energy + 0x05, patch, 4);
    }
}

void ApplyDamage(bool on) {
    if (!addr_dmg) return;
    if (on) {
        float mult = 10.0f;
        WriteMem(addr_dmg + 0x07, &mult, 4);
    }
}

void ApplyStun(bool on) {
    if (!addr_stun) return;
    if (on) {
        float longstun = 30.0f;
        WriteMem(addr_stun + 0x04, &longstun, 4);
    }
}

void ApplySource(bool on) {
    if (!addr_source) return;
    if (on) {
        int val = 999999;
        WriteMem(addr_source + 0x03, &val, 4);
    }
}

void ApplyUpgrade(bool on) {
    if (!addr_upgrade) return;
    if (on) {
        BYTE patch[] = {0x31,0xC0,0x90,0x90};
        WriteBytes(addr_upgrade, patch, 4);
    }
}

void ApplyMaterials(bool on) {
    if (!addr_currencies) return;
    if (on) {
        int val = 99999;
        WriteMem(addr_currencies + 0x02, &val, 4);
    }
}

void DoTeleport() {
    if (!addr_location || !g_hProcess) return;
    float pos[3] = {0};
    if (ReadMem(addr_location + 0x04, pos, 12)) {
        pos[2] += 500.0f;
        WriteMem(addr_location + 0x04, pos, 12);
        SetStatus(L"Teleported +Z");
    }
}

void DoWaypoint() {
    if (!addr_waypoint || !g_hProcess) return;
    float pos[3] = {0};
    if (ReadMem(addr_location + 0x04, pos, 12)) {
        WriteMem(addr_waypoint + 0x04, pos, 12);
        SetStatus(L"Waypoint set");
    }
}

void ApplySpeed() {
    if (!g_hProcess) return;
    wchar_t buf[32];
    GetDlgItemTextW(g_hwnd, ID_EDIT_SPEED, buf, 32);
    g_speed = (float)_wtof(buf);
    if (g_speed < 0.1f) g_speed = 0.1f;
    if (g_speed > 20.0f) g_speed = 20.0f;
    SetStatus(L"Speed applied");
}

bool Attach() {
    if (g_hProcess) {
        CloseHandle(g_hProcess);
        g_hProcess = NULL;
    }
    g_dwPID = FindProcess(L"CONTROLResonant.exe");
    if (!g_dwPID) {
        SetStatus(L"Process not found");
        return false;
    }
    g_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_dwPID);
    if (!g_hProcess) {
        SetStatus(L"OpenProcess failed");
        return false;
    }
    g_modBase = GetModuleBase(g_dwPID, L"CONTROLResonant.exe", &g_modSize);
    if (!g_modBase || !g_modSize) {
        SetStatus(L"Module not found");
        return false;
    }
    SetStatus(L"Scanning patterns...");
    addr_god       = PatternScan(g_hProcess, g_modBase, g_modSize, pat_god, msk_god);
    addr_health    = PatternScan(g_hProcess, g_modBase, g_modSize, pat_health, msk_health);
    addr_fly       = PatternScan(g_hProcess, g_modBase, g_modSize, pat_fly, msk_fly);
    addr_dashes    = PatternScan(g_hProcess, g_modBase, g_modSize, pat_dashes, msk_dashes);
    addr_energy    = PatternScan(g_hProcess, g_modBase, g_modSize, pat_energy, msk_energy);
    addr_dmg       = PatternScan(g_hProcess, g_modBase, g_modSize, pat_dmg, msk_dmg);
    addr_stun      = PatternScan(g_hProcess, g_modBase, g_modSize, pat_stun, msk_stun);
    addr_source    = PatternScan(g_hProcess, g_modBase, g_modSize, pat_source, msk_source);
    addr_upgrade   = PatternScan(g_hProcess, g_modBase, g_modSize, pat_upgrade, msk_upgrade);
    addr_location  = PatternScan(g_hProcess, g_modBase, g_modSize, pat_location, msk_location);
    addr_waypoint  = PatternScan(g_hProcess, g_modBase, g_modSize, pat_waypoint, msk_waypoint);
    addr_player    = PatternScan(g_hProcess, g_modBase, g_modSize, pat_player, msk_player);
    addr_checkhit  = PatternScan(g_hProcess, g_modBase, g_modSize, pat_checkhit, msk_checkhit);
    addr_currencies= PatternScan(g_hProcess, g_modBase, g_modSize, pat_curr, msk_curr);
    addr_itemedit  = PatternScan(g_hProcess, g_modBase, g_modSize, pat_item, msk_item);

    int found = 0;
    if (addr_god) found++;
    if (addr_health) found++;
    if (addr_fly) found++;
    if (addr_dashes) found++;
    if (addr_energy) found++;
    if (addr_dmg) found++;
    if (addr_stun) found++;
    if (addr_source) found++;
    if (addr_upgrade) found++;
    if (addr_location) found++;
    if (addr_waypoint) found++;
    if (addr_player) found++;
    if (addr_checkhit) found++;
    if (addr_currencies) found++;
    if (addr_itemedit) found++;

    wchar_t msg[128];
    swprintf_s(msg, L"Attached PID %lu | Patterns: %d/15", g_dwPID, found);
    SetStatus(msg);
    return true;
}

void OnTimer() {
    if (!g_hProcess) return;
    DWORD code = 0;
    if (!GetExitCodeProcess(g_hProcess, &code) || code != STILL_ACTIVE) {
        CloseHandle(g_hProcess);
        g_hProcess = NULL;
        SetStatus(L"Process closed");
        return;
    }
    if (g_god) ApplyGodMode(true);
    if (g_health) ApplyHealth(true);
    if (g_fly) ApplyFly(true);
    if (g_dashes) ApplyDashes(true);
    if (g_energy) ApplyEnergy(true);
    if (g_damage) ApplyDamage(true);
    if (g_stun) ApplyStun(true);
    if (g_source) ApplySource(true);
    if (g_upgrade) ApplyUpgrade(true);
    if (g_materials) ApplyMaterials(true);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        int y = 12;
        CreateWindowW(L"BUTTON", L"Attach to CONTROLResonant.exe", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, y, 280, 28, hwnd, (HMENU)ID_BTN_ATTACH, NULL, NULL);
        y += 36;
        CreateWindowW(L"BUTTON", L"God Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            12, y, 140, 22, hwnd, (HMENU)ID_CHK_GODMODE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Infinite Health", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            160, y, 140, 22, hwnd, (HMENU)ID_CHK_HEALTH, NULL, NULL);
        y += 26;
        CreateWindowW(L"BUTTON", L"Fly Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            12, y, 140, 22, hwnd, (HMENU)ID_CHK_FLY, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Infinite Air Dashes", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            160, y, 140, 22, hwnd, (HMENU)ID_CHK_DASHES, NULL, NULL);
        y += 26;
        CreateWindowW(L"BUTTON", L"Infinite Energy", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            12, y, 140, 22, hwnd, (HMENU)ID_CHK_ENERGY, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Damage Multi x10", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            160, y, 140, 22, hwnd, (HMENU)ID_CHK_DAMAGE, NULL, NULL);
        y += 26;
        CreateWindowW(L"BUTTON", L"Long Stun", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            12, y, 140, 22, hwnd, (HMENU)ID_CHK_STUN, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Source Points", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            160, y, 140, 22, hwnd, (HMENU)ID_CHK_SOURCE, NULL, NULL);
        y += 26;
        CreateWindowW(L"BUTTON", L"No Upgrade Cost", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            12, y, 140, 22, hwnd, (HMENU)ID_CHK_UPGRADE, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Max Materials", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            160, y, 140, 22, hwnd, (HMENU)ID_CHK_MATERIALS, NULL, NULL);
        y += 32;
        CreateWindowW(L"BUTTON", L"Teleport +Z", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            12, y, 130, 26, hwnd, (HMENU)ID_BTN_TELEPORT, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Set Waypoint", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            150, y, 130, 26, hwnd, (HMENU)ID_BTN_WAYPOINT, NULL, NULL);
        y += 34;
        CreateWindowW(L"STATIC", L"Game Speed:", WS_CHILD | WS_VISIBLE,
            12, y + 2, 80, 20, hwnd, NULL, NULL, NULL);
        CreateWindowW(L"EDIT", L"1.0", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            95, y, 60, 22, hwnd, (HMENU)ID_EDIT_SPEED, NULL, NULL);
        CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            165, y, 70, 22, hwnd, (HMENU)ID_BTN_SPEED, NULL, NULL);
        y += 36;
        g_hStatus = CreateWindowW(L"STATIC", L"Ready - Attach to game", WS_CHILD | WS_VISIBLE | SS_LEFT,
            12, y, 280, 22, hwnd, (HMENU)ID_STATUS, NULL, NULL);
        EnumChildWindows(hwnd, [](HWND h, LPARAM f) -> BOOL {
            SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
            return TRUE;
        }, (LPARAM)hFont);
        SetTimer(hwnd, ID_TIMER, 200, NULL);
        break;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_BTN_ATTACH) {
            Attach();
        } else if (id == ID_CHK_GODMODE) {
            g_god = (IsDlgButtonChecked(hwnd, ID_CHK_GODMODE) == BST_CHECKED);
            ApplyGodMode(g_god);
        } else if (id == ID_CHK_HEALTH) {
            g_health = (IsDlgButtonChecked(hwnd, ID_CHK_HEALTH) == BST_CHECKED);
            ApplyHealth(g_health);
        } else if (id == ID_CHK_FLY) {
            g_fly = (IsDlgButtonChecked(hwnd, ID_CHK_FLY) == BST_CHECKED);
            ApplyFly(g_fly);
        } else if (id == ID_CHK_DASHES) {
            g_dashes = (IsDlgButtonChecked(hwnd, ID_CHK_DASHES) == BST_CHECKED);
            ApplyDashes(g_dashes);
        } else if (id == ID_CHK_ENERGY) {
            g_energy = (IsDlgButtonChecked(hwnd, ID_CHK_ENERGY) == BST_CHECKED);
            ApplyEnergy(g_energy);
        } else if (id == ID_CHK_DAMAGE) {
            g_damage = (IsDlgButtonChecked(hwnd, ID_CHK_DAMAGE) == BST_CHECKED);
            ApplyDamage(g_damage);
        } else if (id == ID_CHK_STUN) {
            g_stun = (IsDlgButtonChecked(hwnd, ID_CHK_STUN) == BST_CHECKED);
            ApplyStun(g_stun);
        } else if (id == ID_CHK_SOURCE) {
            g_source = (IsDlgButtonChecked(hwnd, ID_CHK_SOURCE) == BST_CHECKED);
            ApplySource(g_source);
        } else if (id == ID_CHK_UPGRADE) {
            g_upgrade = (IsDlgButtonChecked(hwnd, ID_CHK_UPGRADE) == BST_CHECKED);
            ApplyUpgrade(g_upgrade);
        } else if (id == ID_CHK_MATERIALS) {
            g_materials = (IsDlgButtonChecked(hwnd, ID_CHK_MATERIALS) == BST_CHECKED);
            ApplyMaterials(g_materials);
        } else if (id == ID_BTN_TELEPORT) {
            DoTeleport();
        } else if (id == ID_BTN_WAYPOINT) {
            DoWaypoint();
        } else if (id == ID_BTN_SPEED) {
            ApplySpeed();
        }
        break;
    }
    case WM_TIMER:
        if (wParam == ID_TIMER) OnTimer();
        break;
    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        if (g_hProcess) CloseHandle(g_hProcess);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    WNDCLASSEXW wc = {sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ControlResonantTrainer";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);
    g_hwnd = CreateWindowExW(0, L"ControlResonantTrainer", L"CONTROL Resonant Trainer v1.0-v1.4.x",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 320, 380, NULL, NULL, hInst, NULL);
    ShowWindow(g_hwnd, nShow);
    UpdateWindow(g_hwnd);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
