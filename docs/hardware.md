# Hardware

## Benötigte Komponenten

| Komponente                        | Pflicht | Anmerkung                              |
|-----------------------------------|---------|----------------------------------------|
| ESP32 DevKit V1                   | Ja      | `esp32doit-devkit-v1`                  |
| 8-Kanal-Relaismodul               | Ja      | Aktiv-LOW oder aktiv-HIGH, konfigurierbar |
| OLED-Display SH1106 128×64 I²C    | Ja      | Adresse 0x3C                           |
| DS3231 RTC-Modul                  | Nein    | Empfohlen für Betrieb ohne WLAN        |
| Externer Helligkeits-/AP-Taster   | Nein    | GPIO 34, **benötigt ext. 10 kΩ Pull-Up** |
| Externer NOT-AUS-Taster           | Nein    | GPIO 35, **benötigt ext. 10 kΩ Pull-Up** |
| Wasserzähler (Reed/Hall)          | Nein    | GPIO 36 (VP), **benötigt ext. 10 kΩ Pull-Up** |
| Netzteil 5 V                      | Ja      | Für ESP32 und Relaismodul              |

---

## Pinbelegung

### Relais-Kanäle

| Kanal | GPIO | Boardpin | Seite       | Anmerkung                          |
|-------|------|----------|-------------|------------------------------------|
| 1     | 13   | D13      | Oben        |                                    |
| 2     | 17   | TX2      | Unten       | GPIO12 (Strapping-Pin) übersprungen |
| 3     | 14   | D14      | Oben        |                                    |
| 4     | 27   | D27      | Oben        |                                    |
| 5     | 26   | D26      | Oben        |                                    |
| 6     | 25   | D25      | Oben        |                                    |
| 7     | 33   | D33      | Oben        |                                    |
| 8     | 32   | D32      | Oben        |                                    |

> Kanäle 1, 3–8 liegen auf der oberen Pinreihe (D13, D14, D27, D26, D25, D33, D32)
> und können mit einem Flachbandkabel verbunden werden. GPIO12 zwischen D13 und D14
> wird übersprungen (Strapping-Pin: HIGH beim Booten → falscher Flash-Modus).
> Kanal 2 liegt auf TX2 (untere Reihe) und benötigt eine separate Leitung.

### I²C (OLED + DS3231)

| Signal | GPIO | Anmerkung           |
|--------|------|---------------------|
| SDA    | 5    | Geteilt OLED + RTC  |
| SCL    | 4    | Geteilt OLED + RTC  |

- OLED SH1106 Adresse: `0x3C`
- DS3231 Adresse: `0x68` (fest)

### Taster und Sondereingänge

| Funktion                     | GPIO    | Boardpin | Typ     | Pull-Up                    |
|------------------------------|---------|----------|---------|----------------------------|
| BOOT-/AP-Taster (intern)     | 0       | BOOT     | Eingang | Intern                     |
| Ext. Helligkeits-/AP-Taster  | 34      | D34      | Eingang | **Ext. 10 kΩ** nach 3V3   |
| Ext. NOT-AUS-Taster          | 35      | D35      | Eingang | **Ext. 10 kΩ** nach 3V3   |
| Wasserzähler-Impuls          | 36      | VP       | Eingang | **Ext. 10 kΩ** nach 3V3   |

> **Wichtig:** GPIO 34, 35 und 36 (VP) sind Input-only ohne internen Pull-Up.
> Alle drei benötigen einen externen 10-kΩ-Widerstand nach 3V3.
> GPIO 32 und 33 werden als Relaiskanäle 8 und 7 genutzt und stehen für
> optionale Eingänge nicht mehr zur Verfügung.

---

## Relais-Logik

Standardmäßig `RELAY_ACTIVE_LOW_DEFAULT = true`: Relais schalten bei **LOW**-Signal
(typisch für optoentkoppelte Relaismodule). Falls dein Modul aktiv-HIGH arbeitet,
kannst du den Wert in der Weboberfläche unter **Konfiguration** umschalten.

---

## Anschlussplan

Siehe [schematic.md](schematic.md) für das vollständige Verdrahtungsdiagramm.
