# Bring-up checklist



Use this the first time you flash **desk_puck** on the round ESP32-C3 module.



## 1. Cable and port



1. Connect USB-C to the puck.

2. Run `pio device list` and note the Espressif port (e.g. `COM5`, serial `84:FC:E6:...`).

3. If not COM5, edit `upload_port` and `monitor_port` in `platformio.ini`.



## 2. Build and upload



```powershell

cd C:\Users\ian\projects\desk_puck

pio run -j 1 -t upload

```



Use `-j 1` if parallel builds fail on Windows. The project uses the **huge_app** partition (~3 MB app slot) for WiFi + Calendar.



Expected esptool summary: **ESP32-C3**, **Embedded Flash 4MB**, USB-Serial/JTAG.



## 3. Serial monitor



```powershell

pio device monitor -b 115200

```



Look for:



```

[APP] Desk Puck v0.3.0 starting...

[CMD] serial wifi/tz ready (help)

[TOUCH] CST816S ready (I2C SDA=4 SCL=5), fw ...

[APP] feature: Clock

[APP] free heap: ... bytes

```



## 4. Display and touch



- Boot shows **Clock** (analogue face; swipe **up** / **down** to cycle registered watch faces).

- Swipe **left** / **right** to switch features; swipe **down** from other features returns to Clock.

- `[APP] feature: ...` lines on serial when the active feature changes.



If touch is dead, confirm you have the **C** (touch) variant and that firmware calls `touch.disable_auto_sleep()` after `begin()`.



**Watch face assets:** `pio run` runs the clock packer automatically. After editing `src/features/clock/assets/faces/<slug>/`, run `python tools/watch_face_pack.py` (requires Pillow).



## 5. WiFi (serial, multi-network)



With the serial monitor open, add one or more networks (up to 5). Same SSID updates the password; a new SSID **adds** a network for use at home, office, etc.



```

wifi add MyHomeSSID mypassword

wifi add "My Office" "pass phrase"

wifi list

wifi status

wifi scan

wifi remove OldSSID

```



After `wifi add`, the puck rescans and connects to the **strongest visible** saved network. The network you last connected to is moved to the front of the list.



Timezone (POSIX string in NVS; default UK-style GMT/BST):



```

tz set GMT0BST,M3.5.0/1,M10.5.0

tz show

```



When WiFi is up, SNTP runs automatically (`[TIME] synced ...` on serial).



## 6. Google Calendar (Diary)



1. Copy [gcal_config.h.example](../src/features/diary/include/gcal/gcal_config.h.example) to **`src/features/diary/include/gcal/gcal_config_private.h`** (gitignored) with your Google Cloud **TVs and Limited Input device** OAuth **client ID** and **secret**.

2. Enable **Google Calendar API**, configure OAuth consent, add your account as a **test user** while the app is in Testing.

3. `wifi add ...` and wait for `[WIFI] connected` and time sync.

4. Swipe to **Diary** → tap **Connect Google Calendar** → scan the **QR** → Allow on your phone.

5. After link, today’s **primary** calendar events appear; swipe up/down to scroll.



See [docs/features/diary/connect-google.md](features/diary/connect-google.md) for the full auth design.



## 7. Switching back to HomePuck



Same USB puck can run HomePuck again:



```powershell

cd C:\Users\ian\projects\HomePuck

git checkout homepuck-2026-10-01

pio run -t upload

```



See [HomePuck restore_firmware.md](../../HomePuck/docs/restore_firmware.md).


