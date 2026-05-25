# Architektur

## Gesamtsystem

Das Projekt besteht aus folgenden Komponenten:

- `ESP32`-Firmware mit Webserver
- OLED-Display für lokale Statusanzeige
- `LittleFS`-Dateisystem für lokale Logs
- `MQTT`-Client für optionale Fernübertragung
- optionaler `DS3231`-RTC für Zeitsicherung

## Softwarekomponenten

### WLAN & Network

- Verbindung als Station (STA) zum bestehenden WLAN
- Alternativ Access Point (AP) mit Captive-Portal
- mDNS zur einfachen lokalen Erreichbarkeit
- DNS-Server für Captive-Portal-Funktionalität

### Web-Oberfläche

- `WebServer` liefert HTML-Seiten und REST-Endpunkte
- Login-Authentifizierung für Konfiguration und Bedienung
- Dynamische Seiten für Status, Pläne, Wintermodus und Logs

### Kanalsteuerung

- Kanaldefinitionen in `CHANNEL_PINS[]`
- Kanäle können manuell gestartet werden
- Laufzeiten und Zeitlimits werden zentral verwaltet
- `SINGLE_CHANNEL_MODE` ermöglicht sequentielle Kanalsteuerung

### Gruppenpläne

- Mehrere Gruppen (`MAX_GROUPS`) mit mehreren Schritten (`MAX_STEPS`)
- Jeder Schritt enthält Kanalnummer und Laufzeit
- Zeitsteuerung basiert auf Uhrzeit und Wochentagen
- Pläne werden nur ausgeführt, wenn sie aktiviert sind

### Warteschlange

- Manuelle Starts werden in eine Queue eingereiht, sobald ein Kanal belegt ist
- Queue-Mechanik verhindert gleichzeitige Kanalkonflikte

### Wintermodus

- Phasen: Blasen, Pause, Cooldown
- Mehrfache Durchläufe über alle definierten Zonen
- Begrenzte Laufzeit und Abkühlzeit zur Schonung der Hardware

### Logging und Persistenz

- Ereignisse werden als CSV im `LittleFS` gespeichert
- Log-Datei: `/logs.csv`
- Einstellungen werden in `Preferences` persistiert

### Zeit & RTC

- Daten von NTP-Servern zur Zeitsynchronisation
- Optionale RTC als Backup, falls NTP nicht erreichbar ist
- Lokale Zeit wird für Plan-Ausführung genutzt

### MQTT (optional)

- `PubSubClient` für MQTT-Verbindungen
- Daten werden an einen MQTT-Broker gesendet
- Retry-Queue verwaltet fehlgeschlagene Übertragungen

## Projektstruktur

- `src/main.cpp` – zentrale Firmware und Geschäftslogik
- `platformio.ini` – Build- und Bibliothekskonfiguration
- `docs/` – Dokumentationsseiten
- `include/` – optionale Header-Dateien
- `lib/` – eigene oder zusätzliche Bibliotheken
- `test/` – Platz für Tests

## Erweiterungsmöglichkeiten

- Sensorbasierte Steuerung (Feuchte, Temperatur)
- Erweiterte Zeitpläne pro Wochentag
- API-Anbindung für Home-Automation-Systeme
- Mehrsprachige Weboberfläche
