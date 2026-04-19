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
bool sendPosition = false;
uint64_t mouseFps = 50;
std::atomic<uint64_t> moveThrottleMs{20};

std::string clickTarget = "";
std::string scrollTarget = "";
std::string positionTarget = "";

bool enableLogging = false;
bool startWithObs = true;

// UI
static QPointer<MouseSettingsDialog> settingsDialog;

void emitBrowserEvent(const std::string &target, const std::string &action, bool includePosition, bool includeScroll)
{
	if (target.empty() || target == "None" || target == "No Browser Sources Found") {
		return;
	}

	obs_source_t *source = obs_get_source_by_name(target.c_str());
	if (!source) return;

	proc_handler_t *ph = obs_source_get_proc_handler(source);
	if (!ph) {
		obs_source_release(source);
		return;
	}

	obs_data_t *event_data = obs_data_create();
	obs_data_t *mouse_data = obs_data_create();

	if (!action.empty()) {
		obs_data_set_string(mouse_data, "action", action.c_str());
	}

	if (includePosition) {
		obs_data_set_int(mouse_data, "x", currentMouseX.load());
		obs_data_set_int(mouse_data, "y", currentMouseY.load());
	}

	if (includeScroll) {
		obs_data_set_double(mouse_data, "scroll_speed", currentScrollSpeed.load());
	}

	obs_data_set_obj(event_data, "mouse", mouse_data);
	obs_data_release(mouse_data);

	const char *json_data = obs_data_get_json(event_data);
	
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "eventName", "mouse_monitor_event");
	calldata_set_string(&cd, "jsonString", json_data);
	proc_handler_call(ph, "javascript_event", &cd);
	calldata_free(&cd);

	obs_data_release(event_data);
	obs_source_release(source);
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
				emitBrowserEvent(positionTarget, "Move", true, false);
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
			if (enableLogging) blog(LOG_INFO, "[Mouse Monitor] Click: %s", clickAction.c_str());
			emitBrowserEvent(clickTarget, clickAction, true, false);
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

			if (enableLogging) blog(LOG_INFO, "[Mouse Monitor] Scroll: %s", scrollAction.c_str());
			emitBrowserEvent(scrollTarget, scrollAction, true, true);
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
				emitBrowserEvent(positionTarget, "Move", true, false);
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
		if (enableLogging) blog(LOG_INFO, "[Mouse Monitor] Click: %s", action.c_str());
		emitBrowserEvent(clickTarget, action, true, false);
	} else if (isScroll && sendScroll) {
		if (enableLogging) blog(LOG_INFO, "[Mouse Monitor] Scroll: %s", action.c_str());
		emitBrowserEvent(scrollTarget, action, true, true);
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
				if (enableLogging) blog(LOG_INFO, "[Mouse Monitor] Settings saved to: %s", path);
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

		clickTarget = obs_data_get_string(settings, "clickTarget");
		scrollTarget = obs_data_get_string(settings, "scrollTarget");
		positionTarget = obs_data_get_string(settings, "positionTarget");
		enableLogging = obs_data_get_bool(settings, "enableLogging");
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

		clickTarget = obs_data_get_string(settings, "clickTarget");
		scrollTarget = obs_data_get_string(settings, "scrollTarget");
		positionTarget = obs_data_get_string(settings, "positionTarget");
		enableLogging = obs_data_get_bool(settings, "enableLogging");
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
	
	if (settingsDialog) {
		delete settingsDialog;
	}

	// Final save on exit
	obs_data_t *currentSettings = obs_data_create();
	obs_data_set_bool(currentSettings, "sendClicks", sendClicks);
	obs_data_set_bool(currentSettings, "sendScroll", sendScroll);
	obs_data_set_bool(currentSettings, "sendPosition", sendPosition);
	obs_data_set_int(currentSettings, "mouseFps", mouseFps);
	obs_data_set_string(currentSettings, "clickTarget", clickTarget.c_str());
	obs_data_set_string(currentSettings, "scrollTarget", scrollTarget.c_str());
	obs_data_set_string(currentSettings, "positionTarget", positionTarget.c_str());
	obs_data_set_bool(currentSettings, "enableLogging", enableLogging);
	obs_data_set_bool(currentSettings, "startWithObs", startWithObs);
	
	SaveLoadSettingsCallback(currentSettings, true);
	obs_data_release(currentSettings);

	SaveLoadSettingsCallback(nullptr, true); // Cleanup savedSettings
}
