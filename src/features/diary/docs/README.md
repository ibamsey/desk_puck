# Feature: Diary



| Field | Value |

|-------|--------|

| Status | spec |

| Depends on | WiFi, device OAuth + QR ([connect-google.md](connect-google.md)), Calendar sync ([google-calendar.md](google-calendar.md)), NTP + timezone |

| Primary input | vertical swipe scroll; tap to connect Google when unlinked |



## Summary



**Diary** shows **today’s Google Calendar** as a scrollable list on the round display. The user links Calendar **once** by scanning a **QR code** on the puck and approving access on their phone—no password on the device.



## Why it’s fun



The puck becomes a **calm agenda tile**: no notifications, no phone unlock—just what’s left in the day when you glance down.



## Behaviour



### Entry and exit



- **Entry:** Swipe left/right from Clock until Diary is active ([ADR-01](../../../adr/ADR-01-touch-navigation-and-app-shell.md)).

- **Exit:** Swipe left/right to other features; swipe down returns to Clock (home).



### First-time / unlinked



When Calendar is not linked (no refresh token in NVS):



1. Diary shows **“Connect Google Calendar?”** with a one-line explanation (read-only).

2. User **taps** to connect (WiFi must be up—otherwise prompt to fix WiFi first).

3. Puck shows a **QR code** and the alphanumeric **user code** as fallback.

4. User **scans with phone** → Google asks to grant access to **Desk Puck** → **Allow**.

5. Puck stores credentials and loads today’s events.



Full protocol, screens, and firmware tasks: **[connect-google.md](connect-google.md)**.



After link, **no QR on daily use** until access is revoked or storage is cleared.



### On-screen (linked)



- **Header:** Feature title + local date for the loaded day.

- **Body:** Chronological list—time column (or “All day”) + event title.

- **Layout:** List in the round **safe area** (~radius 100 px); scroll thumb when content overflows.

- **Refresh:** On enter if stale; periodic poll while linked ([google-calendar.md](google-calendar.md)).



Until sync ships, firmware may show **placeholder events** for UI development only.



### Interaction



- **Swipe up / swipe down:** Scroll the agenda (one row per gesture; echo cooldown matches clock faces).

- **Swipe left / right:** Change app feature (not consumed by Diary).

- **Tap:** Connect / Reconnect when unlinked or link expired; no tap action on list rows in v1.



### Edge cases



- **No WiFi:** Cached day if available + offline hint; connect flow blocked until WiFi works.

- **Empty day:** “Nothing scheduled” centred in the list.

- **Sync failure:** Stale cache + short error in header; retry on next enter or tick.

- **Link revoked:** “Calendar link expired” → tap to show QR again.

- **Timezone / DST:** IANA timezone + NTP—see [google-calendar.md](google-calendar.md).

- **Long titles:** Truncate with ellipsis (future).



## Non-goals (v1)



- Week or month views; create/edit events on device.

- Multiple calendars picker (primary only).

- Push notifications or “meeting now” alerts.

- Password or full browser OAuth on the puck (linking is **QR + phone** only).



## Technical notes



| Layer | Location |

|-------|----------|

| App shell feature | `src/diary/diary_feature.cpp`, `include/diary/diary_feature.h` |

| Event model | `include/diary/diary_types.h` |

| Google link (QR) | [connect-google.md](connect-google.md) → `src/diary/gcal_auth.cpp` (planned) |

| Calendar fetch | [google-calendar.md](google-calendar.md) → `diary_sync`, `diary_cache` (planned) |

| WiFi | `src/wifi/` (planned) |

| Time | NTP + TZ; replace compile-only offset for production diary |



## Docs in this folder



| Doc | Topic |

|-----|--------|

| [connect-google.md](connect-google.md) | **Chosen auth:** QR device OAuth, UX states, FSM, impl backlog |

| [google-calendar.md](google-calendar.md) | API, sync, cache, timezone, architecture |



## Open questions



- [ ] WiFi provisioning UX shared across future net features.

- [ ] Poll interval (15 vs 30 min) vs sync only on Diary enter.

- [ ] Highlight current event from wall time.

- [ ] Cancel gesture during active QR polling.


