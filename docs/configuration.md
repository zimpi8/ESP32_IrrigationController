# Konfiguration

Die wichtigsten Projektparameter stehen in `src/main.cpp`.

---

## WLAN & Access Point

| Parameter              | Beschreibung                                          |
|------------------------|-------------------------------------------------------|
| `WIFI_SSID`            | SSID des Heim-WLANs                                   |
| `WIFI_PASS`            | Passwort des Heim-WLANs                               |
| `AP_SSID`              | SSID des Konfigurations-Access-Points                 |
| `AP_PASS`              | Passwort des Access-Points                            |
| `AP_CONFIG_REQUIRE_AUTH` | Authentifizierung im Captive-Portal ein/aus         |

WLAN-Zugangsdaten können auch im AP-Modus über die Weboberfläche gesetzt werden
(BOOT-Taste 3 Sekunden halten).

---

## Webzugang

| Parameter         | Beschreibung                                |
|-------------------|---------------------------------------------|
| `WEB_USER`        | Benutzername für die Weboberfläche          |
| `WEB_PASS`        | Passwort für die Weboberfläche              |
| `DEVICE_HOSTNAME` | mDNS-Hostname (`<hostname>.local`)          |

---

## Zeit & Zeitzone

| Parameter    | Beschreibung                                                             |
|--------------|--------------------------------------------------------------------------|
| `NTP_SERVER` | NTP-Server für Zeitsynchronisation (Standard: `pool.ntp.org`)            |
| `TZ_DEFAULT` | POSIX-Zeitzonenstring (Standard: `CET-1CEST,M3.5.0,M10.5.0/3` für DE)  |

Die Zeitzone wird über einen POSIX-TZ-String konfiguriert und unterstützt
automatische Sommer-/Winterzeit-Umstellung (DST). Der String kann auch in der
Weboberfläche unter **Konfiguration** geändert werden.

Beispiele für POSIX-TZ-Strings:

| Region              | TZ-String                              |
|---------------------|----------------------------------------|
| Deutschland / EU    | `CET-1CEST,M3.5.0,M10.5.0/3`          |
| UK                  | `GMT0BST,M3.5.0/1,M10.5.0`            |
| US Eastern          | `EST5EDT,M3.2.0,M11.1.0`              |
| UTC                 | `UTC0`                                 |

> Bewässerungspläne, die in das DST-Umstellungsfenster fallen, werden davon
> nicht beeinträchtigt: Die Planausführung basiert auf einem Sliding-Window-
> Mechanismus und `millis()`-basierter Laufzeit.

---

## Laufzeit & Steuerung

| Parameter           | Beschreibung                                           |
|---------------------|--------------------------------------------------------|
| `DEFAULT_RUNTIME_SEC` | Standardlaufzeit pro Kanal (Sekunden)               |
| `MIN_GAP_SEC`       | Mindestpause zwischen zwei Kanalstarts               |
| `MAX_RUNTIME_SEC`   | Maximale Laufzeit pro Kanal                          |
| `SINGLE_CHANNEL_MODE` | Wenn `true`: immer nur ein Kanal gleichzeitig aktiv |

---

## Display

| Parameter     | Beschreibung                                                  |
|---------------|---------------------------------------------------------------|
| `CH_NAME_MAX` | Maximale Länge eines Kanalnamens (Speicher, Standard: 20)     |
| `CH_DISP_MAX` | Maximale Anzahl Zeichen im OLED-Display (Standard: 12)        |

---

## Pläne (Gruppen)

| Parameter        | Beschreibung                                          |
|------------------|-------------------------------------------------------|
| `MAX_GROUPS`     | Maximale Anzahl Bewässerungsgruppen (Standard: 8)     |
| `MAX_STEPS`      | Schritte pro Gruppe (Standard: 8)                     |
| `GROUP_NAME_MAX` | Maximale Länge eines Gruppennamens                    |

---

## Optionale Hardware

### Externer Helligkeits-/AP-Taster (GPIO 34)

```cpp
#define EXTERN_BRIGHTNESS_BUTTON 0   // 0 = deaktiviert (Standard)
```

Kurzer Druck: OLED-Helligkeit wechseln / Display aufwecken  
3 Sekunden halten: AP-Konfigurationsmodus starten

> **Achtung:** GPIO 34 benötigt einen externen 10-kΩ-Pull-Up-Widerstand nach 3V3.
> Floating Pin → AP-Modus beim Booten!

### Externer NOT-AUS-Taster (GPIO 32)

```cpp
#define EXTERN_ESTP_BUTTON 0   // 0 = deaktiviert (Standard)
```

5 Sekunden halten: Alle Kanäle stoppen + ESP32 Neustart.

### Wasserzähler-Impulseingang (GPIO 33)

```cpp
#define WATER_METER_ENABLED 0   // 0 = deaktiviert (Standard)
```

Zählt Impulse über Reed-Kontakt oder Hall-Sensor.
Impulse pro Liter in der Weboberfläche unter **Konfiguration** einstellbar.

---

## Queue-Persistenz nach Stromausfall

Bei Stromausfall speichert das System automatisch:
- Die gesamte Bewässerungswarteschlange (Queue)
- Den aktiven Kanal mit verbleibender Laufzeit

Nach Neustart ist die Queue pausiert und wird erst nach einem Tastendruck
(BOOT-Taste oder ext. Helligkeitstaster) fortgesetzt.
Die minimale Restzeit für eine Wiederherstellung: `MIN_RESTORE_SEC = 30 s`.

---

## Wintermodus

| Parameter              | Beschreibung                                    |
|------------------------|-------------------------------------------------|
| `WINTER_BLOW_SEC`      | Dauer der Ausblasphase pro Kanal (Sekunden)     |
| `WINTER_ZONE_PAUSE_SEC`| Pause zwischen Kanälen                          |
| `WINTER_PASSES`        | Anzahl Durchläufe pro Kanal                     |
| `WINTER_MAX_RUN_SEC`   | Maximale Laufzeit pro Kanal im Wintermodus      |
| `WINTER_COOLDOWN_SEC`  | Abkühlpause zwischen den Durchläufen            |

---

## MQTT

| Parameter              | Beschreibung                                      |
|------------------------|---------------------------------------------------|
| `MQTT_HOST_MAX`        | Maximale Länge des MQTT-Hostnamens                |
| `MQTT_USER_MAX`        | Maximale Länge des MQTT-Benutzernamens            |
| `MQTT_PASS_MAX`        | Maximale Länge des MQTT-Passworts                 |
| `MQTT_PORT_DEFAULT`    | Standardport (1883)                               |
| `MAX_MQTT_RETRY_QUEUE` | Größe der Wiederholungswarteschlange bei Ausfall  |

MQTT-Verbindungsdaten werden in der Weboberfläche unter **Konfiguration** eingestellt.
