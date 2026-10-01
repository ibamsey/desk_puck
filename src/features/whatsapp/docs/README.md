# Feature: WhatsApp

| Field | Value |
|-------|--------|
| Status | spec (planning only) |
| Depends on | WiFi, message bridge ([message-bridge.md](message-bridge.md)), optional shared provisioning with Diary |
| Primary input | vertical swipe scroll; tap to open connect / settings when unconfigured |

## Summary

**WhatsApp** shows the **latest incoming WhatsApp messages** on the round display—a read-only glance tile, not a full chat client. The puck does **not** talk to Meta directly; a **trusted bridge** on your LAN (or Home Assistant) receives message events and exposes a small **poll API** the firmware can call within ESP32-C3 limits.

## Why it’s fun

The desk puck becomes a **quiet inbox light**: who messaged you and the first line, without unlocking your phone or opening WhatsApp. Pair with Clock and Diary for a calm desk stack.

## Behaviour

### Entry and exit

- **Entry:** Swipe left/right in the app carousel until WhatsApp is active ([ADR-01](../../../adr/ADR-01-touch-navigation-and-app-shell.md)).
- **Exit:** Swipe left/right to other features; swipe down returns to Clock (home).

### First-time / unconfigured

When no bridge URL + token are stored in NVS:

1. WhatsApp shows **“Connect message bridge?”** with a one-line explanation (read-only inbox preview).
2. User **taps** to start setup (WiFi must be up—otherwise prompt to fix WiFi first).
3. Puck shows **bridge URL** (editable via serial in dev) and/or a **QR** pointing at setup docs on phone (v1 may be serial-only; see [message-bridge.md](message-bridge.md#provisioning)).
4. User deploys or enables the bridge, copies **base URL + bearer token** into the puck (QR payload or serial `wifi`-style commands—exact UX TBD).
5. Puck stores credentials and fetches the latest message list.

Full protocol, security, and firmware tasks: **[message-bridge.md](message-bridge.md)**.

After setup, **no daily QR** until the token is rotated or storage is cleared.

### On-screen (configured)

- **Header:** “WhatsApp” (or user label) + short status: `Live`, `Offline`, `Bridge error`, or `Updated 2m ago`.
- **Body:** Newest-first list—**sender or chat name**, **one-line preview**, **relative or local time** (e.g. `14:32` or `5m`).
- **Layout:** List in the round **safe area** (~radius 100 px); chord-aware horizontal inset like Diary; scroll thumb when content overflows.
- **Refresh:** On feature enter if cache stale; periodic poll while feature visible (interval in [message-bridge.md](message-bridge.md)); optional slower background poll when elsewhere (v2).

Until sync ships, firmware may show **placeholder rows** for UI development only.

### Interaction

- **Swipe up / swipe down:** Scroll the message list (inertial scroll matching Diary where possible).
- **Swipe left / right:** Change app feature (not consumed by WhatsApp).
- **Tap:** Connect / Reconnect when unconfigured or auth failed; **no** open-chat or reply in v1.

### Edge cases

- **No WiFi:** Show last cached messages + offline hint; block connect until WiFi works.
- **Empty inbox (no recent messages):** “No recent messages” centred in the list.
- **Bridge unreachable:** Stale cache + header hint; retry on enter and on tick backoff.
- **Auth failure (401/403):** “Bridge link expired” → tap to re-enter token.
- **Oversized previews:** Truncate with ellipsis; strip newlines in preview.
- **Group chats:** Show **group title** as primary line, sender as secondary when bridge provides both (see data model in [message-bridge.md](message-bridge.md)).
- **Media-only messages:** Preview text from bridge, e.g. `[Photo]`, `[Voice message]`—never decode media on device in v1.

## Non-goals (v1)

- Sending messages, reactions, read receipts, or typing indicators.
- Full thread view, search, or contact list.
- Official WhatsApp client or WhatsApp Web session **on the ESP32**.
- End-to-end decryption on puck (bridge sends **already-decrypted preview text** you trust).
- Push notifications when another feature is active (consider v2 with subtle Clock badge).
- Multiple WhatsApp accounts on one puck.

## Technical notes

| Layer | Location (planned) |
|-------|---------------------|
| App shell feature | `src/features/whatsapp/whatsapp_feature.cpp`, `include/whatsapp/whatsapp_feature.h` |
| Message model | `include/whatsapp/whatsapp_types.h` |
| HTTP client + poll | `src/features/whatsapp/whatsapp_sync.cpp` → uses shared TLS/HTTP patterns from Diary (`gcal_http` style) |
| NVS credentials | `whatsapp_store.cpp` (bridge URL, token, last etag / cursor) |
| WiFi | `src/wifi/` |
| Time | Wall clock for timestamps; bridge may send UTC epoch |

**RAM / TLS:** Same constraints as Diary—bounded JSON buffer, fixed max messages (e.g. 20), no large strings. See [message-bridge.md](message-bridge.md).

## Docs in this folder

| Doc | Topic |
|-----|--------|
| [message-bridge.md](message-bridge.md) | Why a bridge is required, backend options (HA, self-hosted), REST contract, sync, cache, security |

## Open questions

- [ ] **Backend choice:** Home Assistant helper vs dedicated `desk-puck-bridge` container (see [message-bridge.md](message-bridge.md#backend-options)).
- [ ] **Provisioning UX:** QR with JSON `{url, token}` vs serial-only for v1.
- [ ] **Poll interval** when WhatsApp feature is visible (30s vs 60s) vs fetch only on enter.
- [ ] **Privacy:** Hide message body on lock / require tap-to-reveal (v2).
- [ ] **Carousel order:** Where WhatsApp sits relative to Diary and Clock.
- [ ] **Shared net module:** Extract generic `http_client` + NVS secret store from Diary for WhatsApp reuse.
