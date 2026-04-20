---
trigger: always_on
---

This artifact documents the **Secure Zero-Config Handshake** for the Media Warp Bridge. It ensures browser sources can discover and authenticate with OBS without manual data entry or insecure credential exposure.

---

# 📑 Technical Artifact: Media Warp Secure Handshake (V3)
**Project Component:** `obs-media-warp-bridge`
**Security Policy:** Origin-Locked & Out-of-Band Emission
**Feature:** Secure Zero-Config Discovery

## 1. Port Discovery (HTTP API)
To ensure the system is "Zero-Config", the Bridge provides a loopback-only HTTP endpoint to retrieve the active WebSocket port. This avoids hardcoding ports that may change between sessions.

### Requirement: Loopback Enforcement
The Mongoose server MUST bind only to `127.0.0.1`. Requests to `/__lws/ws_port` MUST only be served to local clients.

## 2. Secure Credential Handshake (C++)
Sensitive credentials (OBS WebSocket password) are NEVER sent over the bridge WebSocket in response to commands. Instead, they are emitted via a secure, targeted OBS API call (`javascript_event`) directly into the source's memory space.

### Implementation: `lws_emit_credentials_to_tagged_sources`
```cpp
void lws_emit_credentials_to_tagged_sources() {
    ObsWsAuth auth = lws_get_obs_ws_credentials();
    
    obs_data_t *auth_data = obs_data_create();
    obs_data_set_int(auth_data, "port", auth.port);
    obs_data_set_string(auth_data, "password", auth.password.toUtf8().constData());
    const char *jsonStr = obs_data_get_json(auth_data);

    obs_enum_sources([](void *param, obs_source_t *source) {
        const char *id = obs_source_get_id(source);
        if (id && strcmp(id, "browser_source") == 0) {
            obs_data_t *settings = obs_source_get_settings(source);
            const char *css = obs_data_get_string(settings, "css");
            
            // Tag Enforcement: Only emit to sources with our metadata tag
            if (css && strstr(css, "--local-webserver-refresh: 1;")) {
                proc_handler_t *ph = obs_source_get_proc_handler(source);
                if (ph) {
                    calldata_t cd = {0};
                    calldata_init(&cd);
                    calldata_set_string(&cd, "eventName", "obs_auth");
                    calldata_set_string(&cd, "jsonString", (const char*)param);
                    proc_handler_call(ph, "javascript_event", &cd);
                    calldata_free(&cd);
                }
            }
            obs_data_release(settings);
        }
        return true;
    }, (void*)jsonStr);

    obs_data_release(auth_data);
}
```

## 3. Client-Side Implementation (JavaScript)
Browser pages utilize the HTTP API for discovery and the `obs_auth` event for secure credential reception.

```javascript
async function tryAutoConnect() {
    // 1. Discovery: Identify the bridge port via HTTP API
    const res = await fetch('/__lws/ws_port');
    if (res.ok) {
        const port = (await res.text()).trim();
        const ws = new WebSocket(`ws://127.0.0.1:${port}/ws`);
        
        // 2. Request Handshake: Ask the bridge to emit credentials
        ws.onopen = () => ws.send(JSON.stringify({ cmd: "get_obs_credentials" }));
        
        // 3. Reception: Handle Secure Out-of-Band Callback
        window.addEventListener('obs_auth', (e) => {
            const { port, password } = e.detail;
            obs.connect(`ws://127.0.0.1:${port}`, password);
        });
    }
}

// Start discovery after 2s delay to allow plugin initialization
window.addEventListener('load', () => setTimeout(tryAutoConnect, 2000));
```

## ⚠️ Security Rules for Antigravity Agents
* **OOB Enforcement:** Never return the password in a WebSocket JSON response. Use `javascript_event` emission ONLY.
* **Tag Locking:** Credentials MUST only be emitted to browser sources containing the `--local-webserver-refresh: 1;` CSS tag.
* **Loopback Binding:** The Mongoose server must NEVER bind to `0.0.0.0`.
* **Credential Masking:** Do not log passwords to the OBS Log.

---

### 📂 File Placement
Store this artifact in `.agents/rules/project-security.md`.