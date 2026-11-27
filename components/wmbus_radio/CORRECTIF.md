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

## Registres d'origine rétablis (27/11/2026, soir)

Après le passage à la longueur fixe, le journal ne montrait que du bruit : une « trame » de 45 octets toutes les secondes environ (RSSI vers -97 dBm), jamais de trame Izar valide. Seuls les registres d'origine (`MDMCFG4/3 = 0x5C/0x04`, `DEVIATN = 0x44`) avaient permis de recevoir de vraies trames (en-tête et CRC du 1er bloc valides) ; avec `0x7B/0xF8/0x50` de la PR IoTLabs, plus aucune n'était détectée. Ces trois valeurs sont donc revenues à l'origine, et seuls IOCFG0 (mot de synchro) et la longueur fixe restent modifiés.

## Débit exact avec la bande d'origine (27/11/2026, nuit)

Avec les registres d'origine et la lecture en longueur fixe, de vraies trames sont enfin reçues : elles commencent par `36 57 1C` (L = 0x19, C = 0x44 codés en 3 parmi 6, vérifié à la main), mais ensuite 1 à 2 bits par octet sont faux. Les erreurs commencent vers le 16e bit. Or le débit d'origine (`0x5C/0x04`) est de 103,2 kbit/s alors que le mode T1 est à 100 kbit/s : l'écart de 3 % décale l'instant d'échantillonnage d'une demi-période au bout de 16 bits. `MDMCFG4/3 = 0x5B/0xF8` donne 99,98 kbit/s en gardant la bande de 325 kHz (c'est la réduction de la bande à 232 kHz, avec la déviation modifiée, qui faisait manquer les trames). Hypothèse, à valider sur le matériel.

## Aucun accès SPI pendant la trame (27/11/2026, nuit)

Les trames reçues (`36 57 1C…` = L 0x19, C 0x44 en 3 parmi 6) n'ont que leurs 3 premiers octets justes, puis des bits aléatoires (la plupart des symboles de 6 bits ne sont plus valides). Toutes les corruptions observées depuis le début s'expliquent par un accès au bus SPI pendant la réception : au 32e octet avec la lecture au seuil de la FIFO d'origine, dès le départ avec la lecture en continu du composant `wmbus` version 3 et avec GDO0 sur le mot de synchro, et après les 3 premiers octets depuis que le RSSI est lu au réveil. Le pilote attend maintenant la durée de la trame (4,4 ms) sans toucher au bus, puis lit le RSSI et la FIFO. Hypothèse, à valider sur le matériel : le trafic SPI parasite le récepteur (fils volants ou alimentation).

## Débit : retour à 0x5C / 0x04 (27/11/2026)

Le passage à `0x5B/0xF8` (PR #22) reposait sur un calcul (écart de 3 % avec 100 kbit/s) et non sur une source. Les configurations CC1101 T1 publiées qui fonctionnent, dont une pour Izar, utilisent toutes `MDMCFG4/3 = 0x5C/0x04` avec le reste identique à ce pilote : `maciekn/izar-wmbus-esp` (`wmbus_t_cc1101_config.h`, « based on TI SWRA234A »), `Albert221/wmbus-cc1101-esphome`, `ryszardlet/esphome-wmbus-cc1101`. Leurs autres registres (FOCCFG `0x2E`, BSCFG `0xBF`, AGCCTRL `0x43/0x09/0xB5`, DEVIATN `0x44`, FSCTRL1 `0x08`) sont les mêmes que les nôtres. Le débit est donc le seul écart avec les références. Le décodage des trames depuis la PR #23 (aucun accès SPI pendant la trame) est conservé.
