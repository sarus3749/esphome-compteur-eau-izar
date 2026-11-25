# Compteur d'eau Diehl IZAR → Home Assistant (ESPHome)

Lecture radio de l'index d'un compteur d'eau **Diehl Altair + module IZAR RC** (wM-Bus T1, 868,95 MHz, protocole PRIOS) avec un **ESP32 D1 mini** et un module **CC1101 868 MHz**, via le composant ESPHome [`SzczepanLeon/esphome-components`](https://github.com/SzczepanLeon/esphome-components) (driver `izar` de wmbusmeters). L'index remonte dans Home Assistant en m³, prêt pour le tableau de bord Eau.

## Schéma de câblage

![Schéma de câblage D1 mini ↔ CC1101](docs/schema-cablage-d1mini-cc1101.svg)

(version PNG : [`docs/schema-cablage-d1mini-cc1101.png`](docs/schema-cablage-d1mini-cc1101.png))

| CC1101 | D1 mini | GPIO ESP32 (sérigraphie Diymore) | GPIO ESP8266 | Rôle |
|---|---|---|---|---|
| VCC | 3V3 | 3V3 | 3V3 | alimentation **3,3 V uniquement** |
| GND | G | GND | GND | masse |
| SCK | D5 | IO18 | GPIO14 | horloge SPI |
| MISO (SO/GDO1) | D6 | IO19 | GPIO12 | données CC1101 → ESP |
| MOSI (SI) | D7 | IO23 | GPIO13 | données ESP → CC1101 |
| CSN | D4 | IO16 | GPIO2 | chip select |
| GDO0 | D1 | IO22 | GPIO5 | interruption (`irq_pin`) |
| GDO2 | D2 | IO21 | GPIO4 | inutilisé en v5 (ESP32), requis en v4 (ESP8266) |

**Diymore ESP32 Mini** (ESP32-WROOM-32, USB CH9102F) : la carte a deux rangées de broches de chaque côté et la sérigraphie indique les numéros `IOxx`, pas `D1`…`D8`. Repérez les broches par la colonne « GPIO ESP32 » ci-dessus. Sous Windows, installez le pilote CH9102 (CH343) si le port USB n'apparaît pas.

L'ordre des broches du CC1101 varie selon les modules : suivez la sérigraphie du vôtre. Fils courts, antenne 868 MHz verticale et éloignée du métal.

## Fichiers

| Fichier | Usage |
|---|---|
| [`compteur-eau-esp32.yaml`](compteur-eau-esp32.yaml) | **Recommandé** : D1 mini ESP32, composant v5 (branche `main`) |
| [`compteur-eau-esp8266.yaml`](compteur-eau-esp8266.yaml) | Variante D1 mini ESP8266, composant v4 (branche `version_4`) |
| [`secrets.example.yaml`](secrets.example.yaml) | Modèle des secrets à ajouter dans ESPHome |
| [`docs/`](docs/) | Schéma de câblage (SVG + PNG) |

## Quelle carte ?

La version actuelle du composant de SzczepanLeon (v5, branche `main`, vérifiée le 25/11/2026) ne fonctionne **que sur ESP32 avec esp-idf** : le support Arduino/ESP8266 a été abandonné. Un D1 mini ESP8266 reste possible avec l'ancienne v4 (branche `version_4`), mais elle n'évolue plus et sa syntaxe est différente. Le brochage physique est identique pour les deux : on peut donc souder le CC1101 une fois et choisir la carte ensuite.

Attention à prendre un CC1101 **868 MHz** (les plus courants sur les sites chinois sont en 433 MHz).

## Mise en route

1. Câbler selon le schéma (3,3 V uniquement).
2. Dans ESPHome (module complémentaire HA), créer l'appareil, coller le YAML, ajouter dans `secrets.yaml` (modèle : `secrets.example.yaml`) : `wifi_ssid`, `wifi_password`, `ap_password`, `ota_password`, `api_encryption_key`.
3. Flasher en USB la première fois, puis ouvrir les logs.
4. **Vérifier l'ID du compteur.** Le YAML part de `0x13903750` (le TxS/N du module), mais l'adresse radio réellement émise peut différer. Dans les logs :
   - si le compteur est trouvé : ligne `Telegramme du compteur d'eau decode` et l'index s'affiche ;
   - sinon, la v5 affiche `Telegram not handled by any handler` puis `Check if telegram with address XXXXXXXX can be parsed on: …`. Repérer l'adresse qui revient toutes les 8 à 32 s avec un bon RSSI (et le constructeur DME/SAP/HYD), la mettre dans `meter_id: "0xXXXXXXXX"` et reflasher (OTA).
   - En v4 (ESP8266), `log_all: True` affiche directement `izar [0xXXXXXXXX] RSSI: …`.
5. Une fois stable, on peut retirer le bloc `on_frame` (v5) ou passer `log_all: False` (v4).

## Tableau de bord Eau

Le capteur **Index eau** (`total_m3`) est en m³, `device_class: water`, `state_class: total_increasing` : il apparaît directement dans *Paramètres → Tableaux de bord → Énergie → Consommation d'eau → Ajouter une source*. Les statistiques se remplissent à partir de l'heure suivante.

Un filtre (`delta` 0,5 L ou `heartbeat` 5 min) limite les écritures en base, car le module émet très souvent.

## Clé AES

Les modules Diehl IZAR en mode PRIOS sont « brouillés » avec une clé que le driver `izar` connaît déjà (clés par défaut Diehl) : **dans la grande majorité des cas, aucune clé n'est nécessaire**, c'est pourquoi `key:` est commenté.

Si les logs affichent `(izar) Decoding PRIOS data failed. Ignoring telegram.`, le distributeur (Veolia, Suez, Saur, régie…) a configuré une clé spécifique. Il faut alors la demander au distributeur (16 octets en hexadécimal, 32 caractères) et la placer dans `key:`. Beaucoup refusent, mais la demande peut s'appuyer sur votre droit d'accès à vos données de consommation.

## Vérifications faites

- Syntaxe vérifiée sur le dépôt `SzczepanLeon/esphome-components` (commit `76dda80` du 22/11/2026, v5) et branche `version_4`.
- Les deux YAML passent `esphome config` (ESPHome 2026.11.1).
- Compilation complète non encore testée au moment de la publication.
