# Konfiguration

Die wichtigsten Projektparameter stehen in `src/main.cpp`.

## WLAN & Access Point

- `WIFI_SSID`, `WIFI_PASS` – WLAN-Zugangsdaten
- `AP_SSID`, `AP_PASS` – Access-Point-Daten
- `AP_CONFIG_REQUIRE_AUTH` – erfordert Authentifizierung im Captive-Portal

## Webzugang

- `WEB_USER`, `WEB_PASS` – Login für die Weboberfläche
- `DEVICE_HOSTNAME` – Hostname für mDNS und Netzwerk

## Zeit

- `NTP_SERVER` – NTP-Server
- `GMT_OFFSET_SEC` und `DAYLIGHT_OFFSET_SEC` – Zeitzone und Sommerzeit

## Laufzeit & Steuerung

- `DEFAULT_RUNTIME_SEC` – Standardlaufzeit
- `MIN_GAP_SEC` – minimale Pause zwischen Kanalstarts
- `MAX_RUNTIME_SEC` – Maximale Dauer
- `SINGLE_CHANNEL_MODE` – nur ein Kanal gleichzeitig

## Wintermodus

- `WINTER_BLOW_SEC`, `WINTER_ZONE_PAUSE_SEC`, `WINTER_PASSES`, `WINTER_MAX_RUN_SEC`, `WINTER_COOLDOWN_SEC`

## MQTT

- `MQTT_HOST_MAX`, `MQTT_USER_MAX`, `MQTT_PASS_MAX`
- `MQTT_PORT_DEFAULT`
- `MAX_MQTT_RETRY_QUEUE`
