# Composant wmbus (version 4) : copie corrigée

Copie de `components/wmbus` de [SzczepanLeon/esphome-components](https://github.com/SzczepanLeon/esphome-components), branche `version_4` (commit `5f8090d`). Seuls les drivers `izar` et `unknown` sont gardés.

## Pourquoi la v4 et pas la v5

Avec la v5 (`wmbus_radio`), le CC1101 de ce montage recevait les trames Izar avec un en-tête correct puis du bruit à partir du 32e octet, et aucune ne passait le CRC, même après plusieurs correctifs. D'autres utilisateurs du CC1101 en v5 rapportent le même problème ([forum ArturHome](https://forum.arturhome.pl/t/komponent-wm-bus-do-esphome-wersja-5-x-watek-ogolny/15057?page=20), [issue #425](https://github.com/SzczepanLeon/esphome-components/issues/425)). La v4 lit le CC1101 autrement (broche GDO2 pour la détection de trame) et reste la version utilisée avec ce module.

## Correctifs

1. **ESPHome 2026.11** a retiré `format_hex_pretty()` de ses fonctions utilitaires : `mbus.h` inclut maintenant `utils.h`, qui en fournit une version locale (même erreur que [issue #478](https://github.com/SzczepanLeon/esphome-components/issues/478)).
2. **Drivers non enregistrés** : avec les ESPHome récents, l'enregistrement statique d'un driver peut être éliminé à l'édition de liens (« Selected driver … doesn't exist », [issue #466](https://github.com/SzczepanLeon/esphome-components/issues/466)). `wmbus.cpp` référence un symbole de chaque driver et affiche au démarrage `Drivers lies : izar=1 unknown=1`.
3. `types.h` inclut `<cstdint>` (utilisé sans être inclus).

Vérifications faites : `esphome config` (ESPHome 2026.11.1) et analyse syntaxique des fichiers indépendants d'Arduino avec les en-têtes d'ESPHome 2026.11.1. La compilation complète n'a pas pu être lancée hors du matériel.
