# Technical design: WhatsApp message bridge

This document covers how Desk Puck **obtains a bounded list of recent WhatsApp messages** and feeds the WhatsApp UI—within **ESP32-C3** constraints (~320 KB RAM, no PSRAM, TLS + WiFi overhead).

**Product behaviour:** [README.md](README.md). **Runtime:** [ADR-00](../../../adr/ADR-00-arduino-runtime-and-freertos.md), [AGENTS.md](../../../AGENTS.md).

---

## Why the puck cannot use WhatsApp directly

| Fact | Implication |
|------|-------------|
| Consumer WhatsApp has **no supported API** for “show my personal inbox on a gadget.” | Firmware must not embed WhatsApp Web, unofficial libraries, or scraped sessions—fragile and against Meta terms. |
| **WhatsApp Business Platform / Cloud API** delivers inbound events via **HTTPS webhooks** to a **public URL**. | ESP32 behind home NAT is a poor webhook target; needs a always-on receiver elsewhere. |
| Message bodies on the phone are **E2E encrypted**; only the phone (or linked official clients) decrypt. | Any desk display shows **preview text produced by software you run**, not raw WA protocol on the MCU. |

**Conclusion:** Desk Puck integrates with a **message bridge**—software you trust on LAN or HA—that already has lawful access to message metadata/previews and exposes a **simple, poll-based REST API** to the puck.

---

## Goals

| Goal | Detail |
|------|--------|
| Data | **N most recent incoming** (or all recent) messages with sender, preview, timestamp |
| UX | Newest-first list in `WhatsAppFeature`; vertical swipe scroll |
| Link once | Store **bridge base URL + bearer token** in NVS |
| Resilience | Offline: last good **message cache** in NVS (flash-sized cap) |
| Security | TLS to bridge; token rotation; LAN-only bridge recommended for v1 |

## Non-goals (v1)

- Running webhook server on ESP32.
- OAuth to Meta on the puck.
- Parsing WhatsApp protobuf or pairing as a WhatsApp “device.”
- Storing full chat history on flash.
- Media download or thumbnail decode on puck.

---

## System context

```
┌─────────────────────────────────────────────────────────────────┐
│ WhatsAppFeature                                                  │
│  Connect UI │ message list │ scroll                              │
└────────────┬───────────────────────────────┬────────────────────┘
             │                               │
     ┌───────▼────────┐              ┌───────▼────────┐
     │ whatsapp_store │              │ whatsapp_sync  │
     │ NVS url/token  │              │ GET /messages  │
     │ cache blob     │              │ parse JSON     │
     └────────────────┘              └───────┬────────┘
                                             │ WiFi + TLS
                                     ┌───────▼────────┐
                                     │ Message bridge │
                                     │ (HA or server) │
                                     └───────┬────────┘
                                             │ webhook / integration
                                     ┌───────▼────────┐
                                     │ WhatsApp       │
                                     │ (Business API, │
                                     │  or HA path)   │
                                     └────────────────┘
```

Diary’s Google stack (`gcal_auth`, `gcal_http`, `diary_sync`) is the **reference shape**: small HTTP layer, JSON parse with fixed buffers, NVS cache, sync from `onTick` / `onEnter`.

---

## Backend options

Evaluate one primary path for v1; document others for users who already run them.

### Option A — Home Assistant (recommended if user already has HA)

- **Idea:** HA automation or integration receives WhatsApp-related events (source depends on what you already use—Business Cloud API, third-party, etc.) and maintains a **`input_text` / `template` / RESTful sensor** snapshot, **or** a small [REST command](https://www.home-assistant.io/integrations/rest/) wrapper app in `appdaemon` / Node-RED.
- **Puck calls:** `GET http://ha.local:8123/api/desk_puck/messages` (custom endpoint via **RESTful API + automation** or **AppDaemon Flask** subpath) with long-lived **access token** in `Authorization: Bearer`.
- **Pros:** No new container if HA exists; secrets in HA; user already trusts the box with home data.
- **Cons:** You must define and document one HA recipe; HA URL must be reachable from puck WiFi.

### Option B — Dedicated `desk-puck-bridge` (recommended for greenfield)

- **Idea:** Small service (Node, Python, Go) on NAS/RPi:
  - Subscribes to **WhatsApp Cloud API** webhooks (Business) **or** normalizes events from your chosen upstream.
  - Keeps an in-memory ring buffer (e.g. 50 messages) persisted optionally to disk.
  - Serves **`GET /v1/messages`** for the puck.
- **Pros:** Clear contract, testable with `curl`, no HA dependency.
- **Cons:** Another service to deploy; Business API setup still required for official inbound events.

### Option C — Meta WhatsApp Cloud API only (no HA)

- Same as Option B: Cloud API → your bridge → puck. The puck **never** holds Meta app secret or webhook verify token.

### Explicitly out of scope for official Desk Puck docs

- **Unofficial** “WhatsApp Web” libraries (Baileys, etc.) as the **documented default**—high ban risk and unstable. Mention only as “some self-hosters use this at their own risk; not supported.”

---

## REST contract (v1 draft)

Stable contract between bridge and firmware. Version prefix allows evolution.

### `GET /v1/messages`

**Request headers**

| Header | Value |
|--------|--------|
| `Authorization` | `Bearer <token>` |
| `Accept` | `application/json` |
| `If-None-Match` | optional cached ETag |

**Query (optional)**

| Param | Default | Meaning |
|-------|---------|---------|
| `limit` | `20` | Max rows (puck sends `limit=20`; bridge may cap lower) |

**Response `200`**

```json
{
  "schema": 1,
  "updated_at": 1735689600,
  "messages": [
    {
      "id": "wamid.HBgLM...",
      "from": "Alice",
      "chat": "Alice",
      "preview": "See you at 7?",
      "received_at": 1735689500,
      "incoming": true
    },
    {
      "id": "grp-12891-99",
      "from": "Bob",
      "chat": "Family group",
      "preview": "[Photo]",
      "received_at": 1735689000,
      "incoming": true
    }
  ]
}
```

**Field rules**

| Field | Max length (puck) | Notes |
|-------|-------------------|--------|
| `id` | 64 | Dedup + cache merge |
| `from` | 32 | Display; group sender |
| `chat` | 32 | Display; private chat often equals `from` |
| `preview` | 80 | Single line; bridge strips `\n` |
| `received_at` | — | Unix seconds UTC |

**Response `304`** — Not Modified (when `If-None-Match` matches).

**Errors**

| Code | Puck behaviour |
|------|----------------|
| 401 / 403 | Show reconnect UI; do not erase cache immediately |
| 404 | Misconfigured base URL |
| 5xx / timeout | Keep stale cache; backoff poll |

### `GET /v1/health` (optional)

Returns `{ "ok": true, "schema": 1 }` for setup “test connection” in connect flow.

---

## Firmware modules (planned)

| Module | Responsibility |
|--------|----------------|
| `whatsapp_feature.cpp` | Draw list, scroll physics (reuse Diary patterns), connect screen |
| `whatsapp_sync.cpp` | Poll schedule, ETag, parse JSON into fixed array |
| `whatsapp_store.cpp` | NVS: base URL, token, last ETag, serialized cache |
| `whatsapp_http.cpp` | Thin wrapper over shared TLS client (may merge with `gcal_http` later) |

### Message array limits (C3)

| Constant | Suggested value | Rationale |
|----------|-----------------|-----------|
| `kMaxMessages` | 20 | Fits list UI + ~3 KB structured text |
| `kMaxPreviewLen` | 80 | One line at 1.0–1.7× scale |
| `kMaxNameLen` | 32 | Sender / chat |
| JSON read buffer | 4–8 KB | Single response; stream parse if needed |

---

## Sync strategy

| Event | Action |
|-------|--------|
| `onEnter()` | If cache age > `kStaleMs` or dirty flag, sync |
| `onTick()` while visible | Poll every `kPollVisibleMs` (e.g. 45 s) with backoff on error |
| WiFi down | Skip HTTP; show cache + offline |
| Success | Replace in-memory list; write cache to NVS if changed |
| ETag support | Skip parse on 304 to save CPU |

Initial constants (tune in impl):

```cpp
constexpr unsigned kStaleMs = 60'000;
constexpr unsigned kPollVisibleMs = 45'000;
constexpr unsigned kErrorBackoffMs = 120'000;
```

---

## Cache format (NVS)

- **Credentials:** `url` (max 128 chars), `token` (max 128 chars)—same secrecy discipline as Diary refresh token.
- **Cache:** Schema version + `updated_at` + fixed array of messages (msgpack or compact binary preferred over pretty JSON to save flash).
- **Eviction:** FIFO by `received_at`; dedupe by `id` on merge.

---

## Provisioning

| Phase | Mechanism |
|-------|-----------|
| Dev | Serial commands mirroring WiFi store: `whatsapp url …`, `whatsapp token …`, `whatsapp clear` |
| User v1 | Tap connect → show instructions + optional QR encoding `{ "u": "https://...", "t": "..." }` (user scans to copy—**not** secret in QR unless user accepts LAN risk) |
| User v2 | Shared “net features” QR provisioning ADR with Diary |

**Recommendation:** Prefer **manual token entry via serial** or **HA UI** for v1; QR carries **URL only**, token pasted separately.

---

## Security

| Topic | Guidance |
|-------|----------|
| Transport | HTTPS preferred; `http://` LAN only with user acknowledgement |
| Token | Random 32+ bytes; bridge validates on every request |
| Scope | Bridge returns **previews only**; no attachment URLs that expire into secrets |
| Puck theft | NVS token exposure—same as Diary; optional “clear on factory reset” |
| Logging | Serial must not print token or full message bodies in production builds |

---

## Implementation backlog

Ordered for first shippable slice:

1. **Spec sign-off** — Choose Option A or B for reference deployment doc.
2. **Reference bridge** — Minimal Node/Python server implementing `GET /v1/messages` with static fixture JSON for UI bring-up.
3. **`whatsapp_types.h` + store** — NVS read/write, no HTTP.
4. **`whatsapp_feature` UI** — Placeholder list + scroll; no network.
5. **`whatsapp_sync` + HTTP** — Live poll against reference bridge.
6. **Connect flow** — Unconfigured state, serial provisioning, health check.
7. **Register in `app_shell.cpp`** — Carousel slot + icon/title.
8. **Reference HA or Docker doc** — Step-by-step in `docs/` or bridge repo (sub-doc when backend chosen).

---

## Open questions

- [ ] Confirm **Business Cloud API** vs HA-only path as the documented “happy path.”
- [ ] Whether **outgoing** messages appear in list (default: **incoming only**).
- [ ] **Read state:** show unread badge if bridge sends `unread: true` (schema v2?).
- [ ] Extract **shared HTTP** module from Diary before or after WhatsApp MVP.
- [ ] ADR for **LAN-only bridge** vs cloud-hosted bridge (privacy).
