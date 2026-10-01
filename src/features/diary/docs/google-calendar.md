# Technical design: Google Calendar for Desk Puck



This document covers how Desk Puck **fetches one local day** of Google Calendar events and feeds the Diary UI—within **ESP32-C3** constraints (~320 KB RAM, no PSRAM, TLS + WiFi overhead).



**Authentication (chosen):** on-puck **device OAuth + QR** — full UX, protocol, FSM, NVS, and task breakdown in **[connect-google.md](connect-google.md)**.



**Product behaviour:** [README.md](README.md). **Runtime:** [ADR-00](../../../adr/ADR-00-arduino-runtime-and-freertos.md), [AGENTS.md](../../../AGENTS.md).



---



## Goals



| Goal | Detail |

|------|--------|

| Data | Events for **today** in the user’s **primary calendar timezone** |

| UX | List in `DiaryFeature`; vertical swipe scrolls cached rows |

| Link once | QR + phone consent → refresh token in NVS; see [connect-google.md](connect-google.md) |

| Resilience | Offline: last good **day cache** in NVS |



## Non-goals (v1)



- Calendar **write** access.

- Multi-account picker on puck.

- Companion-only provisioning (CLI) as the primary path—optional dev shortcut only.

- Home server token relay (future ADR if direct Google proves too heavy).

- Push notification channels.



---



## System context



```

┌──────────────────────────────────────────────────────────────────┐

│ DiaryFeature                                                      │

│  Link UI (connect-google) │ agenda list │ scroll                  │

└────────────┬─────────────────────────────┬────────────────────────┘

             │                             │

     ┌───────▼────────┐            ┌───────▼────────┐

     │  gcal_auth     │            │  diary_sync    │

     │  device QR     │            │  events.list   │

     │  refresh token │            │  JSON → day    │

     └───────┬────────┘            └───────┬────────┘

             │                             │

             └──────────────┬──────────────┘

                            │

                    ┌───────▼────────┐

                    │ wifi_station   │

                    │ SNTP + TZ      │

                    └────────────────┘

```



Sync runs only when **`gcal_auth` reports linked** (valid refresh token). Unlinked Diary shows connect flow only—no placeholder Google data in production builds.



---



## Google API surface



**[Calendar API v3](https://developers.google.com/calendar/api/v3/reference)** over HTTPS.



### List events for today



```http

GET https://www.googleapis.com/calendar/v3/calendars/{calendarId}/events

Authorization: Bearer {access_token}

```



| Query param | Value |

|-------------|--------|

| `timeMin` | Start of local day, RFC3339 |

| `timeMax` | Start of next local day, RFC3339 |

| `singleEvents` | `true` |

| `orderBy` | `startTime` |

| `maxResults` | `50` (cap in firmware) |

| `fields` | Partial response below |



```

fields=items(id,summary,start,end,status),timeZone,updated

```



| JSON field | Handling |

|------------|----------|

| `items[].summary` | Title; truncate (~48 chars) |

| `items[].start.date` | All-day |

| `items[].start.dateTime` | Timed; parse to local minutes-from-midnight |

| `items[].status` | Skip `cancelled` |



**Calendar ID:** `primary` in NVS default; overridable later.



**Access token:** from `gcal_auth_get_access_token()` (refresh via `grant_type=refresh_token`). Details in [connect-google.md](connect-google.md).



---



## Timezone and “today”



| Source | Use |

|--------|-----|

| SNTP | Wall clock instant |

| IANA TZ in NVS (`gcal/tz` or global time module) | Local midnight boundaries |

| `timeZone` in API response | Cross-check on first successful sync |



**Algorithm:**



1. `time()` from SNTP-synchronized clock.

2. Local `struct tm` via `setenv("TZ", tz, 1); tzset();` + `localtime_r`.

3. Build `timeMin` = today 00:00:00 local, `timeMax` = tomorrow 00:00:00 local.

4. Format RFC3339 with numeric offset.



Compile-time `CLOCK_UTC_OFFSET_SEC` alone is **insufficient** once Diary ships; NTP + TZ required ([connect-google.md — prerequisites](connect-google.md#implementation-prerequisites)).



---



## Firmware architecture



### Module split



| Module | Responsibility |

|--------|----------------|

| `src/wifi/wifi_station.cpp` | STA connect, NVS credentials |

| `src/diary/gcal_auth.cpp` | Device OAuth + refresh; see [connect-google.md](connect-google.md) |

| `src/diary/diary_sync.cpp` | HTTPS GET, parse, fill `DiaryDay` |

| `src/diary/diary_cache.cpp` | NVS serialize last day |

| `src/diary/diary_feature.cpp` | UI: link states + list + scroll |



**Loop discipline:** TLS and polling via state machines in `onTick` / `gcal_auth_poll()` / `diary_sync_poll()`—no multi-second `delay()` in connect or sync paths.



### RAM model



| Structure | Bound |

|-----------|--------|

| `DiaryEvent events[50]` | Fixed count |

| Title storage | Fixed char pool (e.g. 50 × 49 bytes + NUL) |

| JSON | Single `StaticJsonDocument`; size from sample payload + `measureJson` |

| QR matrix | One static buffer; free after link screen closes if possible |



Log `ESP.getFreeHeap()` after WiFi connect, after first token refresh, after first parse—maintain **≥80 KB** free heap target.



---



## HTTP / TLS



| Piece | Choice |

|-------|--------|

| TLS | `WiFiClientSecure` + platform CA bundle |

| HTTP | `HTTPClient`, timeout ~10 s |

| Hosts | `oauth2.googleapis.com`, `www.googleapis.com` |



---



## Sync policy



| Trigger | Action |

|---------|--------|

| Enter Diary while linked | If cache date ≠ today OR age &gt; TTL (e.g. 15 min), `diary_sync_request()` |

| `onTick` while linked | Poll sync FSM; detect calendar day rollover → invalidate cache |

| WiFi down | Show NVS cache + offline indicator |

| Not linked | Connect UI only; no sync |



Later: `If-None-Match` / incremental sync to save bandwidth.



---



## Day cache (NVS)



Namespace e.g. `diary` (separate from `gcal` auth keys):



```

magic: "DPCK"  (4 bytes)

version: 1

date: YYYYMMDD (local)

event_count: u8

repeat event_count:

  title_len, title[...], start_min, end_min, flags (all_day)

```



Max blob ~4 KB. Load on boot for instant paint before network.



---



## Security checklist



- [ ] Refresh token + client secret not in git; use `gcal_config.h` from example template.

- [ ] Scope: `calendar.readonly` only.

- [ ] HTTPS only.

- [ ] Production builds: no token or event logging on serial.



---



## Implementation phases (rollup)



Detailed tasks: **[connect-google.md — Implementation plan](connect-google.md#implementation-plan-task-breakdown)**.



| Phase | Deliverable | Status |

|-------|-------------|--------|

| **0** | Diary list UI + placeholder data; design docs | done |

| **A** | WiFi STA + SNTP + TZ | planned |

| **B** | `gcal_auth` device flow + refresh + NVS | planned |

| **C** | QR + Diary link UI | planned |

| **D** | `diary_sync` + cache + live agenda | planned |

| **E** | bringup.md operator guide + revoke/relink tests | planned |



**Dev shortcut (optional):** Python script in `tools/gcal-provision/` to paste refresh token into NVS over serial—**not** the user-facing path; QR flow remains canonical.



---



## Deferred alternatives (not v1)



| Approach | When to revisit |

|----------|-----------------|

| Home Assistant / relay | Heap or OAuth verification blocked on-device |

| Service account | Workspace fleet deploy |

| Email-only / push-without-QR | Not supported by Google for third-party Calendar OAuth |



---



## Open decisions



- [ ] WiFi provisioning UX (captive portal vs serial vs WPS).

- [ ] Continue OAuth poll when user swipes away from Diary during linking.

- [ ] FreeRTOS worker task vs pure `loop()` FSM for HTTPS ([ADR-00](../../../adr/ADR-00-arduino-runtime-and-freertos.md)).



---



## References



- [connect-google.md](connect-google.md) — **auth + QR + impl plan**

- [Events: list](https://developers.google.com/calendar/api/v3/reference/events/list)

- [Partial responses](https://developers.google.com/calendar/api/guides/performance#partial)

- [Device OAuth](https://developers.google.com/identity/protocols/oauth2/limited-input-device)


