# Correctif du pilote wmbus_radio (CC1101)

Copie de `components/wmbus_radio` de [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components) (v5, commit `76dda80`), modifiée uniquement dans `transceiver_cc1101.cpp` (et un log dans `packet.cpp`).

## Problème

Avec le pilote d'origine, les trames T1 des compteurs Diehl IZAR (45 octets codés) arrivaient avec un en-tête valide, puis devenaient du bruit à partir du 32e octet environ. C'est le moment où GDO0, configurée sur le seuil de la FIFO (32 octets), basculait en pleine réception.

## Modifications

| Registre | Origine | Ici | Effet |
|---|---|---|---|
| IOCFG0 | `0x40` (seuil FIFO, actif bas) | `0x46` (mot de synchro, actif bas) | GDO0 bascule au début de la trame et ne bouge plus pendant la réception |
| MDMCFG4 / MDMCFG3 | `0x5C` / `0x04` (325 kHz, 103,2 kbit/s) | `0x7B` / `0xF8` (232 kHz, 100,0 kbit/s) | débit exact du mode T1 |
| DEVIATN | `0x44` (38 kHz) | `0x50` (50,8 kHz) | déviation nominale du mode T1 |

Valeurs reprises du nouveau pilote CC1101 d'[IoTLabs-pl/ESPHome-Components](https://github.com/IoTLabs-pl/ESPHome-Components) (PR #107).
