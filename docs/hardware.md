# Hardware

## Benötigte Komponenten

| Komponente                        | Pflicht | Anmerkung                              |
|-----------------------------------|---------|----------------------------------------|
| ESP32 DevKit V1                   | Ja      | `esp32doit-devkit-v1`                  |
| 8-Kanal-Relaismodul               | Ja      | Aktiv-LOW oder aktiv-HIGH, konfigurierbar |
| OLED-Display SH1106 128×64 I²C    | Ja      | Adresse 0x3C                           |
| DS3231 RTC-Modul                  | Nein    | Empfohlen für Betrieb ohne WLAN        |
| Externer Helligkeits-/AP-Taster   | Nein    | GPIO 34, **benötigt ext. 10 kΩ Pull-Up** |
| Externer NOT-AUS-Taster           | Nein    | GPIO 32, interner Pull-Up              |
| Wasserzähler (Reed/Hall)          | Nein    | GPIO 33, interner Pull-Up              |
| Netzteil 5 V                      | Ja      | Für ESP32 und Relaismodul              |

---

## Pinbelegung

### Relais-Kanäle

| Kanal | GPIO | Anmerkung                                    |
|-------|------|----------------------------------------------|
| 1     | 13   |                                              |
| 2     | 14   |                                              |
| 3     | 25   |                                              |
| 4     | 26   |                                              |
| 5     | 16   |                                              |
| 6     | 2    | Eingebaute LED blinkt beim Booten            |
| 7     | 15   |                                              |
| 8     | 3    | Auch UART RX0 – nicht gleichzeitig nutzen    |

### I²C (OLED + DS3231)

| Signal | GPIO | Anmerkung           |
|--------|------|---------------------|
| SDA    | 5    | Geteilt OLED + RTC  |
| SCL    | 4    | Geteilt OLED + RTC  |

- OLED SH1106 Adresse: `0x3C`
- DS3231 Adresse: `0x68` (fest)

### Taster und Sondereingänge

| Funktion                     | GPIO | Typ        | Pull-Up               |
|------------------------------|------|------------|-----------------------|
| BOOT-/AP-Taster (intern)     | 0    | Eingang    | Intern                |
| Ext. Helligkeits-/AP-Taster  | 34   | Eingang    | **Ext. 10 kΩ** nach 3V3 |
| Ext. NOT-AUS-Taster          | 32   | Eingang    | Intern                |
| Wasserzähler-Impuls          | 33   | Eingang    | Intern                |

> **Wichtig:** GPIO 34 ist Input-only ohne internen Pull-Up. Ohne externen
> 10-kΩ-Widerstand nach 3V3 kann der floating Pin beim Booten ungewollt
> den AP-Modus auslösen. Im Code standardmäßig deaktiviert:
> `#define EXTERN_BRIGHTNESS_BUTTON 0`

---

## Relais-Logik

Standardmäßig `RELAY_ACTIVE_LOW_DEFAULT = true`: Relais schalten bei **LOW**-Signal
(typisch für optoentkoppelte Relaismodule). Falls dein Modul aktiv-HIGH arbeitet,
kannst du den Wert in der Weboberfläche unter **Konfiguration** umschalten.

---

## Anschlussplan

Siehe [schematic.md](schematic.md) für das vollständige Verdrahtungsdiagramm.
