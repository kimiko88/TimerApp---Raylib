#pragma once

// Unified Platform Interface
void PlatformInit(void* windowHandle);
void PlatformCleanup();
void ShowNotification(const char* title, const char* msg);
void LockScreen();
void OpenExternal(const char* path);
int ProcessSystemEvents(); // Returns hotkey ID (1=Start/Pause, 2=Reset) or 0
void SetAppIcon(const char* path);
