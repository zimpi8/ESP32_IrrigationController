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
                    │  GPIO 13 ──────────────────────────────────┼──────────► Relais K1
                    │  GPIO 14 ──────────────────────────────────┼──────────► Relais K2
                    │  GPIO 25 ──────────────────────────────────┼──────────► Relais K3
                    │  GPIO 26 ──────────────────────────────────┼──────────► Relais K4
                    │  GPIO 16 ──────────────────────────────────┼──────────► Relais K5
                    │  GPIO 2  ──────────────────────────────────┼──────────► Relais K6
                    │  GPIO 15 ──────────────────────────────────┼──────────► Relais K7
                    │  GPIO 3  ──────────────────────────────────┼──────────► Relais K8
                    │                                            │
                    │  GPIO 0  ──[BOOT-Taster]─── GND           │  AP-Modus / Display-Helligkeit
                    │                                            │
                    │  GPIO 34 ──[Taster]────────── GND         │  * Ext. Helligkeit / AP-Modus
                    │            └──[10 kΩ]──────── 3V3         │    (EXTERN_BRIGHTNESS_BUTTON)
                    │                                            │
                    │  GPIO 32 ──[Taster]────────── GND         │  * Ext. NOT-AUS
                    │                                            │    (EXTERN_ESTP_BUTTON)
                    │                                            │
                    │  GPIO 33 ──[Reed / Hall]────── GND        │  * Wasserzähler-Impuls
                    │                                            │    (WATER_METER_ENABLED)
                    └────────────────────────────────────────────┘

* = optionale Erweiterungen, per #define aktivierbar
```

---

## Pinbelegung (Tabelle)

| GPIO | Funktion                        | Richtung | Anmerkung                                   |
|------|---------------------------------|----------|---------------------------------------------|
| 13   | Relais Kanal 1                  | Ausgang  | Aktiv LOW (Standard)                        |
| 14   | Relais Kanal 2                  | Ausgang  |                                             |
| 25   | Relais Kanal 3                  | Ausgang  |                                             |
| 26   | Relais Kanal 4                  | Ausgang  |                                             |
| 16   | Relais Kanal 5                  | Ausgang  |                                             |
| 2    | Relais Kanal 6                  | Ausgang  | Eingebaute LED blinkt beim Booten           |
| 15   | Relais Kanal 7                  | Ausgang  |                                             |
| 3    | Relais Kanal 8                  | Ausgang  | Auch UART RX0 – nicht gleichzeitig nutzen   |
| 5    | I²C SDA                         | I/O      | OLED + DS3231 RTC                           |
| 4    | I²C SCL                         | I/O      | OLED + DS3231 RTC                           |
| 0    | BOOT-/AP-Taster                 | Eingang  | Interner Pull-Up, kurz = Helligkeit, 3 s = AP-Modus |
| 34   | Ext. Helligkeits-/AP-Taster *   | Eingang  | **Input-only**, kein int. Pull-Up → ext. 10 kΩ nach 3V3 erforderlich |
| 32   | Ext. NOT-AUS-Taster *           | Eingang  | Interner Pull-Up nutzbar, 5 s halten → Neustart |
| 33   | Wasserzähler-Impuls *           | Eingang  | Interner Pull-Up, Reed-Kontakt oder Hall-Sensor |

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
GPIO 13 ────────────────── IN1
GPIO 14 ────────────────── IN2
GPIO 25 ────────────────── IN3
GPIO 26 ────────────────── IN4
GPIO 16 ────────────────── IN5
GPIO 2  ────────────────── IN6
GPIO 15 ────────────────── IN7
GPIO 3  ────────────────── IN8
5V / 3V3 ───────────────── VCC  (je nach Modul)
GND      ───────────────── GND
```

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

### Externer NOT-AUS-Taster (GPIO 32)

```
GPIO 32 ──[Taster]── GND
```

> Interner Pull-Up wird per `INPUT_PULLUP` aktiviert.
> 5 Sekunden halten → ESP32 Neustart.
> Im Code aktivieren: `#define EXTERN_ESTP_BUTTON 1`

### Wasserzähler-Impuls (GPIO 33)

```
GPIO 33 ──[Reed-Kontakt oder Hall-Sensor]── GND
```

> Interner Pull-Up aktiv. Jeder Schließkontakt zählt als ein Impuls.
> Impulse pro Liter in der Weboberfläche konfigurierbar.
> Im Code aktivieren: `#define WATER_METER_ENABLED 1`
