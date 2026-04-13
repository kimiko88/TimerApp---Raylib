#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include "include/json.hpp"

using json = nlohmann::json;

struct Activity {
    long id;
    std::string title;
    std::string category;
    int initialSeconds;
    int totalTime;
    std::string sound;
    std::string customSoundPath = "";
    std::string autoLaunchPath = "";

    // JSON serialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Activity, id, title, category, initialSeconds, totalTime, sound, customSoundPath, autoLaunchPath)
};

struct Routine {
    std::string name;
    std::vector<Activity> items;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Routine, name, items)
};

struct Config {
    bool isDarkMode = false;
    std::string theme = "";
    std::string selectedSound = "ding";
    bool notificationsEnabled = false;
    std::vector<Activity> activities;
    std::vector<Activity> queue;
    std::vector<Routine> routines;
    int bpm = 60;
    bool autoPlay = true;
    bool isMinimalist = false;
    bool hideCategoryInMinimalist = false;
    bool usePulseAnimation = true;
    bool useTransparency = false;
    bool showMetronome = false;
    bool isMetronomePlaying = false;
    std::string selectedTheme = "Dracula";
    int totalFocusMinutes = 0;
    bool eyeSafeEnabled = false;
    float eyeSafeRemainingSeconds = 7200.0f;
    bool isInEyeSafeBreak = false;
    std::vector<std::string> categories;
    int currentFocusRemainingSeconds = 0;
    long currentFocusActivityID = -1;
    bool useTransitions = true;

    // JSON serialization
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config, isDarkMode, theme, selectedSound, notificationsEnabled, activities, queue, routines, bpm, autoPlay, isMinimalist, hideCategoryInMinimalist, usePulseAnimation, useTransparency, showMetronome, isMetronomePlaying, selectedTheme, totalFocusMinutes, eyeSafeEnabled, eyeSafeRemainingSeconds, isInEyeSafeBreak, categories, currentFocusRemainingSeconds, currentFocusActivityID, useTransitions)
};

class Storage {
public:
    typedef std::map<std::string, std::map<std::string, float>> DailyStats;

    static Config loadConfig() {
        Config config;
        std::ifstream f("focus_config.json");
        if (f.is_open()) {
            try {
                json data;
                f >> data;
                config = data.get<Config>();
            } catch (...) {
                std::cerr << "Error loading config, using defaults.\n";
            }
        }
        return config;
    }

    static void saveConfig(const Config& config) {
        std::ofstream f("focus_config.json");
        if (f.is_open()) {
            json data = config;
            f << data.dump(4);
        }
    }

    static DailyStats loadStats() {
        DailyStats stats;
        std::ifstream f("focus_stats.json");
        if (f.is_open()) {
            try {
                json data;
                f >> data;
                stats = data.get<DailyStats>();
            } catch (...) {}
        }
        return stats;
    }

    static void saveStats(const DailyStats& stats) {
        std::ofstream f("focus_stats.json");
        if (f.is_open()) {
            json data = stats;
            f << data.dump(4);
        }
    }
};
