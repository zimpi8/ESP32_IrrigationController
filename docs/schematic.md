# Schematischer Anschlussplan

## Blockdiagramm

```
                    ┌────────────────────────────────────────────┐
                    │             ESP32 DevKit V1                │
                    │                                            │
          3V3 ──────┤ 3V3                                        │
          GND ──────┤ GND                                        │
                    │                                            │
                    │  GPIO 5 (SDA) ──────────────────────────── ┼──── SDA ──► OLED SH1106
                    │  GPIO 4 (SCL) ──────────────────────────── ┼──── SCL ──► (I²C-Bus)
                    │                                            │            ► DS3231 RTC
                    │                                            │
                    │  GPIO 13 (D13, oben) ──────────────────────┼──────────► Relais K1
                    │  GPIO 17 (TX2, unten)──────────────────────┼──────────► Relais K2
                    │  GPIO 14 (D14, oben) ──────────────────────┼──────────► Relais K3
                    │  GPIO 27 (D27, oben) ──────────────────────┼──────────► Relais K4
                    │  GPIO 26 (D26, oben) ──────────────────────┼──────────► Relais K5
                    │  GPIO 25 (D25, oben) ──────────────────────┼──────────► Relais K6
                    │  GPIO 33 (D33, oben) ──────────────────────┼──────────► Relais K7
                    │  GPIO 32 (D32, oben) ──────────────────────┼──────────► Relais K8
                    │                                            │
                    │  GPIO 0  ──[BOOT-Taster]─── GND           │  AP-Modus / Display-Helligkeit
                    │                                            │
                    │  GPIO 34 ──[Taster]────────── GND         │  * Ext. Helligkeit / AP-Modus
                    │            └──[10 kΩ]──────── 3V3         │    (EXTERN_BRIGHTNESS_BUTTON)
                    │                                            │
                    │  GPIO 35 ──[Taster]────────── GND         │  * Ext. NOT-AUS
                    │            └──[10 kΩ]──────── 3V3         │    (EXTERN_ESTP_BUTTON)
                    │                                            │
                    │  GPIO 36 ──[Reed / Hall]────── GND        │  * Wasserzähler-Impuls
                    │  (VP)      └──[10 kΩ]──────── 3V3         │    (WATER_METER_ENABLED)
                    └────────────────────────────────────────────┘

* = optionale Erweiterungen, per #define aktivierbar
```

---

## Pinbelegung (Tabelle)

| GPIO | Boardpin | Funktion                        | Richtung | Anmerkung                                            |
|------|----------|---------------------------------|----------|------------------------------------------------------|
| 13   | D13      | Relais Kanal 1                  | Ausgang  | Aktiv LOW (Standard), obere Reihe                    |
| 17   | TX2      | Relais Kanal 2                  | Ausgang  | Untere Reihe (GPIO12 übersprungen – Strapping-Pin)   |
| 14   | D14      | Relais Kanal 3                  | Ausgang  | Obere Reihe                                          |
| 27   | D27      | Relais Kanal 4                  | Ausgang  | Obere Reihe                                          |
| 26   | D26      | Relais Kanal 5                  | Ausgang  | Obere Reihe                                          |
| 25   | D25      | Relais Kanal 6                  | Ausgang  | Obere Reihe                                          |
| 33   | D33      | Relais Kanal 7                  | Ausgang  | Obere Reihe                                          |
| 32   | D32      | Relais Kanal 8                  | Ausgang  | Obere Reihe                                          |
| 5    | D5       | I²C SDA                         | I/O      | OLED + DS3231 RTC                                    |
| 4    | D4       | I²C SCL                         | I/O      | OLED + DS3231 RTC                                    |
| 0    | BOOT     | BOOT-/AP-Taster                 | Eingang  | Interner Pull-Up, kurz = Helligkeit, 3 s = AP-Modus |
| 34   | D34      | Ext. Helligkeits-/AP-Taster *   | Eingang  | **Input-only**, ext. 10 kΩ nach 3V3 erforderlich    |
| 35   | D35      | Ext. NOT-AUS-Taster *           | Eingang  | **Input-only**, ext. 10 kΩ nach 3V3, 5 s → Neustart |
| 36   | VP       | Wasserzähler-Impuls *           | Eingang  | **Input-only**, ext. 10 kΩ nach 3V3, Reed/Hall      |

---

## I²C-Bus

Beide I²C-Geräte teilen sich denselben Bus (SDA GPIO 5, SCL GPIO 4):

```
ESP32                OLED SH1106 128×64       DS3231 RTC
GPIO 5 (SDA) ───────┬──── SDA                 SDA
GPIO 4 (SCL) ───────┼──── SCL                 SCL
3V3          ───────┼──── VCC  (max. 3.3 V)   VCC
GND          ───────┴──── GND                 GND

OLED-Adresse: 0x3C
DS3231-Adresse: 0x68 (fest)
```

---

## Relais-Modul (8-Kanal)

```
ESP32                       8-Kanal-Relaismodul
GPIO 13 (D13, oben)  ───── IN1
GPIO 17 (TX2, unten) ───── IN2   ← separate Leitung (untere Reihe)
GPIO 14 (D14, oben)  ───── IN3
GPIO 27 (D27, oben)  ───── IN4
GPIO 26 (D26, oben)  ───── IN5
GPIO 25 (D25, oben)  ───── IN6
GPIO 33 (D33, oben)  ───── IN7
GPIO 32 (D32, oben)  ───── IN8
5V / 3V3 ───────────────── VCC  (je nach Modul)
GND      ───────────────── GND
```

> K1 und K3–K8 (GPIO 13, 14, 27, 26, 25, 33, 32) liegen auf der oberen Pinreihe
> und können über ein 7-adriges Flachbandkabel verbunden werden. GPIO 12 zwischen
> D13 und D14 bleibt unverkabelt (Strapping-Pin). K2 (GPIO 17/TX2) benötigt eine
> separate Leitung auf der unteren Reihe.

> Standardmäßig ist `RELAY_ACTIVE_LOW_DEFAULT = true`. Die Relais schalten bei LOW-Signal.
> Aktiv-HIGH-Module: `relayActiveLow` in der Weboberfläche → Konfiguration umschalten.

---

## Optionale Erweiterungen

### Externer Helligkeits-/AP-Taster (GPIO 34)

```
3V3 ──[10 kΩ]──┬── GPIO 34
               │
            [Taster]
               │
              GND
```

> GPIO 34 ist **Input-only** und hat keinen internen Pull-Up.
> Der externe 10-kΩ-Widerstand ist **zwingend erforderlich** — ohne ihn löst der
> floating Pin beim Booten AP-Modus aus.
> Im Code aktivieren: `#define EXTERN_BRIGHTNESS_BUTTON 1`

### Externer NOT-AUS-Taster (GPIO 35 / D35)

```
3V3 ──[10 kΩ]──┬── GPIO 35
               │
            [Taster]
               │
              GND
```

> GPIO 35 ist **Input-only** ohne internen Pull-Up → ext. 10-kΩ-Widerstand nach 3V3 erforderlich.
> 5 Sekunden halten → ESP32 Neustart.
> GPIO 32 ist als Relaiskanal 8 belegt und steht nicht mehr zur Verfügung.
> Im Code aktivieren: `#define EXTERN_ESTP_BUTTON 1`

### Wasserzähler-Impuls (GPIO 36 / VP)

```
3V3 ──[10 kΩ]──┬── GPIO 36 (VP)
               │
        [Reed / Hall-Sensor]
               │
              GND
```

> GPIO 36 (VP) ist **Input-only** ohne internen Pull-Up → ext. 10-kΩ-Widerstand nach 3V3 erforderlich.
> GPIO 33 ist als Relaiskanal 7 belegt und steht nicht mehr zur Verfügung.
> Impulse pro Liter in der Weboberfläche konfigurierbar.
> Im Code aktivieren: `#define WATER_METER_ENABLED 1`
