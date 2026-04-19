#pragma once

#include <obs-module.h>
#include <string>
#include <atomic>
#include <mutex>
#include <unordered_set>

// Forward declarations
class MouseSettingsDialog;

// Global settings/state
extern std::atomic<int> currentMouseX;
extern std::atomic<int> currentMouseY;
extern std::atomic<double> currentScrollSpeed;
extern std::atomic<uint64_t> lastScrollTimestamp;
extern std::atomic<uint64_t> lastMoveTimestamp;

extern bool sendClicks;
extern bool sendScroll;
extern bool sendPosition;
extern uint64_t mouseFps;
extern std::atomic<uint64_t> moveThrottleMs;

extern std::string clickTarget;
extern std::string scrollTarget;
extern std::string positionTarget;

extern bool enableLogging;

// Hook functions
#ifdef _WIN32
void startWindowsHooks();
void stopWindowsHooks();
#endif

#ifdef __APPLE__
void startMacOSMouseHook();
void stopMacOSMouseHook();
bool checkMacOSAccessibilityPermissions();
#endif

#ifdef __linux__
void startLinuxMouseHook();
void stopLinuxMouseHook();
#endif

// Shared functions
void emitBrowserEvent(const std::string &target, const std::string &action, bool includePosition = false, bool includeScroll = false);
obs_data_t *SaveLoadSettingsCallback(obs_data_t *settings, bool saving);
void openSettingsDialog();
