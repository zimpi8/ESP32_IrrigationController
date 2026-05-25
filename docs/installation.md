# Installation

## Voraussetzungen

- PlatformIO installiert
- ESP32-Toolchain verfügbar
- Konfigurierter serieller Port für das Board

## Build

```bash
platformio run
```

## Upload

```bash
platformio run --target upload --upload-port /dev/cu.usbserial-0001
```

Passe den Port an dein System an.

## Weitere Hinweise

Bei Problemen überprüfe:

- Board-Auswahl in `platformio.ini`
- PlatformIO-Bibliotheken
- Kabelverbindungen zum ESP32
