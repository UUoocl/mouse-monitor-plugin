#include "mouse-monitor-plugin.hpp"
#include "mouse-settings-dialog.hpp"
#include "version.h"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.h>
#include <string>
#include <mutex>
#include <atomic>
#include <cmath>
#include <QMainWindow>
#include <util/platform.h>
#include <QPointer>
#include <QAction>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <thread>
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Antigravity")
OBS_MODULE_USE_DEFAULT_LOCALE("mouse-monitor-plugin", "en-US")

#ifdef _WIN32
HHOOK mouseHook = NULL;
#endif

#ifdef __APPLE__
CFMachPortRef eventTap = nullptr;
CFRunLoopSourceRef eventSource = nullptr;
#endif

#ifdef __linux__
Display *display = nullptr;
std::thread linuxHookThread;
std::atomic<bool> linuxHookRunning{false};
#endif

// Mouse tracking state
std::atomic<int> currentMouseX{0};
std::atomic<int> currentMouseY{0};
std::atomic<double> currentScrollSpeed{0.0};
std::atomic<uint64_t> lastScrollTimestamp{0};
std::atomic<uint64_t> lastMoveTimestamp{0};

// Settings
bool sendClicks = true;
bool sendScroll = true;
bool sendPosition = true;
uint64_t mouseFps = 50;
std::atomic<uint64_t> moveThrottleMs{20};

bool startWithObs = true;

// UI
static QPointer<MouseSettingsDialog> settingsDialog;

void broadcastMouseSignal(const char *addr, double val)
{
	obs_data_t *packet = obs_data_create();
	obs_data_set_string(packet, "t", "mouse");
	obs_data_set_string(packet, "a", addr);
	obs_data_set_double(packet, "v", val);

	signal_handler_t *sh = obs_get_signal_handler();
	if (sh) {
		calldata_t cd = {0};
		calldata_set_ptr(&cd, "packet", packet);
		signal_handler_signal(sh, "media_warp_transmit", &cd);
	}

	obs_data_release(packet);
}

static void on_media_warp_receive(void *data, calldata_t *cd)
{
	(void)data;
	const char *json_str = calldata_string(cd, "json_str");
	if (!json_str) return;

	obs_data_t *msg = obs_data_create_from_json(json_str);
	if (!msg) return;

	const char *type = obs_data_get_string(msg, "t");
	const char *addr = obs_data_get_string(msg, "a");

	// No control messages currently handled
	(void)type;
	(void)addr;

	obs_data_release(msg);
}

#ifdef _WIN32
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION) {
		MSLLHOOKSTRUCT *p = (MSLLHOOKSTRUCT *)lParam;
		currentMouseX.store(p->pt.x);
		currentMouseY.store(p->pt.y);

		// Position updates
		if (wParam == WM_MOUSEMOVE && sendPosition) {
			uint64_t now = os_gettime_ns() / 1000000;
			if (now - lastMoveTimestamp.load() > moveThrottleMs.load()) {
				lastMoveTimestamp.store(now);
				broadcastMouseSignal("move/x", (double)p->pt.x);
				broadcastMouseSignal("move/y", (double)p->pt.y);
			}
		}

		// Click updates
		std::string clickAction = "";
		switch (wParam) {
			case WM_LBUTTONDOWN: clickAction = "Left Click"; break;
			case WM_RBUTTONDOWN: clickAction = "Right Click"; break;
			case WM_MBUTTONDOWN: clickAction = "Middle Click"; break;
			case WM_XBUTTONDOWN:
				if (HIWORD(p->mouseData) == XBUTTON1) clickAction = "X Button 1";
				else if (HIWORD(p->mouseData) == XBUTTON2) clickAction = "X Button 2";
				break;
		}

		if (!clickAction.empty() && sendClicks) {
			
			std::string addr = "click/";
			if (wParam == WM_LBUTTONDOWN) addr += "left";
			else if (wParam == WM_RBUTTONDOWN) addr += "right";
			else if (wParam == WM_MBUTTONDOWN) addr += "middle";
			else if (wParam == WM_XBUTTONDOWN) {
				if (HIWORD(p->mouseData) == XBUTTON1) addr += "x1";
				else if (HIWORD(p->mouseData) == XBUTTON2) addr += "x2";
			}
			broadcastMouseSignal(addr.c_str(), 1.0);
		}

		// Scroll updates
		if ((wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL) && sendScroll) {
			uint64_t now = os_gettime_ns() / 1000000;
			uint64_t diff = now - lastScrollTimestamp.load();
			lastScrollTimestamp.store(now);

			double delta = (double)GET_WHEEL_DELTA_WPARAM(p->mouseData);
			if (diff > 0 && diff < 1000) {
				currentScrollSpeed.store(std::abs(delta) / (double)diff);
			} else {
				currentScrollSpeed.store(0.0);
			}

			std::string scrollAction = "";
			if (wParam == WM_MOUSEWHEEL) {
				scrollAction = (delta > 0) ? "Scroll Up" : "Scroll Down";
			} else {
				scrollAction = (delta > 0) ? "Scroll Right" : "Scroll Left";
			}

			broadcastMouseSignal("scroll/delta", delta);
			broadcastMouseSignal("scroll/speed", currentScrollSpeed.load());
			broadcastMouseSignal((wParam == WM_MOUSEWHEEL) ? "scroll/v" : "scroll/h", delta);
		}
	}
	return CallNextHookEx(mouseHook, nCode, wParam, lParam);
}

void startWindowsHooks() {
	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);
}

void stopWindowsHooks() {
	if (mouseHook) UnhookWindowsHookEx(mouseHook);
}
#endif

#ifdef __APPLE__
CGEventRef CGEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
	(void)proxy;
	(void)refcon;
	if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
		blog(LOG_INFO, "[Mouse Monitor] Hook disabled by OS, re-enabling...");
		if (eventTap) CGEventTapEnable(eventTap, true);
		return event;
	}

	CGPoint location = CGEventGetLocation(event);
	currentMouseX.store((int)location.x);
	currentMouseY.store((int)location.y);

	if (type == kCGEventMouseMoved || type == kCGEventLeftMouseDragged || type == kCGEventRightMouseDragged || type == kCGEventOtherMouseDragged) {
		if (sendPosition) {
			uint64_t now = os_gettime_ns() / 1000000;
			if (now - lastMoveTimestamp.load() > moveThrottleMs.load()) {
				lastMoveTimestamp.store(now);
				broadcastMouseSignal("move/x", (double)location.x);
				broadcastMouseSignal("move/y", (double)location.y);
			}
		}
	}

	std::string action = "";
	bool isClick = false;
	bool isScroll = false;

	if (type == kCGEventLeftMouseDown) { action = "Left Click"; isClick = true; }
	else if (type == kCGEventRightMouseDown) { action = "Right Click"; isClick = true; }
	else if (type == kCGEventOtherMouseDown) {
		int64_t buttonNumber = CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
		if (buttonNumber == 2) action = "Middle Click";
		else action = "Button " + std::to_string(buttonNumber);
		isClick = true;
	}
	else if (type == kCGEventScrollWheel) {
		isScroll = true;
		int64_t deltaY = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1);
		int64_t deltaX = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis2);
		
		uint64_t now = os_gettime_ns() / 1000000;
		uint64_t diff = now - lastScrollTimestamp.load();
		lastScrollTimestamp.store(now);
		
		if (diff > 0 && diff < 1000) {
			currentScrollSpeed.store((std::abs(deltaY) + std::abs(deltaX)) / (double)diff);
		} else {
			currentScrollSpeed.store(0.0);
		}

		if (std::abs(deltaY) > std::abs(deltaX)) {
			action = (deltaY > 0) ? "Scroll Up" : "Scroll Down";
		} else {
			action = (deltaX > 0) ? "Scroll Left" : "Scroll Right";
		}
	}

	if (isClick && sendClicks) {
		
		std::string addr = "click/";
		if (type == kCGEventLeftMouseDown) addr += "left";
		else if (type == kCGEventRightMouseDown) addr += "right";
		else if (type == kCGEventOtherMouseDown) {
			int64_t buttonNumber = CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
			if (buttonNumber == 2) addr += "middle";
			else addr += std::to_string(buttonNumber);
		}
		broadcastMouseSignal(addr.c_str(), 1.0);
	} else if (isScroll && sendScroll) {
		
		int64_t deltaY = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1);
		int64_t deltaX = CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis2);
		
		if (std::abs(deltaY) > 0) broadcastMouseSignal("scroll/v", (double)deltaY);
		if (std::abs(deltaX) > 0) broadcastMouseSignal("scroll/h", (double)deltaX);
		broadcastMouseSignal("scroll/speed", currentScrollSpeed.load());
	}

	return event;
}

bool checkMacOSAccessibilityPermissions() {
	return AXIsProcessTrusted();
}

void startMacOSMouseHook() {
	if (!checkMacOSAccessibilityPermissions()) return;

	CGEventMask eventMask = CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventRightMouseDown) |
				CGEventMaskBit(kCGEventOtherMouseDown) | CGEventMaskBit(kCGEventScrollWheel) |
				CGEventMaskBit(kCGEventMouseMoved) | CGEventMaskBit(kCGEventLeftMouseDragged) |
				CGEventMaskBit(kCGEventRightMouseDragged) | CGEventMaskBit(kCGEventOtherMouseDragged);

	eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
				    eventMask, CGEventCallback, nullptr);

	if (eventTap) {
		eventSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
		CFRunLoopAddSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopCommonModes);
		CGEventTapEnable(eventTap, true);
		blog(LOG_INFO, "[Mouse Monitor] macOS mouse hook started successfully");
	} else {
		blog(LOG_ERROR, "[Mouse Monitor] Failed to create CGEventTap");
	}
}

void stopMacOSMouseHook() {
	if (eventTap) {
		CGEventTapEnable(eventTap, false);
		if (eventSource) {
			CFRunLoopRemoveSource(CFRunLoopGetCurrent(), eventSource, kCFRunLoopCommonModes);
			CFRelease(eventSource);
			eventSource = nullptr;
		}
		CFRelease(eventTap);
		eventTap = nullptr;
	}
}
#endif

// Linux implementation would go here (similar pattern with X11)

void openSettingsDialog() {
	if (!settingsDialog) {
		settingsDialog = new MouseSettingsDialog((QMainWindow *)obs_frontend_get_main_window());
		settingsDialog->setAttribute(Qt::WA_DeleteOnClose);
	}
	settingsDialog->show();
	settingsDialog->raise();
	settingsDialog->activateWindow();
}

obs_data_t *SaveLoadSettingsCallback(obs_data_t *settings, bool saving)
{
	static obs_data_t *savedSettings = nullptr;

	if (saving) {
		if (settings == nullptr) { // Cleanup signal
			if (savedSettings) obs_data_release(savedSettings);
			savedSettings = nullptr;
			return NULL;
		}

		if (savedSettings) obs_data_release(savedSettings);
		savedSettings = obs_data_newref(settings);

		char *path = obs_module_config_path("mouse-monitor-settings.json");
		if (path) {
			// Ensure directory exists
			std::string sPath = path;
			size_t lastSlash = sPath.find_last_of("/\\");
			if (lastSlash != std::string::npos) {
				std::string dir = sPath.substr(0, lastSlash);
				os_mkdir(dir.c_str());
			}

			if (obs_data_save_json(settings, path)) {
				// Success
			} else {
				blog(LOG_ERROR, "[Mouse Monitor] Failed to save settings to: %s", path);
			}
			bfree(path);
		}

		// Update global variables
		sendClicks = obs_data_get_bool(settings, "sendClicks");
		sendScroll = obs_data_get_bool(settings, "sendScroll");
		sendPosition = obs_data_get_bool(settings, "sendPosition");
		mouseFps = obs_data_get_int(settings, "mouseFps");
		if (mouseFps == 0) mouseFps = 50;
		moveThrottleMs.store(1000 / mouseFps);

		startWithObs = obs_data_get_bool(settings, "startWithObs");


		return NULL;
	} else {
		if (!savedSettings) {
			char *path = obs_module_config_path("mouse-monitor-settings.json");
			if (path) {
				savedSettings = obs_data_create_from_json_file(path);
				bfree(path);
			}
		}
		return savedSettings ? obs_data_newref(savedSettings) : nullptr;
	}
}

bool obs_module_load(void)
{
	obs_data_t *settings = SaveLoadSettingsCallback(nullptr, false);
	if (settings) {
		sendClicks = obs_data_get_bool(settings, "sendClicks");
		sendScroll = obs_data_get_bool(settings, "sendScroll");
		sendPosition = obs_data_get_bool(settings, "sendPosition");
		mouseFps = obs_data_get_int(settings, "mouseFps");
		if (mouseFps == 0) mouseFps = 50;
		moveThrottleMs.store(1000 / mouseFps);

		startWithObs = obs_data_get_bool(settings, "startWithObs");

		
		obs_data_release(settings);
	}

	if (startWithObs) {
#ifdef _WIN32
		startWindowsHooks();
#endif
#ifdef __APPLE__
		startMacOSMouseHook();
#endif
	}

	signal_handler_t *sh = obs_get_signal_handler();
	if (sh) {
		signal_handler_connect(sh, "media_warp_receive", on_media_warp_receive, nullptr);
	}

	obs_frontend_add_event_callback([](enum obs_frontend_event event, void *) {
		if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
			QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(obs_module_text("Settings.Title"));
			QObject::connect(action, &QAction::triggered, []() { openSettingsDialog(); });
		}
	}, nullptr);

	blog(LOG_INFO, "[Mouse Monitor] Plugin loaded successfully");
	return true;
}

void obs_module_unload(void)
{
#ifdef _WIN32
	stopWindowsHooks();
#endif
#ifdef __APPLE__
	stopMacOSMouseHook();
#endif
	
	signal_handler_t *sh = obs_get_signal_handler();
	if (sh) {
		signal_handler_disconnect(sh, "media_warp_receive", on_media_warp_receive, nullptr);
	}

	if (settingsDialog) {
		delete settingsDialog;
	}

	// Final save on exit
	obs_data_t *currentSettings = obs_data_create();
	obs_data_set_bool(currentSettings, "sendClicks", sendClicks);
	obs_data_set_bool(currentSettings, "sendScroll", sendScroll);
	obs_data_set_bool(currentSettings, "sendPosition", sendPosition);
	obs_data_set_int(currentSettings, "mouseFps", mouseFps);
	obs_data_set_bool(currentSettings, "startWithObs", startWithObs);
	
	SaveLoadSettingsCallback(currentSettings, true);
	obs_data_release(currentSettings);

	SaveLoadSettingsCallback(nullptr, true); // Cleanup savedSettings
}
