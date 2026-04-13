#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <math.h>
#include <algorithm>

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "include/raygui.h"

#include "storage.hpp"
#include "timer.hpp"
#include "platform_helper.hpp"

// --- Focus Management ---
enum FocusID { FOCUS_NONE, FOCUS_NEW_ACT, FOCUS_NEW_MINS, FOCUS_NEW_SECS, FOCUS_EDIT_ACT, FOCUS_EDIT_MINS, FOCUS_EDIT_SECS, FOCUS_ROUTINE_NAME, FOCUS_EDITOR_NAME, FOCUS_NEW_CAT, FOCUS_EDIT_SOUND, FOCUS_EDIT_LAUNCH, FOCUS_NEW_SOUND, FOCUS_NEW_LAUNCH };
FocusID currentFocus = FOCUS_NONE;

// --- Theme Definitions ---
struct Theme {
    std::string name; Color base; Color text; Color subtext; Color accent; Color surface;
};

const std::vector<Theme> THEMES = {
    {"Dracula",   { 40, 42, 54, 255 },    { 220, 220, 220, 255 }, { 98, 114, 164, 255 }, { 189, 147, 249, 255 }, { 68, 71, 90, 255 }},
    {"Solarized", { 253, 246, 227, 255 }, { 7, 54, 66, 255 },     { 88, 110, 117, 255 }, { 38, 139, 210, 255 },  { 238, 232, 213, 255 }},
    {"Nord",      { 46, 52, 64, 255 },    { 216, 222, 233, 255 }, { 76, 86, 106, 255 },  { 136, 192, 208, 255 }, { 59, 66, 82, 255 }},
    {"Rose Pine", { 25, 23, 36, 255 },    { 224, 222, 244, 255 }, { 110, 106, 134, 255 }, { 235, 188, 186, 255 }, { 31, 29, 46, 255 }}
};

const int SCREEN_WIDTH = 450;
const int SCREEN_HEIGHT = 650;

// --- App State ---
Config config; Storage::DailyStats stats; Timer timer;
bool showStats = false, showSettings = false, showActivities = false, showRoutines = false, showQueue = false;
void* windowHandle = nullptr; Activity* currentActivity = nullptr; Font mainFont; Font uiFont;
float pulseTime = 0.0f; double lastMetronomeTick = 0; int selectedThemeIdx = 0; float catScrollOffset = 0.0f;
float viewAlpha = 0.0f; bool isTransitioning = false;

// Temp UI state
char newActTitle[128] = ""; char newActMinsStr[16] = "25", newActSecsStr[16] = "00";
int editingActIdx = -1; char editTitle[128] = ""; char editMinsStr[16] = "0", editSecsStr[16] = "0";
char routineName[128] = "My Routine";
int newActCatIdx = 0; int editingActCatIdx = 0;
char newCatName[128] = "";
char editSoundPath[256] = "", editLaunchPath[256] = "";
char newSoundPath[256] = "", newLaunchPath[256] = "";

int editingRoutineIdx = -2; Routine tempRoutine; char tempRoutineName[128] = "";
bool isMiniMode = false; bool isDragging = false; Vector2 dragOffset = { 0 };
double lastSaveEyesLog = 0; Sound lastCustomSound = { 0 };

// --- Tooltip State ---
std::string currentTooltip = "";
void DrawTooltip(Font font, Theme t) {
    if (currentTooltip.empty()) return;
    int size = 14; Vector2 mPos = GetMousePosition(); Vector2 txtSize = MeasureTextEx(font, currentTooltip.c_str(), size, 1);
    Rectangle rect = { mPos.x + 15, mPos.y + 15, txtSize.x + 20, txtSize.y + 12 };
    if (rect.x + rect.width > (isMiniMode ? 150 : SCREEN_WIDTH)) rect.x = mPos.x - rect.width - 5;
    DrawRectangleRounded(rect, 0.4, 6, {20, 20, 20, 240}); DrawRectangleRoundedLines(rect, 0.4, 6, t.accent);
    DrawTextEx(font, currentTooltip.c_str(), { rect.x + 10, rect.y + 6 }, size, 1, WHITE);
}

// --- Audio Engine ---
void PlaySynthPreset(std::string presetName, std::string customPath = "") {
    static Sound lastSynthSound = { 0 };
    
    // Stop and unload previous synth sound to prevent memory leaks
    if (lastSynthSound.frameCount > 0) {
        if (IsSoundPlaying(lastSynthSound)) StopSound(lastSynthSound);
        UnloadSound(lastSynthSound);
        lastSynthSound = (Sound){ 0 };
    }

    if (!customPath.empty()) {
        if (lastCustomSound.frameCount > 0) UnloadSound(lastCustomSound);
        lastCustomSound = LoadSound(customPath.c_str());
        if (lastCustomSound.frameCount > 0) { PlaySound(lastCustomSound); return; }
    }
    int samplesCount = 44100 * 0.6; short* samples = (short*)malloc(samplesCount * sizeof(short));
    for (int i = 0; i < samplesCount; i++) {
        float t = (float)i / 44100.0f; float vol = 0.8f; float signal = 0;
        if (presetName == "ding") { vol *= expf(-10.0f * t); signal = sinf(2.0f * PI * 880.0f * t); }
        else if (presetName == "chime") { vol *= expf(-3.0f * t); signal = (sinf(2.0f * PI * 440.0f * t) * 0.5f) + (sinf(2.0f * PI * 660.0f * t) * 0.3f); }
        else if (presetName == "bell") { vol *= (1.0f - t/0.6f); signal = sinf(2.0f * PI * 330.0f * t) * 0.7f; }
        else if (presetName == "woodblock") { vol *= expf(-40.0f * t); signal = (t < 0.05f) ? sinf(2.0f * PI * 1200.0f * t) : 0; }
        else if (presetName == "tick") { vol = 0.3f * expf(-50.0f * t); signal = sinf(2.0f * PI * 330.0f * t); if (i > 44100 * 0.1) break; }
        samples[i] = (short)(30000.0f * vol * signal);
    }
    Wave wave = { (unsigned int)samplesCount, 44100, 16, 1, samples };
    lastSynthSound = LoadSoundFromWave(wave); PlaySound(lastSynthSound); free(samples);
}

// --- Helpers ---
void ProcessNextInQueue() {
    if (config.queue.empty()) { currentActivity = nullptr; return; }
    Activity next = config.queue.front(); config.queue.erase(config.queue.begin());
    timer.overrideDuration(next.initialSeconds);
    currentActivity = nullptr; for(auto& a : config.activities) if(a.id == next.id) currentActivity = &a;
    if (config.autoPlay) { timer.start(); 
        if (currentActivity && !currentActivity->autoLaunchPath.empty()) OpenExternal(currentActivity->autoLaunchPath.c_str());
        PlaySynthPreset(config.selectedSound, currentActivity ? currentActivity->customSoundPath : ""); 
    }
}

std::string GetDateOffset(int days) {
    std::time_t t = std::time(nullptr); t += (days * 24 * 60 * 60);
    auto tm = *std::localtime(&t); std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d"); return oss.str();
}

Color GetContrastColor(Color bg) {
    float luminance = (0.299f * bg.r + 0.587f * bg.g + 0.114f * bg.b) / 255.0f;
    return (luminance > 0.65f) ? (Color){15, 15, 15, 255} : (Color){255, 255, 255, 255};
}

void ApplyStyle() {
    Theme cur = THEMES[selectedThemeIdx]; GuiSetStyle(DEFAULT, TEXT_SIZE, 18); GuiSetStyle(BUTTON, BORDER_WIDTH, 0);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(cur.surface)); GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL, ColorToInt(cur.text));
    GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED, ColorToInt(cur.accent)); 
    
    // Theme-specific hover overrides
    if (selectedThemeIdx == 0) // Dracula
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToInt(WHITE));
    else
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED, ColorToInt(GetContrastColor(cur.accent)));

    // Contextual input coloring: White (Idle) / Black (Active)
    GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL, ColorToInt(WHITE));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_FOCUSED, ColorToInt(BLACK));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_PRESSED, ColorToInt(BLACK));
}

void DrawSharpText(Font font, const char* text, Vector2 pos, float size, float spacing, Color color) {
    DrawTextEx(font, text, { (float)((int)pos.x), (float)((int)pos.y) }, (float)((int)size), spacing, color);
}

void DrawPieChart(Vector2 center, float radius, std::map<std::string, float> data, Theme t) {
    float total = 0; for (auto const& [cat, val] : data) total += val;
    if (total <= 0) { DrawCircleLines(center.x, center.y, radius, t.surface); return; }
    float startAngle = 0; int colorIdx = 0;
    Color palette[] = { t.accent, {255, 100, 100, 255}, {100, 255, 100, 255}, {100, 100, 255, 255}, {255, 255, 100, 255} };
    for (auto const& [cat, val] : data) {
        float angle = (val / total) * 360.0f;
        DrawCircleSector(center, radius, startAngle, startAngle + angle, 32, palette[colorIdx % 5]);
        float midAngle = (startAngle + angle / 2.0f) * DEG2RAD;
        if (angle > 15) DrawSharpText(mainFont, cat.c_str(), { center.x + cosf(midAngle)*radius*0.7f - 20, center.y + sinf(midAngle)*radius*0.7f - 10 }, 10, 1, WHITE);
        startAngle += angle; colorIdx++;
    }
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_TRANSPARENT); 
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Daily Focus Pro");
    InitAudioDevice(); SetTargetFPS(60);
    int codepoints[256]; for (int i = 0; i < 256; i++) codepoints[i] = i;
    mainFont = LoadFontEx("assets/Roboto-Regular.ttf", 64, codepoints, 256); 
    if (mainFont.texture.id == 0) mainFont = GetFontDefault(); // Fallback
    GenTextureMipmaps(&mainFont.texture);
    SetTextureFilter(mainFont.texture, TEXTURE_FILTER_TRILINEAR); 

    // Dedicated UI font for crisp inputs (pixel-perfect size 20)
    uiFont = LoadFontEx("assets/Roboto-Regular.ttf", 20, codepoints, 256);
    if (uiFont.texture.id == 0) uiFont = GetFontDefault(); // Fallback
    SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
    GuiSetFont(uiFont);
    windowHandle = GetWindowHandle(); SetAppIcon("assets/app_icon.png");
    PlatformInit(windowHandle);
    config = Storage::loadConfig(); stats = Storage::loadStats();
    if (config.categories.empty()) config.categories = {"Work", "Study", "Workout", "Other"};
    for(size_t i=0; i<THEMES.size(); i++) if(THEMES[i].name == config.selectedTheme) selectedThemeIdx = i;
    ApplyStyle(); float tempBpm = (float)config.bpm;

    // Restore Session
    if (config.currentFocusActivityID != -1) {
        for (auto& a : config.activities) {
            if (a.id == config.currentFocusActivityID) {
                currentActivity = &a;
                timer.overrideDuration(a.initialSeconds);
                if (config.currentFocusRemainingSeconds > 0 && config.currentFocusRemainingSeconds < a.initialSeconds) {
                    // We don't have a direct "set remaining" but we can hack it or add it to timer.hpp
                    // For now, let's just override and see
                    timer.overrideDuration(config.currentFocusRemainingSeconds);
                }
                break;
            }
        }
    }

    while (!WindowShouldClose()) {
        // Handle Window Dragging in Mini-Mode
        if (isMiniMode) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { 
                isDragging = true; 
                dragOffset = GetMousePosition(); 
            }
            if (isDragging) {
                Vector2 mPos = GetMousePosition();
                Vector2 wPos = GetWindowPosition();
                SetWindowPosition(wPos.x + (mPos.x - dragOffset.x), wPos.y + (mPos.y - dragOffset.y));
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) isDragging = false;
            }
        }
        int hk = ProcessSystemEvents(); if (hk == 1) { if (timer.isRunning()) timer.pause(); else { timer.start(); if(currentActivity && !currentActivity->autoLaunchPath.empty()) OpenExternal(currentActivity->autoLaunchPath.c_str()); } } else if (hk == 2) timer.reset();
        timer.update(); if (timer.isRunning() && config.usePulseAnimation) pulseTime += GetFrameTime(); else pulseTime = 0;
        
        if (config.eyeSafeEnabled) {
            config.eyeSafeRemainingSeconds -= GetFrameTime();
            if (GetTime() - lastSaveEyesLog > 5.0) { Storage::saveConfig(config); lastSaveEyesLog = GetTime(); }
            if (config.eyeSafeRemainingSeconds <= 300.2 && config.eyeSafeRemainingSeconds > 300.0) ShowNotification("EyeSafe", "Break in 5 minutes!");
            if (!config.isInEyeSafeBreak && config.eyeSafeRemainingSeconds <= 0) {
                LockScreen(); config.isInEyeSafeBreak = true; config.eyeSafeRemainingSeconds = 900;
                Storage::saveConfig(config);
            } else if (config.isInEyeSafeBreak && config.eyeSafeRemainingSeconds <= 0) {
                config.isInEyeSafeBreak = false; config.eyeSafeRemainingSeconds = 7200;
                ShowNotification("EyeSafe", "You can now resume working!");
                Storage::saveConfig(config);
            }
        }

        // Periodic Persistence
        static double lastHealthSave = 0;
        if (timer.isRunning() && GetTime() - lastHealthSave > 2.0) {
            config.currentFocusRemainingSeconds = timer.getRemainingSeconds();
            config.currentFocusActivityID = currentActivity ? currentActivity->id : -1;
            Storage::saveConfig(config);
            lastHealthSave = GetTime();
        }

        if (config.isMetronomePlaying && (GetTime() - lastMetronomeTick >= (60.0 / (double)config.bpm))) { PlaySynthPreset("tick"); lastMetronomeTick = GetTime(); }
        if (timer.hasCompleted()) {
            PlaySynthPreset(config.selectedSound, currentActivity ? currentActivity->customSoundPath : ""); ShowNotification("Focus Completed!", "Task finished.");
            float mins = timer.getTotalSeconds() / 60.0f; config.totalFocusMinutes += mins;
            std::string d = GetDateOffset(0); std::string cat = (currentActivity) ? currentActivity->category : "Focus";
            if (!stats[d].count(cat)) { stats[d][cat] = 0; } stats[d][cat] += mins;
            Storage::saveStats(stats); if (!config.queue.empty() && config.autoPlay) ProcessNextInQueue(); Storage::saveConfig(config);
        }

        BeginDrawing(); Theme t = THEMES[selectedThemeIdx]; 
        if (isMiniMode && config.useTransparency) ClearBackground(BLANK); else ClearBackground(t.base);

        // Alpha Transition Logic (Fade out overlay)
        if (isTransitioning) {
            viewAlpha -= 3.0f * GetFrameTime();
            if (viewAlpha <= 0.0f) { viewAlpha = 0.0f; isTransitioning = false; }
        }
        
        BeginBlendMode(BLEND_ALPHA);

        if (isMiniMode) {
             DrawRectangleRounded({ 5, 5, 130, 35 }, 0.5, 8, config.useTransparency ? (Color){t.surface.r, t.surface.g, t.surface.b, 160} : t.surface);
             int rem = timer.getRemainingSeconds();
             DrawSharpText(mainFont, TextFormat("%02d:%02d", rem/60, rem%60), { 12, 12 }, 18, 1, t.text);
             if (GuiButton({ 75, 8, 28, 28 }, timer.isRunning()?"#132#":"#131#")) { if(timer.isRunning()) timer.pause(); else timer.start(); }
             if (GuiButton({ 105, 8, 25, 25 }, "#117#")) { isMiniMode = false; SetWindowSize(SCREEN_WIDTH, SCREEN_HEIGHT); ClearWindowState(FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNDECORATED); }
        } else {
            DrawSharpText(mainFont, "DAILY FOCUS", { 25, 25 }, 24, 1, t.accent);
            if (GuiButton({ SCREEN_WIDTH - 65, 20, 40, 30 }, config.isDarkMode ? "#01#" : "#02#")) { config.isDarkMode = !config.isDarkMode; selectedThemeIdx = config.isDarkMode ? 0 : 1; config.selectedTheme = THEMES[selectedThemeIdx].name; ApplyStyle(); Storage::saveConfig(config); }
            if (GuiButton({ SCREEN_WIDTH - 110, 20, 40, 30 }, "#140#")) { showSettings = !showSettings; showStats = false; showActivities = false; showRoutines = false; showQueue = false; editingRoutineIdx = -2; }
            if (GuiButton({ SCREEN_WIDTH - 155, 20, 40, 30 }, "#118#")) { isMiniMode = true; SetWindowSize(140, 45); SetWindowState(FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNDECORATED); }
            if (CheckCollisionPointRec(GetMousePosition(), { SCREEN_WIDTH - 155, 20, 40, 30 })) currentTooltip = "Mini-Timer Overlay";

            if (GuiButton({20, 65, 75, 30}, (showStats||showSettings||showActivities||showRoutines||showQueue)?"BACK":"TIMER")) { 
                if (editingRoutineIdx != -2) { editingRoutineIdx = -2; viewAlpha = 1.0f; isTransitioning = true; }
                else { showStats=showSettings=showActivities=showRoutines=showQueue=false; editingActIdx=-1; currentFocus=FOCUS_NONE; viewAlpha = 1.0f; isTransitioning = true; }
            }
            if (editingRoutineIdx == -2 && GuiButton({100, 65, 85, 30}, "ANALYTICS")) { showStats=true; showSettings=showActivities=showRoutines=showQueue=false; viewAlpha = 1.0f; isTransitioning = true; }
            if (editingRoutineIdx == -2 && GuiButton({190, 65, 80, 30}, "ACTIVITIES")) { showActivities=true; showSettings=showStats=showRoutines=showQueue=false; viewAlpha = 1.0f; isTransitioning = true; }
            if (editingRoutineIdx == -2 && GuiButton({275, 65, 80, 30}, "ROUTINES")) { showRoutines=true; showSettings=showStats=showActivities=showQueue=false; viewAlpha = 1.0f; isTransitioning = true; }
            if (editingRoutineIdx == -2 && GuiButton({360, 65, 70, 30}, "QUEUE")) { showQueue=true; showSettings=showStats=showActivities=showRoutines=false; viewAlpha = 1.0f; isTransitioning = true; }

            if (showSettings) {
                DrawSharpText(mainFont, "SETTINGS", { 25, 110 }, 22, 1, t.text); int sy = 160;
                GuiCheckBox({ 25, (float)sy, 24, 24 }, "MINIMAL MODE", &config.isMinimalist); sy += 45;
                GuiCheckBox({ 25, (float)sy, 24, 24 }, "TRANSPARENCY", &config.useTransparency); sy += 45;
                DrawSharpText(mainFont, "THEME", { 25, (float)sy }, 14, 1, t.subtext); sy += 25;
                for (int i=0; i<(int)THEMES.size(); i++) {
                    if (GuiButton({ 25.0f + i*102, (float)sy, 100, 30 }, THEMES[i].name.c_str())) { selectedThemeIdx = i; config.selectedTheme = THEMES[i].name; ApplyStyle(); Storage::saveConfig(config); }
                }
                sy += 45;
                GuiCheckBox({ 25, (float)sy, 24, 24 }, "EYESAFE (2H LOCK)", &config.eyeSafeEnabled); 
                DrawSharpText(mainFont, TextFormat("REMAINING: %dm", (int)config.eyeSafeRemainingSeconds/60), { 280, (float)sy+5 }, 12, 1, t.accent); sy += 45;
                GuiCheckBox({ 25, (float)sy, 24, 24 }, "METRONOME", &config.isMetronomePlaying); sy += 45;
                if (GuiSliderBar({ 25, (float)sy, 250, 20 }, NULL, TextFormat("%i BPM", (int)tempBpm), &tempBpm, 40, 200)) { config.bpm = (int)tempBpm; } 
                sy += 40;
                DrawSharpText(mainFont, "CATEGORIES", { 25, (float)sy }, 18, 1, t.text); sy += 25;
                if (GuiTextBox({ 25, (float)sy, 250, 35 }, newCatName, 128, currentFocus == FOCUS_NEW_CAT)) currentFocus = FOCUS_NEW_CAT;
                if (GuiButton({ 285, (float)sy, 140, 35 }, "ADD CATEGORY")) { if(strlen(newCatName) > 0) { config.categories.push_back(newCatName); strcpy(newCatName, ""); Storage::saveConfig(config); currentFocus = FOCUS_NONE; } }
                sy += 45;

                int maxVis = 4; int catCount = (int)config.categories.size();
                float maxScroll = (catCount > maxVis) ? (float)(catCount - maxVis) : 0.0f;
                if (catScrollOffset > maxScroll) catScrollOffset = maxScroll;
                
                if (catCount > maxVis) {
                    GuiSliderBar({ 385, (float)sy, 15, (float)(maxVis * 35 - 5) }, NULL, NULL, &catScrollOffset, 0, maxScroll);
                }

                int startIdx = (int)catScrollOffset;
                for (int i = startIdx; i < startIdx + maxVis && i < catCount; i++) {
                    DrawRectangleRounded({ 25, (float)sy, 350, 30 }, 0.2, 8, t.surface); 
                    DrawSharpText(mainFont, config.categories[i].c_str(), { 40, (float)sy+8 }, 14, 1, t.text);
                    if (GuiButton({ 385, (float)sy, 40, 30 }, "#143#")) { 
                        config.categories.erase(config.categories.begin() + i); 
                        Storage::saveConfig(config); i--; // Slider clamping handled next frame
                    }
                    sy += 35;
                }
                sy += 15;
                DrawSharpText(mainFont, "SHORTCUTS", { 25, (float)sy }, 18, 1, t.text); sy += 25;
                DrawSharpText(mainFont, "- ALT+P: Start/Pause", { 40, (float)sy }, 14, 1, t.subtext); sy += 20;
                DrawSharpText(mainFont, "- ALT+R: Reset Timer", { 40, (float)sy }, 14, 1, t.subtext); sy += 20;
            } else if (showStats) {
                DrawSharpText(mainFont, "ANALYTICS", { 25, 110 }, 22, 1, t.text);
                DrawPieChart({ 300, 210 }, 80, stats[GetDateOffset(0)], t);
                DrawSharpText(mainFont, "TODAY'S SPLIT", { 25, 160 }, 16, 1, t.accent);
                DrawSharpText(mainFont, TextFormat("TOTAL: %im", (int)config.totalFocusMinutes), { 25, 185 }, 12, 1, t.subtext);
                int cX = 50, cY = 550, cW = 350, cH = 100; DrawLineEx({(float)cX, (float)cY}, {(float)cX+cW, (float)cY}, 2, t.surface);
                float maxM = 1.0f; std::vector<float> dTs; for(int i=-6; i<=0; i++) { float tot = 0; for(auto const& [cat, m] : stats[GetDateOffset(i)]) tot += m; dTs.push_back(tot); if (tot > maxM) maxM = tot; }
                for(size_t i=0; i<dTs.size(); i++) { float h = (dTs[i]/maxM)*cH; DrawRectangle(cX+i*(cW/7)+5, cY-h, (cW/7)-10, h, t.accent); DrawSharpText(mainFont, GetDateOffset(i-6).substr(8,2).c_str(), { (float)(cX+i*(cW/7)+10), (float)(cY+5) }, 12, 1, t.subtext); }
            } else if (showQueue) {
                DrawSharpText(mainFont, "QUEUE STATUS", { 25, 110 }, 22, 1, t.text); int qy = 145;
                if (currentActivity) {
                    DrawRectangleRounded({ 25, (float)qy, 400, 50 }, 0.2, 8, t.surface); DrawRectangleRoundedLines({ 25, (float)qy, 400, 50 }, 0.2, 8, t.accent);
                    DrawSharpText(mainFont, currentActivity->title.c_str(), { 40, (float)qy+15 }, 18, 1, t.text); qy += 70;
                }
                for (int i=0; i < (int)config.queue.size(); i++) {
                    DrawRectangleRounded({ 25, (float)qy, 400, 50 }, 0.2, 8, t.surface); DrawSharpText(mainFont, config.queue[i].title.c_str(), { 40, (float)qy+15 }, 18, 1, t.text);
                    if (i > 0 && GuiButton({ 275, (float)qy+10, 30, 30 }, "#114#")) { std::swap(config.queue[i], config.queue[i-1]); Storage::saveConfig(config); }
                    if (i < (int)config.queue.size()-1 && GuiButton({ 310, (float)qy+10, 30, 30 }, "#115#")) { std::swap(config.queue[i], config.queue[i+1]); Storage::saveConfig(config); }
                    if (GuiButton({ 355, (float)qy+10, 35, 30 }, "#143#")) { config.queue.erase(config.queue.begin()+i); Storage::saveConfig(config); i--; }
                    qy += 60; if (qy > SCREEN_HEIGHT-65) break;
                }
            } else if (showRoutines) {
                if (editingRoutineIdx == -2) {
                    DrawSharpText(mainFont, "ROUTINES", { 25, 110 }, 22, 1, t.text);
                    if (GuiButton({ 285, 105, 140, 35 }, "#08# NEW ROUTINE")) { editingRoutineIdx = -1; tempRoutine = Routine(); tempRoutine.name = "My New Block"; strcpy(tempRoutineName, tempRoutine.name.c_str()); }
                    if (GuiButton({ 140, 105, 140, 35 }, "#121# POMODORO")) {
                        Activity work = { 0, "Work (Pomo)", "Work", 25*60 }; 
                        Activity sBreak = { 0, "Short Break", "Study", 5*60 };
                        Activity lBreak = { 0, "Long Break", "Study", 15*60 };
                        config.queue.clear();
                        for(int i=0; i<3; i++) { config.queue.push_back(work); config.queue.push_back(sBreak); }
                        config.queue.push_back(work); config.queue.push_back(lBreak);
                        Storage::saveConfig(config); 
                        showQueue = true; showRoutines = false; viewAlpha = 1.0f; isTransitioning = true;
                    }
                    int ry = 160; for (int i=0; i<(int)config.routines.size(); i++) {
                        DrawRectangleRounded({ 25, (float)ry, 400, 50 }, 0.2, 8, t.surface); DrawSharpText(mainFont, config.routines[i].name.c_str(), { 40, (float)ry+15 }, 18, 1, t.text);
                        if (GuiButton({ 195, (float)ry+10, 70, 30 }, "LOAD")) { config.queue.insert(config.queue.end(), config.routines[i].items.begin(), config.routines[i].items.end()); Storage::saveConfig(config); showQueue=true; showRoutines=false; }
                        if (GuiButton({ 275, (float)ry+10, 70, 30 }, "EDIT")) { editingRoutineIdx = i; tempRoutine = config.routines[i]; strcpy(tempRoutineName, tempRoutine.name.c_str()); }
                        if (GuiButton({ 355, (float)ry+10, 35, 30 }, "#143#")) { config.routines.erase(config.routines.begin()+i); Storage::saveConfig(config); i--; }
                        ry += 60; if (ry > SCREEN_HEIGHT-50) break;
                    }
                } else {
                    DrawSharpText(mainFont, "ROUTINE EDITOR", { 25, 110 }, 22, 1, t.accent);
                    if (GuiTextBox({ 25, 150, 240, 35 }, tempRoutineName, 128, currentFocus == FOCUS_EDITOR_NAME)) currentFocus = FOCUS_EDITOR_NAME;
                    if (GuiButton({ 275, 150, 70, 35 }, "SAVE")) { tempRoutine.name = tempRoutineName; if (editingRoutineIdx == -1) config.routines.push_back(tempRoutine); else config.routines[editingRoutineIdx] = tempRoutine; Storage::saveConfig(config); editingRoutineIdx = -2; }
                    if (GuiButton({ 355, 150, 70, 35 }, "EXIT")) { editingRoutineIdx = -2; }
                    int by = 220; for (int i=0; i < (int)tempRoutine.items.size(); i++) {
                        DrawRectangleRounded({ 25, (float)by, 400, 45 }, 0.2, 8, t.surface); DrawSharpText(mainFont, tempRoutine.items[i].title.c_str(), { 40, (float)by+12 }, 16, 1, t.text);
                        if (i > 0 && GuiButton({ 275, (float)by+7, 30, 30 }, "#114#")) std::swap(tempRoutine.items[i], tempRoutine.items[i-1]);
                        if (i < (int)tempRoutine.items.size()-1 && GuiButton({ 310, (float)by+7, 30, 30 }, "#115#")) std::swap(tempRoutine.items[i], tempRoutine.items[i+1]);
                        if (GuiButton({ 355, (float)by+7, 35, 30 }, "#143#")) { tempRoutine.items.erase(tempRoutine.items.begin()+i); i--; }
                        by += 50; if (by > 400) break;
                    }
                    int ly = 450; for (int i=0; i < (int)config.activities.size(); i++) {
                        DrawRectangleRounded({ 25, (float)ly, 400, 40 }, 0.2, 8, t.surface); DrawSharpText(mainFont, config.activities[i].title.c_str(), { 40, (float)ly+10 }, 14, 1, t.text);
                        if (GuiButton({ 355, (float)ly+5, 35, 30 }, "#08#")) tempRoutine.items.push_back(config.activities[i]);
                        ly += 45; if (ly > SCREEN_HEIGHT-50) break;
                    }
                }
            } else if (showActivities) {
                DrawSharpText(mainFont, "ACTIVITIES", { 25, 110 }, 22, 1, t.text);
                
                GuiSetState(editingActIdx != -1 ? STATE_DISABLED : STATE_NORMAL);
                if (GuiTextBox({ 25, 145, 150, 35 }, newActTitle, 128, currentFocus == FOCUS_NEW_ACT)) currentFocus = FOCUS_NEW_ACT;
                if (GuiTextBox({ 180, 145, 40, 35 }, newActMinsStr, 16, currentFocus == FOCUS_NEW_MINS)) currentFocus = FOCUS_NEW_MINS;
                if (GuiTextBox({ 225, 145, 40, 35 }, newActSecsStr, 16, currentFocus == FOCUS_NEW_SECS)) currentFocus = FOCUS_NEW_SECS;
                if (!config.categories.empty() && GuiButton({ 270, 145, 80, 35 }, config.categories[newActCatIdx % config.categories.size()].c_str())) newActCatIdx = (newActCatIdx + 1) % (int)config.categories.size();
                if (GuiButton({ 355, 145, 75, 35 }, "ADD")) { if(strlen(newActTitle) > 0) { Activity a; a.id = time(0); a.title = newActTitle; a.category = config.categories[newActCatIdx % config.categories.size()]; int m = atoi(newActMinsStr), s = atoi(newActSecsStr); a.initialSeconds = m*60 + s; config.activities.push_back(a); Storage::saveConfig(config); strcpy(newActTitle, ""); } }
                GuiSetState(STATE_NORMAL);

                int listY = 195;
                for (int i=0; i < (int)config.activities.size(); i++) {
                    DrawRectangleRounded({ 25, (float)listY, 400, editingActIdx==i?140.0f:50.0f }, 0.2, 8, t.surface);
                    if (editingActIdx == i) {
                        if (GuiTextBox({ 40, (float)listY+10, 120, 30 }, editTitle, 128, currentFocus == FOCUS_EDIT_ACT)) currentFocus = FOCUS_EDIT_ACT;
                        if (GuiTextBox({ 165, (float)listY+10, 35, 30 }, editMinsStr, 16, currentFocus == FOCUS_EDIT_MINS)) currentFocus = FOCUS_EDIT_MINS;
                        if (GuiTextBox({ 205, (float)listY+10, 35, 30 }, editSecsStr, 16, currentFocus == FOCUS_EDIT_SECS)) currentFocus = FOCUS_EDIT_SECS;
                        if (!config.categories.empty() && GuiButton({ 245, (float)listY+10, 70, 30 }, config.categories[editingActCatIdx % config.categories.size()].c_str())) editingActCatIdx = (editingActCatIdx + 1) % (int)config.categories.size();
                        if (CheckCollisionPointRec(GetMousePosition(), { 245, (float)listY+10, 70, 30 })) currentTooltip = "Switch Category";
                        
                        if (GuiTextBox({ 40, (float)listY+50, 280, 30 }, editSoundPath, 256, currentFocus == FOCUS_EDIT_SOUND)) { currentFocus = FOCUS_EDIT_SOUND; }
                        DrawSharpText(mainFont, "WAV/MP3 PATH", { 330, (float)listY+58 }, 10, 1, t.subtext);
                        if (GuiTextBox({ 40, (float)listY+90, 280, 30 }, editLaunchPath, 256, currentFocus == FOCUS_EDIT_LAUNCH)) { currentFocus = FOCUS_EDIT_LAUNCH; }
                        DrawSharpText(mainFont, "LAUNCH COMMAND", { 330, (float)listY+98 }, 10, 1, t.subtext);
                        if (GuiButton({ 350, (float)listY+10, 65, 30 }, "SAVE")) { config.activities[i].title = editTitle; config.activities[i].category = config.categories[editingActCatIdx % config.categories.size()]; int m = atoi(editMinsStr), s = atoi(editSecsStr); config.activities[i].initialSeconds = m*60 + s; config.activities[i].customSoundPath = editSoundPath; config.activities[i].autoLaunchPath = editLaunchPath; Storage::saveConfig(config); editingActIdx = -1; }
                        if (GuiButton({ 350, (float)listY+50, 65, 30 }, "EXIT")) { editingActIdx = -1; } 
                        listY += 150;
                    } else {
                        DrawSharpText(mainFont, config.activities[i].title.c_str(), { 40, (float)listY+15 }, 18, 1, t.text);
                        DrawSharpText(mainFont, TextFormat("| %s", config.activities[i].category.c_str()), { 160, (float)listY+18 }, 12, 1, t.accent);
                        
                        // Action Buttons with Tooltips (Shifted left to fit container)
                        GuiSetState(editingActIdx != -1 ? STATE_DISABLED : STATE_NORMAL);
                        if (GuiButton({ 255, (float)listY+10, 35, 30 }, "#131#")) { currentActivity = &config.activities[i]; timer.overrideDuration(currentActivity->initialSeconds); timer.start(); OpenExternal(currentActivity->autoLaunchPath.c_str()); showActivities=false; }
                        if (CheckCollisionPointRec(GetMousePosition(), { 255, (float)listY+10, 35, 30 })) currentTooltip = "Quick Start";
                        
                        if (GuiButton({ 295, (float)listY+10, 35, 30 }, "#14#")) { editingActIdx = i; strcpy(editTitle, config.activities[i].title.c_str()); sprintf(editMinsStr, "%i", config.activities[i].initialSeconds/60); sprintf(editSecsStr, "%i", config.activities[i].initialSeconds%60); strcpy(editSoundPath, config.activities[i].customSoundPath.c_str()); strcpy(editLaunchPath, config.activities[i].autoLaunchPath.c_str()); editingActCatIdx=0; for(int k=0; k<(int)config.categories.size(); k++) if(config.categories[k]==config.activities[i].category) editingActCatIdx=k; }
                        if (CheckCollisionPointRec(GetMousePosition(), { 295, (float)listY+10, 35, 30 })) currentTooltip = "Edit Activity";
                        
                        if (GuiButton({ 335, (float)listY+10, 35, 30 }, "#174#")) { config.queue.push_back(config.activities[i]); Storage::saveConfig(config); }
                        if (CheckCollisionPointRec(GetMousePosition(), { 335, (float)listY+10, 35, 30 })) currentTooltip = "Add to Queue";
                        
                        if (GuiButton({ 375, (float)listY+10, 35, 30 }, "#143#")) { config.activities.erase(config.activities.begin()+i); i--; }
                        if (CheckCollisionPointRec(GetMousePosition(), { 375, (float)listY+10, 35, 30 })) currentTooltip = "Delete Activity";
                        
                        GuiSetState(STATE_NORMAL);
                        listY += 60;
                    }
                }
            } else {
                int rem = timer.getRemainingSeconds(); char timerStr[10]; sprintf(timerStr, "%02d:%01d%d", rem / 60, (rem % 60) / 10, rem % 10);
                Vector2 ctr = { SCREEN_WIDTH / 2.0f, 260 }; 
                float breathe = (!timer.isRunning()) ? sinf(GetTime() * 2.0f) * 2.0f : 0;
                float r = 140.0f + ((pulseTime > 0) ? sinf(pulseTime * 2.0f) * 4.0f : breathe);
                DrawRing(ctr, r - 12, r, 0, 360, 64, t.surface); DrawRing(ctr, r - 12, r, -90.0f, -90.0f + (360.0f * (float)rem / (float)timer.getTotalSeconds()), 64, t.accent);
                float fs = config.isMinimalist ? 100.0f : 80.0f; DrawSharpText(mainFont, timerStr, { ctr.x - MeasureTextEx(mainFont, timerStr, fs, 2).x/2, ctr.y - MeasureTextEx(mainFont, timerStr, fs, 2).y/2 }, fs, 2, t.text);
                if (currentActivity) DrawSharpText(mainFont, currentActivity->title.c_str(), { ctr.x - MeasureTextEx(mainFont, currentActivity->title.c_str(), 18, 1).x/2, ctr.y + 160 }, 18, 1, t.subtext);
                if (GuiButton({ 125, 450, 200, 50 }, timer.isRunning() ? "PAUSE" : "START")) { if (timer.isRunning()) timer.pause(); else { timer.start(); if(currentActivity) OpenExternal(currentActivity->autoLaunchPath.c_str()); } }
                if (GuiButton({ 175, 510, 100, 30 }, "RESET")) timer.reset();
                if (GuiButton({ 25, 570, SCREEN_WIDTH - 50, 40 }, "SELECT ACTIVITY")) { showActivities = true; }
            }
        }
        if (viewAlpha > 0) DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(t.base, viewAlpha));
        EndBlendMode();
        DrawTooltip(mainFont, t); EndDrawing(); currentTooltip = "";
    }
    PlatformCleanup(); UnloadFont(mainFont); if (lastCustomSound.frameCount > 0) UnloadSound(lastCustomSound); CloseAudioDevice(); CloseWindow(); return 0;
}
