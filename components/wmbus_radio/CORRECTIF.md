# Copie corrigée de `wmbus_radio`

Copie du composant `wmbus_radio` de [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components)
(branche `main`, commit `76dda80` du 22/11/2026), avec un seul changement dans
`transceiver_cc1101.cpp`, fonction `CC1101::read_in_task()` :

- **Problème** : quand le programme avait rattrapé la réception (un seul octet dans la FIFO
  du CC1101), il lisait cet octet alors que le suivant arrivait. C'est le cas décrit par
  l'errata TI SWRZ020 : la lecture renvoie alors des données fausses. Sur les trames T1 de
  45 octets, tout ce qui suivait les ~33 premiers octets était corrompu, le décodage
  « 3 parmi 6 » échouait et aucune trame ne passait le contrôle CRC.
- **Correctif** : on laisse toujours au moins un octet dans la FIFO tant que la trame n'est
  pas entièrement reçue, et on relit `RXBYTES` jusqu'à obtenir deux valeurs identiques
  (même errata).

`wmbus_common` et `wmbus_meter` restent chargés depuis le dépôt d'origine.
À retirer quand le correctif sera intégré en amont.
