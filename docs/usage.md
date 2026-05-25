# Nutzung

## Weboberfläche

Nach dem Start kann das Gerät im WLAN oder per Access Point erreicht werden. Die Weboberfläche bietet:

- Startseite mit Systemstatus
- Planverwaltung
- Konfiguration
- Wintermodus
- Logs

## Lokale Protokollierung

- Log-Datei: `/logs.csv`
- Speicherung im `LittleFS`

## Tägliche Nutzung

1. WLAN-Daten in `src/main.cpp` eintragen oder im AP-Modus konfigurieren.
2. ESP32 starten.
3. Weboberfläche öffnen.
4. Bewässerungspläne anlegen oder manuelle Kanäle starten.

## Fehlerbehebung

- WLAN-Verbindung nicht möglich: SSID/Passwort prüfen
- OLED-Anzeige leer: I²C-Verkabelung prüfen
- Relais reagieren nicht: `RELAY_ACTIVE_LOW_DEFAULT` prüfen
