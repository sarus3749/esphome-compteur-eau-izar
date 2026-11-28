# Pilote wmbus_radio (CC1101)

Copie de `components/wmbus_radio` de [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components) (v5), modifiée dans `transceiver_cc1101.cpp` pour recevoir les trames T1 des compteurs Diehl IZAR, plus un bouton Home Assistant de réinitialisation de la radio (`button.py`, `reset_button.h`).

## Problème d'origine

Avec le pilote d'origine, les trames T1 Izar arrivaient avec un en-tête valide puis devenaient du bruit. Plusieurs causes se cumulaient.

## Ce que fait le pilote

- **Longueur fixe** : une trame Izar (`L = 0x19`) fait 30 octets, soit 45 octets codés en 3 parmi 6. `PKTCTRL0 = 0x00` et `PKTLEN = 45` : le CC1101 reçoit 45 octets après le mot de synchro puis repasse seul à IDLE (`MCSM1 = 0x00`). Seules les trames de 45 octets codés sont reçues en entier.
- **Aucun accès SPI pendant la trame** : le pilote attend la durée de la trame (4,4 ms), puis lit le RSSI et la FIFO au repos.
- **Lecture de la FIFO octet par octet** (accès simple, `0xBF`). Une lecture en rafale de 45 octets répétait des octets en fin de trame (`34 34 34…`) : c'était la cause du décodage raté.
- **GDO0 sur le mot de synchro** (`IOCFG0 = 0x46`) : la broche ne bascule plus au seuil de la FIFO en pleine réception.
- **Débit** `MDMCFG4/3 = 0x5B/0xF8` (99,98 kbit/s), bande de 325 kHz, déviation `0x44`, `AGCCTRL0 = 0xB5`. Les valeurs `0x5C/0x04` (projets de référence) ne détectaient presque rien sur ce montage, et `0x7B/0xF8/0x50` (bande 232 kHz) non plus.
- **Réinitialisation** : reset logiciel (SRES) puis configuration complète, au démarrage, en secours si la puce ne répond plus (MARCSTATE `0x00` ou `0x1F`), et à la demande avec le bouton « Réinitialiser la radio ».

`FOCCFG = 0x0E` n'a pas été prouvé utile (valeur des projets de référence : `0x2E`).
