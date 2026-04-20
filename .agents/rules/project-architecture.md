
## 📂 Project: Modular OBS Input & Visual Engine
**Objective:** A high-performance, modular system for live-stream orchestration using C++ OBS plugins, a centralized Mongoose WebSocket bridge, and a Svelte 5 management dashboard.

---

### 🏛️ System Architecture
The system follows a "Hub-and-Spoke" model to isolate high-frequency data from production logic.

1.  **Input Producers (C++ Plugins):** Specialized macOS plugins (`CoreMIDI`, `CoreGraphics`, `AVFoundation`) that capture hardware events and emit internal **OBS Signals**.
2.  **The Central Bridge (C++ Plugin):** The only plugin running a **Mongoose WebSocket Server** (`ws://localhost:8001`). It listens for internal OBS signals and broadcasts them as JSON.
3.  **The Orchestrator (Svelte 5):** A local web page that manages session state, filters input data, and communicates with the **OBS WebSocket (Port 4455)** to trigger scene changes.
4.  **Visual Engine (Cables.gl Desktop):** A transparent Electron window receiving high-frequency data from the Bridge for WebGL overlays.

---

### 🛠️ Technical Stack Constraints
* **Language:** C++20 for Plugins, JavaScript (Svelte 5) for Manager.
* **Frameworks:** `libobs` (OBS Studio), `Mongoose` (Networking), `oscpack` (OSC).
* **Platform:** macOS (M1 Pro / M4), strictly Apple Silicon native.
* **Networking:** Loopback only (`127.0.0.1`), `TCP_NODELAY` enabled for all sockets.

---

### 📋 Agent Instructions (Global Rules)
> **Note to Antigravity Agent:** Follow these rules for all code generation and refactoring tasks in this repository.

#### **1. C++ Plugin Patterns**
* **Threading:** Never perform network I/O or heavy computation on the OBS graphics thread. Use `std::thread` for Mongoose and input listeners.
* **Signals:** Use `signal_handler_signal` with `obs_data_t` objects to pass data to the Bridge.
* **Memory:** Strictly adhere to `libobs` reference counting (`obs_data_release`).

#### **2. WebSocket Messaging Protocol**
All messages must be flat JSON with short-key identifiers for fast parsing:
* `t`: Message Type (e.g., `midi`, `pose`, `key`)
* `a`: Address/Key (e.g., `/cc/1`, `button_a`)
* `v`: Value (normalized 0.0-1.0 or raw integers)

#### **3. Svelte 5 Manager Logic**
* **Reactivity:** Use Svelte 5 `$state` and `$derived` for UI updates.
* **Throttling:** High-frequency data (Mouse/MIDI) should bypass global state if intended strictly for the Visual Engine.

---

### 🔄 Workflows
**To add a new Input Plugin:**
1.  Generate a new `libobs` plugin template.
2.  Implement the hardware-specific capture (e.g., `GameController.framework`).
3.  Connect the output to the `bridge_transmit` signal.
4.  Update the Svelte Manager's message handler to recognize the new `t` (type).

---

### ⚠️ Critical Constraints for Agents
* **Do not** attempt to use `window.obsstudio.send()` in Browser Sources; it is restricted.
* **Do not** use `0.0.0.0` for bindings; strictly use `127.0.0.1` to avoid macOS firewall prompts.
* **Always** prioritize `ScreenCaptureKit` compatibility for transparent window overlays.

---

### 🚀 Implementation Status
* [x] Bridge Plugin (Mongoose Server)
* [x] Svelte 5 Manager Skeleton
* [ ] MIDI Input Plugin (In Progress)
* [ ] MediaPipe Pose Integration (Backlog)

---

### How to use this in Antigravity:
1.  **Save as a Rule:** Create a file named `.antigravity/rules/project-architecture.md`.
2.  **Reference in Chat:** Mention `@project-architecture` when asking the agent to "Create a new plugin for Game Controllers."
3.  **Mission Control:** In the **Manager View**, assign a mission: *"Analyze the bridge plugin and ensure the socket options are optimized for M4 performance."*