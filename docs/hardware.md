# Hardware

## Benötigte Komponenten

- ESP32-Entwicklungsboard (z. B. `esp32doit-devkit-v1`)
- 8-Kanal-Relaismodul
- OLED-Display 128x64 I²C
- optional DS3231-RTC-Modul
- optional Taster für OLED-Helligkeit

## Pinbelegung

### Relais-Kanäle

- Kanal 1: GPIO 13
- Kanal 2: GPIO 14
- Kanal 3: GPIO 25
- Kanal 4: GPIO 26
- Kanal 5: GPIO 16
- Kanal 6: GPIO 2
- Kanal 7: GPIO 15
- Kanal 8: GPIO 3

### I²C

- SDA: GPIO 5
- SCL: GPIO 4

### Optionaler Helligkeitstaster

- GPIO 34

## Relais-Logik

Der Standardmodus ist `RELAY_ACTIVE_LOW_DEFAULT = true`. Wenn dein Relaismodul aktiv HIGH schaltet, musst du diesen Wert anpassen.
