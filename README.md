# Desk Puck

Starter firmware for the **ESP32-2424S012** round touch display — the same hardware module as [HomePuck](../HomePuck), but with **no boat, WiFi, or SignalK** code. Use this repo as a clean base for a desk or home application.

## Hardware

| Item | Value |
|------|--------|
| MCU | ESP32-C3, 4 MB flash |
| Display | 240×240 GC9A01 (SPI) |
| Touch | CST816S (I2C) |
| USB | Native USB serial (CDC on boot) |

Pin map and bring-up notes: [docs/hardware.md](docs/hardware.md). Feature ideas: [docs/features/](docs/features/README.md). Architecture: [adr/](adr/README.md).

## Quick start

Prerequisites: [PlatformIO](https://platformio.org/).

```powershell
cd C:\Users\ian\projects\desk_puck
pio run -t upload -t monitor
```

Default serial port is **COM5** in `platformio.ini`. List ports with `pio device list` and adjust `upload_port` / `monitor_port` if needed.

On first boot you should see a **Desk Puck** splash; swipes update the status line and log to serial (`DEBUG_TOUCH=1` in `include/config.h`).

## Project layout

```
desk_puck/
├── include/          config.h, public headers
├── src/
│   ├── main.cpp      setup/loop, touch dispatch
│   ├── LGFX_config.h GC9A01 + SPI (from HomePuck bring-up)
│   ├── app/          app shell, feature registry
│   ├── display/      LovyanGFX init (shared)
│   ├── clock/        clock feature
│   └── patterns/     patterns feature (demo)
├── docs/
├── platformio.ini
├── AGENTS.md         guidance for AI assistants
└── README.md
```

## Related projects

| Repo | Role |
|------|------|
| [HomePuck](../HomePuck) | Same puck, OpenPlotter wind instrument (reference hardware) |
| OpenRemote e-ink dashboard | Different display; optional source for WiFi patterns later |

## License

Add a license when you publish; starter scaffold is private project code until then.
