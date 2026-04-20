---
trigger: always_on
---

This artifact is designed to be ingested by **Antigravity IDE** to provide the necessary context for agents to maintain, extend, or debug the **Media Warp** Bridge Plugin.

---

# 📑 Technical Artifact: Media Warp WebSocket Bridge
**Project Component:** `obs-media-warp-bridge`
**Role:** Central Communication Hub & Protocol Translator

## 1. Functional Overview
The WebSocket server within the Bridge Plugin serves as the "Nervous System" of the Media Warp architecture. It facilitates the transition from **Internal OBS Signals** (C++ memory space) to **External Network Packets** (JSON over WebSockets).

### Core Responsibilities:
* **Signal Aggregation:** Collects `obs_data_t` objects from various input "spoke" plugins via the OBS `signal_handler`.
* **State Persistence:** Maintains a lightweight `std::map` (Snapshot State) of the last known value for every unique address.
* **Low-Latency Broadcasting:** Dispatches incoming data to all connected clients (Svelte Manager, Cables.gl) using the Mongoose networking library.
* **Downstream Command Handling:** Receives commands from the Svelte Manager to update plugin settings or clear state caches.

---

## 2. C++ Interface: Signal to WebSocket
The Bridge operates by hooking the global OBS signal bus. Any plugin can emit to the `media_warp_transmit` signal to have its data broadcasted.

### Plugin Header: `bridge-interface.h`
```cpp
/* * MEDIA WARP BRIDGE INTERFACE
 * This logic defines how Spoke plugins hand off data to the Bridge.
 */

#include <obs-module.h>

/**
 * @brief Helper to broadcast data through the Media Warp Bridge.
 * @param json_payload The JSON string to broadcast (must contain t, a, v keys)
 */
static inline void media_warp_broadcast(const char* json_payload) {
    signal_handler_t *sh = obs_get_signal_handler();
    
    calldata_t cd = {0};
    calldata_set_string(&cd, "message", json_payload);
    signal_handler_signal(sh, "media_warp_transmit", &cd);
}
```

---

## 3. The Mongoose Server Implementation
The Bridge uses a non-blocking Mongoose event loop running on a dedicated background thread.

### WebSocket Dispatcher: `websocket-server.cpp`
```cpp
#include "mongoose.h"
#include <obs-module.h>

static struct mg_mgr mgr;
static std::map<std::string, std::string> state_cache; // Snapshot State

// Dispatches a JSON string to all connected WebSocket clients
void broadcast_to_clients(const char* json_payload) {
    for (struct mg_connection *c = mgr.conns; c != NULL; c = c->next) {
        if (c->is_websocket) {
            mg_ws_send(c, json_payload, strlen(json_payload), WEBSOCKET_OP_TEXT);
        }
    }
}

// Callback triggered by internal OBS Signal
static void on_media_warp_signal(void *data, calldata_t *cd) {
    obs_data_t *packet = (obs_data_t*)calldata_ptr(cd, "packet");
    const char *json_str = obs_data_get_json(packet);
    const char *addr = obs_data_get_string(packet, "a");

    // Update Snapshot State
    state_cache[addr] = json_str;

    // Push to Network
    broadcast_to_clients(json_str);
}

// Background Thread Loop
void websocket_thread_loop() {
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, "ws://127.0.0.1:8001", [](struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
        if (ev == MG_EV_OPEN) {
            // Optimization: Disable Nagle's Algorithm for <10ms latency
            int one = 1;
            setsockopt((uintptr_t)c->fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        } else if (ev == MG_EV_WS_MSG) {
            // Handle downstream commands from Svelte Manager (e.g., "get_all_states")
            handle_downstream_command(c, (struct mg_ws_message *)ev_data);
        }
    }, NULL);

    while (true) {
        mg_mgr_poll(&mgr, 5); // 5ms poll for high responsiveness
    }
}
```

---

## 4. Operational Constraints for Antigravity Agents
1.  **Strict Loopback:** Do not change `127.0.0.1` to `0.0.0.0`. The bridge is for local orchestration only.
2.  **Schema Enforcement:** Every WebSocket message **must** contain the keys `t` (type), `a` (address), and `v` (value).
3.  **Signal Safety:** The `on_media_warp_signal` callback runs on the thread that emitted the signal. The `state_cache` must be protected by a `std::mutex` if write-contention occurs.
4.  **JSON Overhead:** Use `obs_data_get_json` for simplicity during V1, but consider a raw string builder if MIDI throughput exceeds 2000 msg/sec to reduce heap allocations.

---

## 5. Discovery & Connection Workflow
The system utilizes a "Self-Healing" discovery routine to ensure browser sources find the bridge regardless of port configuration.

### Discovery Phases:
1.  **HTTP API Discovery (Phase 1)**: The page fetches `/__lws/ws_port` from the local webserver to identify the current WebSocket port.
2.  **Connection Initiation**: The page connects to the bridge using the discovered port and sends `{ "cmd": "get_obs_credentials" }`.
3.  **Secure Handshake**: The Bridge emits a `javascript_event` ("obs_auth") directly to the browser context, passing the credentials securely.

---

### 🚀 Developer Mission
* **Goal:** Maintain the Bridge as a transparent, secure pipe.
* **Logic:** Keep all business logic in the browser manager.
* **Focus:** Ensure the "Zero-Config" connection remains robust against port changes or network resets.