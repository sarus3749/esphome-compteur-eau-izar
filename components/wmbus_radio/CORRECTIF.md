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

## Longueur fixe (27/11/2026)

Les corrections de registres ci-dessus ne suffisaient pas : en mode paquet infini, le CC1101 continue d'écrire du bruit dans sa FIFO après la fin de la trame, et lire la FIFO pendant la réception (seuil, `RXBYTES`, lecture en bloc) corrompait les octets de la fin.

Une trame T1 de format A d'un compteur Izar (`L = 0x19`) fait 30 octets, soit 45 octets codés en 3 parmi 6. Le pilote règle donc `PKTCTRL0 = 0x00` (longueur fixe) et `PKTLEN = 45` :

- le CC1101 reçoit 45 octets après le mot de synchro puis repasse seul à IDLE (`MCSM1 = 0x00`) ;
- `read_in_task` lit le RSSI au réveil, attend l'état IDLE (20 ms au plus), puis lit toute la FIFO en une fois au repos ;
- les octets sont ensuite rendus au composant par morceaux (en-tête de 3 octets, puis le reste).

Limite : seules les trames de 45 octets codés sont reçues en entier. Les autres modèles de compteurs, de longueur différente, sont rejetés (message `Trame trop courte`).
