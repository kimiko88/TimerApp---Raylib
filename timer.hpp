#pragma once
#include <chrono>
#include <string>

enum class TimerMode {
    FOCUS,
    SHORT_BREAK,
    LONG_BREAK,
    CUSTOM
};

struct ModeInfo {
    TimerMode mode;
    std::string label;
    int minutes;
};

class Timer {
public:
    Timer() : remainingSeconds(25 * 60), totalSeconds(25 * 60), mode(TimerMode::FOCUS), running(false) {}

    void start() {
        if (!running) {
            lastTick = std::chrono::steady_clock::now();
            running = true;
        }
    }

    void pause() {
        running = false;
    }

    void reset() {
        running = false;
        remainingSeconds = totalSeconds;
    }

    void setMode(TimerMode newMode, int mins) {
        running = false;
        mode = newMode;
        totalSeconds = mins * 60;
        remainingSeconds = totalSeconds;
    }

    void overrideDuration(int secs) {
        running = false;
        totalSeconds = secs;
        remainingSeconds = totalSeconds;
    }

    void update() {
        if (!running) return;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastTick).count();

        if (elapsed >= 1) {
            remainingSeconds -= (int)elapsed;
            lastTick = now;
            if (remainingSeconds <= 0) {
                remainingSeconds = 0;
                running = false;
                completed = true;
            }
        }
    }

    bool isRunning() const { return running; }
    int getRemainingSeconds() const { return remainingSeconds; }
    int getTotalSeconds() const { return totalSeconds; }
    TimerMode getMode() const { return mode; }
    
    bool hasCompleted() {
        if (completed) {
            completed = false;
            return true;
        }
        return false;
    }

private:
    int remainingSeconds;
    int totalSeconds;
    TimerMode mode;
    bool running;
    bool completed = false;
    std::chrono::steady_clock::time_point lastTick;
};
