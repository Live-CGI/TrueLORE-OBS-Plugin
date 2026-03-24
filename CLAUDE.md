# lore-obs

OBS Studio plugin for the Lore streaming platform. Authenticates users, manages broadcasts, and mirrors the stream to Lore's RTMP ingest — all from within OBS.

## Tech Stack

- **Language:** C/C++ (mixed — C for OBS module API, C++ for Qt UI and HTTP)
- **Build System:** CMake 3.28–3.30
- **OBS SDK:** OBS Studio 31.1.1 (libobs + obs-frontend-api)
- **UI:** Qt 6 (Widgets, Core, Network)
- **HTTP:** libcurl (sync + async via taffy_json_http)
- **JSON:** nlohmann/json (header-only)
- **Crypto:** OpenSSL (AES skeleton — unused currently)
- **Platforms:** macOS (ARM64/x86), Windows, Linux
- **Installer:** Inno Setup (Windows)

## Commands

```bash
# macOS
cmake --preset macos -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build_macos --config RelWithDebInfo

# Windows
cmake --preset windows-x64
cmake --build build_windows --config RelWithDebInfo
```

Pre-built OBS deps (obs-deps 2025-07-11, Qt6 2025-07-11) are fetched automatically by the CMake bootstrap.

## Project Structure

```
src/
  plugin-main.c                # OBS module entry point (obs_module_load)
  taffylabs_settings.c/.h      # Settings persistence (OBS config file)
  taffylabs_ui.cpp/.h          # Qt settings dialog, broadcast dropdown, login UI
  taffylabs_mirror.c/.h        # RTMP mirror output lifecycle (start/stop)
  lore-api.cpp/.h              # Lore API client (broadcasts, keys, auth)
  lore-auth.cpp/.h             # Device login OAuth2 flow (3-step)
  lore-deviceInfo.cpp/.h       # Hardware UUID, geolocation, OBS scene JSON
  taffy_json_http.cpp/.h       # libcurl HTTP wrapper (sync + async)
  aes_string.cpp/.h            # AES encryption skeleton (unused)
  taffy-tools.c                # Utility functions
  plugin-support.h             # OBS macros
cmake/                         # Platform-specific CMake configs
data/                          # Plugin data files
InnoSetup/                     # Windows installer config
buildspec.json                 # OBS plugin metadata (name, version, deps)
```

## Architecture

### Plugin Lifecycle

1. `obs_module_load()` — OBS loads the plugin
2. Generates unique device ID (`taffylabs_get_unique_device_id_json()`)
3. Initializes UI (`taffylabs_ui_init()`) — adds "TaffyLabs Lore Settings" to Tools menu
4. Registers streaming event callbacks:
   - `OBS_FRONTEND_EVENT_STREAMING_STARTED` → `taffylabs_mirror_start(server, key)`
   - `OBS_FRONTEND_EVENT_STREAMING_STOPPED` → `taffylabs_mirror_stop()`

### Authentication (Device Login Flow)

Three-step OAuth2 device flow — no browser session needed inside OBS:

1. **Begin:** `GET /auth/device-login?rid=<64-char-hex>` → returns Google OAuth URL + state token
2. **Poll:** `GET /auth/device-login/check?rid=<id>&state=<state>` → polls every 1s for up to 90s
3. **Exchange:** `POST /auth/device-login/exchange-code` with `{"code":"..."}` → returns JWT

JWT stored in OBS config (`bearerToken` key). Validated on UI init by calling `GET /broadcasts`.

### RTMP Mirror

When the user starts streaming in OBS, the plugin creates a second RTMP output that mirrors to Lore:

- Creates `obs_service_create("rtmp_custom", ...)` with Lore server + stream key
- Shares the primary stream's audio/video encoders (no re-encode)
- User streams to their normal destination AND Lore simultaneously

### Broadcast Management

- **List:** `GET /broadcasts` — populates dropdown in settings UI
- **Create:** `POST /broadcasts` with metadata (title, description, deviceInfo)
- **Auto-generate:** If enabled, creates a new broadcast on each stream start
- **Update:** `PATCH /broadcasts/:id` — update metadata

### Metadata Sent to Backend

```json
{
  "metadata": {
    "title": "New Broadcast 2026-03-21 14.30.00 UTC",
    "description": "New Broadcast created at 14.30.00 UTC on 2026-03-21",
    "deviceInfo": {
      "ok": true,
      "platform": "macos",
      "idType": "IOPlatformUUID",
      "deviceId": "<hardware-uuid>"
    }
  }
}
```

Device info sources:
- **macOS:** `IOPlatformUUID` (CoreLocation for geolocation)
- **Windows:** `Win32_ComputerSystemProduct.UUID` (WMI, PowerShell geolocation)
- **Linux:** `/etc/machine-id` or `/var/lib/dbus/machine-id` (GeoClue2)

## Settings (OBS Config)

Stored in OBS config section `[taffylabs]`:

| Key | Type | Description |
|-----|------|-------------|
| `enabled` | bool | Plugin enabled |
| `auto_broadcast` | bool | Auto-create broadcast on stream start |
| `optional_metadata_enabled` | bool | Include optional metadata fields |
| `title` | string | Broadcast title |
| `description` | string | Broadcast description |
| `bearerToken` | string | JWT from device login |
| `streamKey` | string | Current stream key |
| `streamServer` | string | RTMP server URL |
| `broadCast` | string | Selected broadcast JSON |
| `uniqueID` | string | Device ID JSON |

## API Endpoints Used

| Method | Path | Auth | Purpose |
|--------|------|------|---------|
| GET | `/auth/device-login` | None | Begin device login |
| GET | `/auth/device-login/check` | None | Poll for login completion |
| POST | `/auth/device-login/exchange-code` | None | Exchange code for JWT |
| GET | `/broadcasts` | Bearer | List broadcasts |
| POST | `/broadcasts` | Bearer | Create broadcast |
| PATCH | `/broadcasts/:id` | Bearer | Update broadcast |
| DELETE | `/broadcasts/:id` | Bearer | Delete broadcast |
| GET | `/keys` | Bearer | List RSA keys |
| GET | `/keys/generate` | Bearer | Generate RSA keypair |
| GET | `/keys/:id/get` | Bearer | Get public key |
| DELETE | `/keys/:id` | Bearer | Delete keypair |

## TODOs

- [ ] **Configurable API URL:** API base URL is hardcoded to `https://dev-api.tome-lore.com/` in `lore-api.cpp:17`. RTMP server is hardcoded to `rtmps://dev-rtmp.tome-lore.com:443/live` in `taffylabs_mirror.c:11`. Both need to be configurable (settings UI or fetched from API) for dev/staging/prod environments.
- [ ] **Stop notification to lore-api:** Plugin only calls `obs_output_stop()` on stream end — no API call to signal the stream has stopped. Should call `PATCH /broadcasts/:id` with `{ "state": "complete" }`. Currently relies on RTMP disconnect detection server-side (Nginx `on_publish_done`).
- [ ] **Reconnect handling:** Device-login has a hard 90s timeout with no retry. RTMP mirror has no reconnect logic — if it drops, it stays down. Add exponential backoff retry for both auth polling and mirror output.
- [ ] **Encrypt stored bearer token:** JWT is stored in plaintext via OBS config (`taffylabs_settings.c`). The AES encryption skeleton in `aes_string.cpp` exists but is unused. Not critical for MVP but should be revisited.
- [ ] **Token refresh / expiry handling:** No token refresh mechanism. Token is validated on UI init via `GET /broadcasts`. If JWT expires mid-stream, API calls fail silently (RTMP mirror keeps running since it doesn't need the JWT). Consider token refresh or longer-lived tokens.
- [ ] **Full ILoreMetadata support:** Plugin sends `title`, `description`, and `deviceInfo` but doesn't populate the full `ILoreMetadata` spec (no `tv`/`music`/`podcast` content-type extensions). The `publicKeyUrl` is added server-side. Content-type extensions could be a future UI addition.
- [ ] **Consistent naming:** Files use mixed prefixes — `taffylabs_*` (old project name) and `lore-*` (new). Rename for consistency when convenient.

## Workspace — Related Projects

The LORE platform spans seven repositories that work together:

| Repo | Language | Purpose |
|------|----------|---------|
| lore-obs | C/C++ (OBS plugin) | OBS Studio plugin — this repo |
| lore-ui | TypeScript (Next.js 15) | Frontend — playback, signature verification, ledger |
| lore-api | TypeScript (NestJS 11) | Backend API, auth, DB, key management |
| lore-contract | Solidity + TypeScript | Smart contracts (LoreStreams on Polygon) |
| viadpl-media-server | Node.js + Nginx + Go | Streaming infrastructure: RTMP ingest, HLS signing, recording, S3 sync |
| go-stream-signing-tool | Go | RSA signing of HLS/MP4 streams (static, watch, verify modes) |
| go-sync-folder | Go | Watches a folder and syncs to S3 via AWS CLI |

### Data Flow (end-to-end)

```
lore-obs (OBS plugin)
  → Auth via device-login flow (lore-api)
  → POST /broadcasts → stream_key + RSA public key
  → RTMP mirror to rtmps://dev-rtmp.tome-lore.com:443/live/<stream_key>
  → viadpl-media-server (Nginx RTMP ingest, validates stream_key against DB)
  → ffmpeg (fMP4 HLS segments)
  → go-stream-signing-tool (signs segments, writes playlist-signed.m3u8)
  → go-sync-folder (syncs signed HLS to S3)
  → CloudFront (serves signed HLS)
  → lore-ui (playback + real-time signature verification in browser)
  → lore-contract (on-chain recording of stream lifecycle via lore-api)
```
