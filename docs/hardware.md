# Desk Puck hardware

Same physical module as HomePuck bring-up (August 2026). Values below match `include/config.h`, `src/LGFX_config.h`, and `platformio.ini`.

## Board summary

| Item | Value |
|------|--------|
| **Board name** | ESP32-2424S012 (touch variant **2424S012C**) |
| **MCU** | Espressif **ESP32-C3** (QFN32), 4 MB embedded flash |
| **Display** | 240×240 round IPS, **GC9A01** (SPI) |
| **Touch** | **CST816S**, I2C address `0x15` |
| **USB** | USB-C, native USB Serial/JTAG (`ARDUINO_USB_CDC_ON_BOOT=1`) |

### Variant note

| SKU | Touch |
|-----|--------|
| **2424S012 / 2424S012C** | Yes — **this project** |
| **2424S012N** | No — touch code not applicable |

Product links and community pin docs are collected in [HomePuck hardware.md](../../HomePuck/docs/hardware.md).

---

## USB and programming

| Item | Typical dev value |
|------|-------------------|
| Serial port | `COM5` (see `platformio.ini`) |
| USB VID:PID | `303A:1001` (Espressif) |
| Monitor baud | 115200 |

```powershell
cd C:\Users\ian\projects\desk_puck
pio run -t upload -t monitor
```

---

## Pin map

### Display (GC9A01, SPI)

| Signal | GPIO |
|--------|------|
| SCLK | 6 |
| MOSI | 7 |
| DC | 2 |
| CS | 10 |
| BL | 3 |
| RST | — (not routed; `-1` in driver) |

SPI host `SPI2_HOST`, write clock 40 MHz, panel **invert = true**.

### Touch (CST816S, I2C)

| Signal | GPIO |
|--------|------|
| SDA | 4 |
| SCL | 5 |
| INT | 0 |
| RST | 1 |

---

## PlatformIO target

| Setting | Value |
|---------|--------|
| Environment | `esp32-2424s012` |
| Board ID | `esp32-c3-devkitm-1` (custom pins via `build_flags`) |
| Framework | Arduino |

Libraries: LovyanGFX, CST816S (see `platformio.ini`).

---

## Files referencing hardware

| File | Role |
|------|------|
| `include/config.h` | Pin defines, display size |
| `src/LGFX_config.h` | LovyanGFX panel/bus/light |
| `platformio.ini` | Upload port, `-DPIN_LCD_*` overrides |
| `src/main.cpp` | Touch controller init |
