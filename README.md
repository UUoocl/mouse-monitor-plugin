# Mouse Monitor Plugin for OBS Studio

A high-performance OBS Studio plugin designed for real-time monitoring of mouse input. It tracks position, clicks, and scroll wheel activity, and emits granular data directly to OBS browser sources for use in streaming overlays.

---

## 🚀 Features

- **Granular Event Tracking**:
  - **Position**: Real-time X/Y coordinate tracking with configurable update rates (FPS).
  - **Clicks**: Monitoring for Left, Right, Middle, and X-buttons (4, 5, etc.).
  - **Scroll**: Tracks vertical and horizontal scroll wheel movement with speed detection.
- **Per-Action Targeting**: 
  - Assign unique browser sources as targets for different event types (e.g., send clicks to one overlay and position to another).
  - Events are emitted via the standard OBS `javascript_event` protocol.
- **Modern UI**: Clean, premium Settings dialog accessible via `Tools > Mouse Monitor Settings`, featuring the **Catppuccin Mocha** color palette.
- **Optimized Performance**: Throttled event emission to ensure minimal CPU impact while maintaining smooth tracking.

## 🛠 Installation (macOS)

1. **Build** the plugin using the instructions below.
2. **Install**: Copy `mouse-monitor-plugin.plugin` to:
   `~/Library/Application Support/obs-studio/plugins`
3. **Restart** OBS Studio.

## 🏗 Building from Source

### macOS
```bash
# Configure the project
cmake --preset macos

# Build the release bundle
cmake --build build_macos --config Release

# Deploy and sign locally
./deploy_macos.sh
```

## 📜 Event Protocol

The plugin emits a `mouse_monitor_event` to targeted browser sources with a JSON payload:

```json
{
  "mouse": {
    "action": "Left Click",
    "x": 1920,
    "y": 1080,
    "scroll_speed": 0.0
  }
}
```

## 🤝 Credits

This project was rebranded and refactored from the **StreamUP Hotkey Display** plugin to provide a more focused and high-performance mouse monitoring experience.

---
*Developed for professional streamers by Antigravity.*
