#include "platform_helper.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>

// --- WINDOWS IMPLEMENTATION (NO RAYLIB HERE TO AVOID CONFLICTS) ---
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define Rectangle _WinRectangle
#define CloseWindow _WinCloseWindow
#define ShowCursor _WinShowCursor
#include <windows.h>
#include <shellapi.h>
#undef Rectangle
#undef CloseWindow
#undef ShowCursor

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ICON 101

static NOTIFYICONDATA nid = { 0 };
static HWND g_hwnd = NULL;

void PlatformInit(void* windowHandle) {
    g_hwnd = (HWND)windowHandle;
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = g_hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strncpy(nid.szTip, "Daily Focus Pro", 128);
    Shell_NotifyIcon(NIM_ADD, &nid);

    RegisterHotKey(g_hwnd, 1, MOD_ALT | MOD_NOREPEAT, 'P');
    RegisterHotKey(g_hwnd, 2, MOD_ALT | MOD_NOREPEAT, 'R');
}

void PlatformCleanup() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void ShowNotification(const char* title, const char* msg) {
    NOTIFYICONDATA nid_note = nid;
    nid_note.uFlags |= NIF_INFO;
    strncpy(nid_note.szInfoTitle, title, sizeof(nid_note.szInfoTitle) - 1);
    nid_note.szInfoTitle[sizeof(nid_note.szInfoTitle) - 1] = '\0';
    strncpy(nid_note.szInfo, msg, sizeof(nid_note.szInfo) - 1);
    nid_note.szInfo[sizeof(nid_note.szInfo) - 1] = '\0';
    nid_note.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIcon(NIM_MODIFY, &nid_note);
}

void LockScreen() {
    LockWorkStation();
}

void OpenExternal(const char* path) {
    if (path && strlen(path) > 0) {
        ShellExecuteA(NULL, "open", path, NULL, NULL, SW_SHOWNORMAL);
    }
}

int ProcessSystemEvents() {
    MSG msg = { 0 };
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_HOTKEY) return (int)msg.wParam;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

#undef LoadImage
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef DrawTextEx
#undef PlaySound
#undef ShowWindow
#undef Rectangle
#endif

// --- RAYLIB DEPENDENT CODE (IN SINGLE SECTION) ---
#include "raylib.h"

#if !defined(_WIN32)
// Fallback for non-windows
void PlatformInit(void* windowHandle) {}
void PlatformCleanup() {}
int ProcessSystemEvents() {
    if (IsKeyDown(KEY_LEFT_ALT)) {
        if (IsKeyPressed(KEY_P)) return 1;
        if (IsKeyPressed(KEY_R)) return 2;
    }
    return 0;
}
#endif

#if defined(__linux__)
void ShowNotification(const char* title, const char* msg) {
    std::string cmd = "notify-send \"" + std::string(title) + "\" \"" + std::string(msg) + "\"";
    int r = system(cmd.c_str()); (void)r;
}
void LockScreen() {
    int r = system("loginctl lock-session || xdg-screensaver lock || gnome-screensaver-command -l"); (void)r;
}
void OpenExternal(const char* path) {
    if (path && strlen(path) > 0) {
        std::string cmd = "xdg-open \"" + std::string(path) + "\" &";
        int r = system(cmd.c_str()); (void)r;
    }
}
#elif defined(__APPLE__)
void ShowNotification(const char* title, const char* msg) {
    std::string cmd = "osascript -e 'display notification \"" + std::string(msg) + "\" with title \"" + std::string(title) + "\"'";
    int r = system(cmd.c_str()); (void)r;
}
void LockScreen() {
    int r = system("osascript -e 'tell application \"System Events\" to lock screen'"); (void)r;
}
void OpenExternal(const char* path) {
    if (path && strlen(path) > 0) {
        std::string cmd = "open \"" + std::string(path) + "\"";
        int r = system(cmd.c_str()); (void)r;
    }
}
#endif

void SetAppIcon(const char* path) {
    Image icon = LoadImage(path);
    if (icon.data != NULL) {
        SetWindowIcon(icon);
        UnloadImage(icon);
    }
}
