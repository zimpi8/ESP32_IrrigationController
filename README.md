# ESP32 Bewässerungssteuerung

## Übersicht

Dieses Projekt ist ein vollständiger `ESP32`-basierter Bewässerungscontroller für bis zu 8 Kanäle. Es enthält Funktionen für manuelle Steuerung, Gruppenpläne, Warteschlangensteuerung, Winterentwässerung, WLAN-Konfiguration, Web-Oberfläche, lokale Protokollierung und optionales MQTT.

## Hauptfunktionen

- Steuerung von bis zu 8 Relais-Kanälen
- Manuelle Kanalstarts mit Laufzeit-Konfiguration
- Gruppenpläne mit mehreren Schritten und sequentieller Ausführung
- Queue-Mechanik: manuelle Starts werden bei besetztem Kanal gespeichert
- Winter-Entwässerungsmodus mit Blas-/Pause-/Cooldown-Zyklen
- WLAN-Station + Access Point mit Captive-Portal und QR-Code
- Web-UI mit Seiten: Start, Pläne, Konfiguration, Winter, Logs
- OLED-Display für Status, Uhrzeit, IP-Adresse und laufende Kanäle
- Lokale Log-Datei im `LittleFS`-Dateisystem (CSV)
- Optionaler MQTT-Datenexport mit Retry-Queue
- NTP-Zeit-Synchronisation und optionaler DS3231-RTC-Support
- Konfigurierbare Geräteeinstellungen direkt im Quellcode

## Hardware

### Benötigte Bauteile

- ESP32-Entwicklungsboard (z. B. `esp32doit-devkit-v1`)
- 8 Relais oder ein Relais-Board
- OLED-Display 128x64 mit I²C
- optional DS3231 RTC-Modul
- optional Taster für OLED-Helligkeit
- Stromversorgung für ESP32 und Relais

### Pinbelegung

Aktuelle Standardbelegung in `src/main.cpp`:

- Kanal 1: GPIO 13
- Kanal 2: GPIO 14
- Kanal 3: GPIO 25
- Kanal 4: GPIO 26
- Kanal 5: GPIO 16
- Kanal 6: GPIO 2
- Kanal 7: GPIO 15
- Kanal 8: GPIO 3

OLED / I²C:

- SDA: GPIO 5
- SCL: GPIO 4
- OLED-Adresse: `0x3C`

Optionaler Helligkeitsbutton:

- GPIO 34 (nur wenn `EXTERN_BRIGHTNESS_BUTTON` aktiviert ist)

### Relais-Logik

Standardmäßig ist `RELAY_ACTIVE_LOW_DEFAULT = true`, also aktive Relais werden mit LOW geschaltet. Passe diese Einstellung an, falls dein Relaismodul anders arbeitet.

## Konfiguration

Die wichtigsten Einstellungen befinden sich in `src/main.cpp`.

### WLAN und Access Point

- `WIFI_SSID`, `WIFI_PASS`: WLAN-Zugangsdaten für den Station-Modus
- `AP_SSID`, `AP_PASS`: SSID und Passwort für den Access Point
- `AP_CONFIG_REQUIRE_AUTH`: Authentifizierung im Captive-Portal ein/aus

### Webzugang

- `WEB_USER`, `WEB_PASS`: Zugangsdaten für die Web-Oberfläche
- `DEVICE_HOSTNAME`: Hostname für mDNS und Netzwerk

### Zeit und RTC

- `NTP_SERVER`: NTP-Server für Zeitsynchronisation
- `GMT_OFFSET_SEC`, `DAYLIGHT_OFFSET_SEC`: Zeitzone und Sommerzeit
- DS3231-RTC wird optional unterstützt, falls ein Modul angeschlossen ist

### Laufzeit und Steuerung

- `DEFAULT_RUNTIME_SEC`: Standardlaufzeit pro Kanal in Sekunden
- `MIN_GAP_SEC`: Mindestzeit zwischen Kanalstarts
- `MAX_RUNTIME_SEC`: Maximale Laufzeit pro Kanal
- `SINGLE_CHANNEL_MODE`: Wenn aktiviert, läuft immer nur ein Kanal zurzeit

### Gruppenpläne

- `MAX_GROUPS`: Maximal mögliche Plan-Gruppen
- `MAX_STEPS`: Schritte pro Gruppe
- `CH_NAME_MAX`, `GROUP_NAME_MAX`: Längenbegrenzung für Kanal- und Gruppennamen

### Wintermodus

- `WINTER_BLOW_SEC`: Dauer der Ausblasphase
- `WINTER_ZONE_PAUSE_SEC`: Pause zwischen Kanälen
- `WINTER_PASSES`: Anzahl Durchläufe
- `WINTER_MAX_RUN_SEC`: Max. Laufzeit pro Kanal im Wintermodus
- `WINTER_COOLDOWN_SEC`: Abkühlphase zwischen Durchläufen

### MQTT

- `MQTT_HOST_MAX`, `MQTT_USER_MAX`, `MQTT_PASS_MAX`: Größenbegrenzung der MQTT-Daten
- `MQTT_PORT_DEFAULT`: Standardport für MQTT
- `MAX_MQTT_RETRY_QUEUE`: Größe der Wiederholungswarteschlange

## Softwarearchitektur

### Dateien und Ordner

- `src/main.cpp`: Hauptprogramm und Konfiguration
- `platformio.ini`: PlatformIO-Projektkonfiguration
- `include/`: Zusätzliche Header-Dateien (falls vorhanden)
- `lib/`: Zusätzliche Bibliotheken oder lokale Komponenten
- `test/`: Platz für Tests

### Genutzte Bibliotheken

Im `platformio.ini` sind diese Bibliotheken definiert:

- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `QRCode` von ricmoo
- `PubSubClient`
- `RTClib`

## Kompilierung und Upload

### Voraussetzungen

- PlatformIO installiert
- ESP32-Toolchain verfügbar
- Serial-Port des ESP32 identifiziert

### Build

```bash
platformio run
```

### Upload

```bash
platformio run --target upload --upload-port /dev/cu.usbserial-0001
```

Passe ggf. den Portnamen an dein System an.

## Nutzung

### Weboberfläche

Nach dem Start baut das Gerät zunächst eine WLAN-Verbindung auf. Alternativ wechselt es in den Access-Point-Modus und bietet ein Konfigurationsportal.

Die Weboberfläche enthält:

- Startseite mit Statusinformationen
- Planverwaltung mit Gruppen und Zeitschaltungen
- Konfiguration für WLAN, MQTT und Systemparameter
- Wintermodus-Steuerung
- Protokollanzeige der letzten Bewässerungsereignisse

### Lokale Protokollierung

- Log-Datei: `/logs.csv`
- Nutzt `LittleFS` zum Speichern von Ereignissen
- Eignet sich für Debugging und Auswertung

## Anpassung

### Netzwerkdaten ändern

Passe `WIFI_SSID`, `WIFI_PASS`, `AP_SSID`, `AP_PASS`, `WEB_USER` und `WEB_PASS` direkt in `src/main.cpp` an.

### Relais-Pins ändern

Ändere das Array `CHANNEL_PINS[]`, um andere GPIOs zu verwenden.

### Geräte-Hostname

Ändere `DEVICE_HOSTNAME`, damit das Gerät im lokalen Netzwerk ein anderen Namen erhält.

### MQTT aktivieren

MQTT kann über die Weboberfläche ein- und ausgeschaltet werden. Trage Host, Benutzername, Passwort und Topic ein.

## Fehlerbehebung

- Wenn das Gerät keine WLAN-Verbindung herstellt, prüfe SSID/Passwort und Serielle Ausgabe.
- Bei Problemen mit dem OLED-Display, kontrolliere I²C-Verkabelung und die Adresse `0x3C`.
- Bei fehlerhaften Relais, überprüfe, ob `RELAY_ACTIVE_LOW_DEFAULT` richtig gesetzt ist.
- Wenn `LittleFS` nicht funktioniert, achte darauf, dass der ESP32 den Flash korrekt formatiert hat.

## Weiterentwicklung

Dieses Projekt ist als Basis für individuelle Bewässerungssteuerungen ausgelegt. Erweiterungen können umfassen:

- Sensorbasierte Bodenfeuchte-Steuerung
- Temperatur- oder Regenüberwachung
- Zeitpläne pro Wochentag
- API für Home Automation

---

*Erstellt für das `ESP32_IrrigationController`-Projekt.*
