# ESP32 Bewässerungssteuerung

Vollständiger ESP32-basierter Bewässerungscontroller für bis zu 8 Kanäle mit
Weboberfläche, Bewässerungsplänen, Queue-Persistenz und optionalen Hardware-Erweiterungen.

## Dokumentation

Ausführliche Anleitungen im Ordner [`docs/`](docs/):

| Datei | Inhalt |
|-------|--------|
| [hardware.md](docs/hardware.md) | Bauteile, Pinbelegung, Relais-Logik |
| [schematic.md](docs/schematic.md) | Schematischer Anschlussplan |
| [installation.md](docs/installation.md) | Kompilierung und Upload |
| [configuration.md](docs/configuration.md) | Alle Konfigurationsparameter |
| [usage.md](docs/usage.md) | Bedienung, Webinterface, API |
| [architektur.md](docs/architektur.md) | Softwarearchitektur |

---

## Hauptfunktionen

- Steuerung von bis zu 8 Relais-Kanälen
- Manuelle Kanalstarts mit konfigurierbarer Laufzeit
- Bewässerungsgruppen mit mehreren Schritten und sequentieller Ausführung
- **Manuelle Plan-Starts** direkt aus der Weboberfläche (außerhalb Zeitplan)
- Monatliche Einschränkung von Plänen (Vorauswahl Mai–August)
- Queue-Mechanik: Aufgaben werden bei besetztem Kanal gepuffert
- **Queue-Persistenz**: Warteschlange und aktiver Kanal bleiben bei Stromausfall erhalten
- **Automatische DST-Behandlung**: Bewässerungspläne sind von Sommer-/Winterzeit-Umstellung unabhängig
- Winter-Entwässerungsmodus mit Blas-/Pause-/Cooldown-Zyklen
- WLAN-Station + Access Point mit Captive-Portal und QR-Code
- Web-UI mit Seiten: Start, Pläne, Konfiguration, Winter, Logs
- OLED-Display (SH1106 128×64): Datum + Uhrzeit, IP, aktive Kanäle mit verbleibender Zeit
- Lokale Protokollierung im `LittleFS`-Dateisystem (CSV)
- REST-API mit Bearer-Token-Authentifizierung
- MQTT-Datenexport mit Retry-Queue
- NTP-Synchronisation + optionaler DS3231-RTC-Support
- Optionaler Wasserzähler-Impulseingang (Reed/Hall, GPIO 33)
- Optionaler externer NOT-AUS-Taster (GPIO 32, 5 s → Neustart)
- Optionaler externer Helligkeits-/AP-Taster (GPIO 34, ext. Pull-Up erforderlich)

---

## Hardware (Kurzübersicht)

| GPIO | Funktion                      |
|------|-------------------------------|
| 13   | Relais Kanal 1                |
| 14   | Relais Kanal 2                |
| 25   | Relais Kanal 3                |
| 26   | Relais Kanal 4                |
| 16   | Relais Kanal 5                |
| 2    | Relais Kanal 6                |
| 15   | Relais Kanal 7                |
| 3    | Relais Kanal 8                |
| 5    | I²C SDA (OLED + RTC)          |
| 4    | I²C SCL (OLED + RTC)          |
| 0    | BOOT-/AP-Taster (intern)      |
| 34   | Ext. Helligkeits-/AP-Taster * |
| 32   | Ext. NOT-AUS-Taster *         |
| 33   | Wasserzähler-Impuls *         |

`*` = optional, per `#define` aktivierbar — siehe [schematic.md](docs/schematic.md)

### Relais-Logik

Standard: `RELAY_ACTIVE_LOW_DEFAULT = true` (aktiv-LOW, typisch für optoentkoppelte Module).
Einstellbar in der Weboberfläche → Konfiguration.

---

## Konfiguration

Wichtigste Compile-Time-Parameter in `src/main.cpp`:

```cpp
#define EXTERN_BRIGHTNESS_BUTTON 0   // ext. Helligkeitstaster aktivieren
#define EXTERN_ESTP_BUTTON       0   // ext. NOT-AUS-Taster aktivieren
#define WATER_METER_ENABLED      0   // Wasserzähler aktivieren

static const char* TZ_DEFAULT = "CET-1CEST,M3.5.0,M10.5.0/3";  // Zeitzone (DE)
static const char* NTP_SERVER = "pool.ntp.org";
```

Alle weiteren Einstellungen (WLAN, Zeitzone, Kanalnamen, MQTT, Wasserzähler)
sind über die Weboberfläche konfigurierbar und werden im Flash (NVS/LittleFS) gespeichert.

---

## Schnellstart

```bash
# Kompilieren und flashen
platformio run --target upload --upload-port /dev/cu.usbserial-XXXX

# Filesystem flashen (Erstinstallation)
platformio run --target uploadfs
```

Nach dem ersten Start: BOOT-Taste 3 Sekunden halten → AP-Modus →
WLAN `IrrigationController` verbinden → Zugangsdaten konfigurieren.

---

## Genutzte Bibliotheken

- `U8g2` — OLED-Display (SH1106)
- `RTClib` — DS3231 RTC
- `QRCode` (ricmoo) — QR-Code im AP-Modus
- `PubSubClient` — MQTT
- `Preferences` / `LittleFS` — Persistente Konfiguration und Logs (ESP32-Arduino)

---

*Erstellt für das `ESP32_IrrigationController`-Projekt.*
