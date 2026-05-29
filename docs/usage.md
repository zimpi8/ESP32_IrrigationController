# Nutzung

## Erster Start

1. WLAN-Zugangsdaten in `src/main.cpp` eintragen **oder** im AP-Modus konfigurieren.
2. ESP32 kompilieren und flashen (siehe [installation.md](installation.md)).
3. Bei ersten Start: BOOT-Taste 3 Sekunden halten → AP-Modus → mit dem WLAN
   `IrrigationController` verbinden → Weboberfläche öffnet sich automatisch.
4. WLAN-Zugangsdaten eingeben, speichern, Neustart.

---

## Weboberfläche

Nach dem Start im WLAN-Modus erreichbar unter:
- `http://irrigationcontroller.local` (mDNS, bei den meisten Betriebssystemen)
- IP-Adresse (im OLED-Display angezeigt)

Alle Seiten erfordern Authentifizierung (`WEB_USER` / `WEB_PASS`).

### Startseite

- Übersicht aller 8 Kanäle mit Status, verbleibender Zeit (`hh:mm:ss`) und Buttons
- Aktuelle Queue-Anzeige
- NOT-AUS-Button (stoppt alles + leert Queue)
- Log-Vorschau

### Pläne

- Bewässerungsgruppen anlegen und bearbeiten
- Jede Gruppe hat: Name, Startzeit, Wochentage, Monate (Vorauswahl Mai–Aug), Schritte
- **Jetzt starten**: Gruppe sofort manuell starten, unabhängig von Uhrzeit/Tag/Monat
- Bestehende Gruppen löschen

### Konfiguration

- Kanalnamen (bis zu 20 Zeichen, im Display bis zu 12 Zeichen)
- Relais-Logik (aktiv LOW / aktiv HIGH)
- WLAN-Zugangsdaten (nur im AP-Modus)
- Zeitzone (POSIX-TZ-String)
- Wasserzähler: Impulse pro Liter einstellen, Zählerstand zurücksetzen
- MQTT-Verbindungsdaten

### Wintermodus

Geführter Entwässerungsmodus für alle Kanäle. Konfigurierbare Blas-/Pause-/
Cooldown-Zyklen. Start über die Wintermodus-Seite.

### Logs

- Anzeige und Download der Bewässerungs-Ereignisse als CSV
- Logs löschen

---

## OLED-Display

Das Display zeigt im Normalbetrieb:

1. **Datum und Uhrzeit** — Format: `DD.MM.YYYY - HH:MM:SS`
2. **IP-Adresse** (oder `AP-Modus` / `Verbinde...`)
3. **Aktive Kanäle** — Kanalname (bis 12 Zeichen) links, verbleibende Zeit `HH:MM:SS` rechts

Bei pausierter Queue nach Stromausfall blinkt eine Hinweismeldung.

### Helligkeit

- **BOOT-Taste** (GPIO 0) kurz drücken: Helligkeit wechseln (3 Stufen) / Display aufwecken
- Ext. Helligkeitstaster (GPIO 34, optional) hat dieselbe Funktion

---

## Bewässerungspläne

### Automatisch

Jede Gruppe wird täglich geprüft. Bedingungen für Auslösung:
- Gruppe ist **aktiv**
- Aktueller **Wochentag** ist in `daysMask` enthalten
- Aktueller **Monat** ist in `monthsMask` enthalten (0 = alle Monate)
- Plan-Uhrzeit liegt im Fenster `(letzter Check, jetzt]`
- Gruppe wurde **heute noch nicht** ausgeführt (`lastRunDay`)

Bewässerungspläne in DST-Umstellungsfenstern werden nicht beeinträchtigt:
Die Kanal-Laufzeit basiert auf `millis()`, nicht auf der Systemuhr.

### Manuell

Auf der Seite **Pläne** → Button **▶ Jetzt starten** unter jedem Plan:
Reiht alle Schritte der Gruppe sofort in die Queue ein (Quelle: `manual`).

---

## Queue-Verhalten

- Neue Aufgaben werden am Ende der Queue eingereiht (`push_back`)
- Im `SINGLE_CHANNEL_MODE` (Standard) läuft immer nur ein Kanal gleichzeitig
- Die Queue wird bei jedem Einreihen im Flash (NVS) gesichert

### Nach Stromausfall

Beim Neustart:
1. Die gespeicherte Queue wird wiederhergestellt
2. Falls ein Kanal aktiv war, wird dessen verbleibende Zeit an den Anfang der Queue gesetzt
3. Queue ist **pausiert** — im Display blinkt ein Hinweis
4. Kurzer Tastendruck (BOOT-Taste oder ext. Taster) → Queue wird fortgesetzt

Mindestrestzeit für Wiederherstellung: 30 Sekunden.

---

## Taster-Übersicht

| Taster            | GPIO | Kurz drücken            | Lang halten (≥ 3 s / 5 s)    |
|-------------------|------|-------------------------|-------------------------------|
| BOOT (intern)     | 0    | Helligkeit / Aufwecken  | 3 s → AP-Konfigurationsmodus  |
| Ext. Helligkeit * | 34   | Helligkeit / Aufwecken  | 3 s → AP-Konfigurationsmodus  |
| Ext. NOT-AUS *    | 32   | —                       | 5 s → Alle stoppen + Neustart |

`*` = optionale Hardware, per `#define` aktivierbar

---

## API (Bearer-Token)

Für Home-Automation-Integration steht eine REST-API zur Verfügung:

| Methode | Pfad                  | Beschreibung                     |
|---------|-----------------------|----------------------------------|
| GET     | `/api/status`         | JSON-Status aller Kanäle + Queue |
| POST    | `/api/start`          | Kanal starten (`ch`, `sec`)      |
| POST    | `/api/stop`           | Kanal stoppen (`ch`)             |
| POST    | `/api/winter`         | Wintermodus starten/stoppen      |

Authentifizierung: `Authorization: Bearer <token>`
Tokens werden in der Weboberfläche unter **Konfiguration** verwaltet.

---

## Lokale Protokollierung

- Log-Datei: `/logs.csv` im `LittleFS`-Dateisystem
- Spalten: `timestamp, channel, name, action, durationSec, source`
- Download: Konfigurationsseite → **Logs** → **Download**

---

## Fehlerbehebung

| Problem                               | Lösung                                                   |
|---------------------------------------|----------------------------------------------------------|
| Keine WLAN-Verbindung                 | SSID / Passwort prüfen, serielle Ausgabe beobachten      |
| OLED leer                             | I²C-Verkabelung prüfen (SDA=GPIO5, SCL=GPIO4, Addr=0x3C) |
| Relais reagieren nicht                | `relayActiveLow` in Konfiguration prüfen                 |
| Nach Boot sofort AP-Modus             | GPIO 34 floating → ext. 10 kΩ Pull-Up anschließen oder `EXTERN_BRIGHTNESS_BUTTON 0` |
| LittleFS-Fehler                       | Flash-Partition prüfen, ggf. vollständig neu flashen     |
| Uhrzeit falsch nach DST-Umstellung    | TZ-String in Konfiguration prüfen (Standard: `CET-1CEST,M3.5.0,M10.5.0/3`) |
