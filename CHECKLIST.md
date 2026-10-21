# ✅ Checklist — Station météo Worldwide Weather Watcher

> Ce qui est demandé dans le PDF *« Modes de fonctionnement »*, ce qui est fait, et ce qu'il reste.
>
> **✅ fait et testé** · **🧪 fait, à tester par vous** · **❌ pas encore fait**

---

## 📊 Où on en est

```
Capteurs et matériel   ██████████  fini
LED d'état             ██████████  fini
Mode standard          ██████████  fini
Mode configuration     ██████████  fini
Mode maintenance       ██████████  fini
Mode économique        ██████████  fini
Carte SD               █████████░  fini ; reste à tester la carte pleine et le retrait réel
Livrables              ░░░░░░░░░░  à faire
```

---

## 🔌 1. Le matériel

- ✅ **Température, humidité, pression** : capteur BME280, sur un port **I2C**
- ✅ **Horloge (RTC)** : sur un port **I2C**, à l'heure (vérifié : 1 s d'écart avec le PC)
- ✅ **GPS** : sur le port **A0**
- ✅ **Luminosité** : sur le port **A2**
- ✅ **LED** : sur le port **D2**
- ✅ **Boutons rouge et vert** : sur le port **D7**
- ✅ **Carte SD** : fonctionne (FAT32, 8 Go). Les mesures y sont enregistrées.

---

## 🔁 2. Les 4 modes et les boutons

**Le bouton vert entre et sort de l'économique. Le bouton rouge entre et sort de la maintenance.**

```
🟢 STANDARD   ──vert 5 s──►  🔵 ÉCONOMIQUE ──vert 5 s──►  🟢 STANDARD

🟢 STANDARD   ──rouge 5 s─►  🟠 MAINTENANCE ──rouge 5 s─► 🟢 STANDARD
🔵 ÉCONOMIQUE ──rouge 5 s─►  🟠 MAINTENANCE ──rouge 5 s─► 🔵 ÉCONOMIQUE
                                             (retour au mode d'avant)

Démarrage avec le bouton rouge appuyé ──► 🟡 CONFIGURATION
                                          retour en standard après 30 min sans activité
```

- ✅ Démarrage normal → mode **standard**
- ✅ Démarrage avec le bouton rouge appuyé → mode **configuration**
- ✅ Vert 5 s en standard → mode **économique**
- 🧪 Vert 5 s en économique → retour en **standard**
- ✅ Rouge 5 s en standard → mode **maintenance**
- 🧪 Rouge 5 s en économique → mode **maintenance**
- ✅ Rouge 5 s en maintenance → retour au **mode d'avant** (standard ou économique)
- ✅ Configuration → retour en standard après **30 min sans activité** (le délai repart à chaque commande ou appui)
- 🧪 Ce retour automatique après 30 min n'a pas encore été testé en vrai (trop long à attendre)

> ⚠️ **À expliquer dans le rapport :** le PDF demande deux choses pour le même geste en mode économique (rouge 5 s) : aller en maintenance, et revenir en standard. Nous avons gardé le rouge pour la maintenance, comme dans les autres modes, et utilisé le **vert** pour revenir en standard. Chaque bouton a ainsi un seul rôle, facile à retenir.

---

## 💡 3. La LED

| La LED fait… | Ça veut dire… | |
|---|---|---|
| 🟢 vert fixe | mode standard | ✅ |
| 🟡 jaune fixe | mode configuration | ✅ |
| 🔵 bleu fixe | mode économique | ✅ |
| 🟠 orange fixe | mode maintenance | ✅ |
| 🔴🔵 rouge / bleu | l'horloge (RTC) ne répond pas | 🧪 débrancher la RTC pour tester |
| 🔴🟡 rouge / jaune | le GPS ne répond pas | ✅ |
| 🔴🟢 rouge / vert | un capteur ne répond pas | ✅ luminosité · 🧪 débrancher le BME280 pour tester |
| 🔴🟢 rouge / vert *(vert 2× plus long)* | une valeur mesurée est anormale | ✅ |
| 🔴⚪ rouge / blanc | la carte SD est pleine | 🧪 codé, difficile à tester (il faudrait remplir la carte) |
| 🔴⚪ rouge / blanc *(blanc 2× plus long)* | erreur de la carte SD | 🧪 retirer la carte en mode standard pour tester |

---

## 🌡️ 4. Mode standard *(LED verte)*

- ✅ Mesure tous les capteurs toutes les **LOG_INTERVAL** minutes (10 min par défaut)
- ✅ Chaque mesure est **datée** grâce à l'horloge
- ✅ Un capteur qui ne répond pas affiche **« NA »** (indisponible)
- ✅ Après **2 mesures** sans réponse, le capteur est signalé en erreur sur la LED
- ✅ Luminosité classée **faible / moyenne / forte**
- ✅ Valeur hors des seuils → LED « valeur anormale »
- ✅ Enregistrer les mesures sur la **carte SD**, dans des fichiers `AAMMJJ_0.LOG` (ex : `261014_0.LOG` ; `000000_0.LOG` si l'heure est indisponible), une ligne CSV par mesure
- ✅ Quand un fichier atteint **FILE_MAX_SIZE**, il est archivé en `_1`, `_2`… et l'écriture recommence dans `_0`

---

## ⚙️ 5. Mode configuration *(LED jaune)*

**Pour y entrer :** maintenir le bouton rouge appuyé, appuyer sur le bouton **reset** de la carte, puis relâcher.
Les commandes se tapent dans le **moniteur série**, puis Entrée.

- ✅ Les mesures sont **arrêtées** dans ce mode
- ✅ Les réglages sont **gardés en mémoire**, même après avoir débranché la carte
- ✅ Une valeur hors limites est **refusée** avec un message clair
- ✅ Toutes les commandes du PDF fonctionnent :

| Commande | Ce qu'elle fait | Exemple |
|---|---|---|
| `NOM=valeur` | change un réglage | `LOG_INTERVAL=5` |
| `RESET` | remet tous les réglages d'usine | |
| `VERSION` | affiche la version et le numéro de lot | |
| `CLOCK=` | règle l'heure | `CLOCK=14:30:00` |
| `DATE=` | règle la date (mois, jour, année) | `DATE=10,12,2026` |
| `DAY=` | règle le jour de la semaine | `DAY=MON` |
| `CONFIG` | affiche tous les réglages *(en plus du PDF, affichage seulement)* | |
| `HELP` | affiche la liste des commandes *(en plus du PDF, affichage seulement)* | |

**Pour quitter le mode configuration** (comme le PDF) : attendre **30 min sans activité**, ou redémarrer la carte sans appuyer sur le bouton rouge.

<details>
<summary><b>📋 Tous les réglages possibles (cliquer pour afficher)</b></summary>

| Réglage | Rôle | Valeurs | Par défaut |
|---|---|---|---|
| `LOG_INTERVAL` | minutes entre 2 mesures | 1 à 1440 | 10 |
| `FILE_MAX_SIZE` | taille max d'un fichier (octets) | 512 à 32768 | 2048 (4096 conseillé) |
| `TIMEOUT` | secondes d'attente d'un capteur | 1 à 300 | 30 |
| `LUMIN` | capteur de luminosité allumé | 0 ou 1 | 1 |
| `LUMIN_LOW` | en dessous = luminosité faible | 0 à 1023 | 255 |
| `LUMIN_HIGH` | au-dessus = luminosité forte | 0 à 1023 | 768 |
| `TEMP_AIR` | capteur de température allumé | 0 ou 1 | 1 |
| `MIN_TEMP_AIR` | température minimale (°C) | -40 à 85 | -10 |
| `MAX_TEMP_AIR` | température maximale (°C) | -40 à 85 | 60 |
| `HYGR` | capteur d'humidité allumé | 0 ou 1 | 1 |
| `HYGR_MINT` | humidité ignorée en dessous de (°C) | -40 à 85 | 0 |
| `HYGR_MAXT` | humidité ignorée au-dessus de (°C) | -40 à 85 | 50 |
| `PRESSURE` | capteur de pression allumé | 0 ou 1 | 1 |
| `PRESSURE_MIN` | pression minimale (hPa) | 300 à 1100 | 850 |
| `PRESSURE_MAX` | pression maximale (hPa) | 300 à 1100 | 1080 |

</details>

---

## 🔧 6. Mode maintenance *(LED orange)*

- ✅ Les mesures s'affichent **en direct** sur le moniteur série (toutes les 10 s)
- ✅ Plus rien n'est écrit sur la carte SD
- ✅ La carte SD est libérée à l'entrée en maintenance : elle peut être retirée et remise sans risque, puis elle est réutilisée automatiquement au retour en standard

---

## 🔋 7. Mode économique *(LED bleue)*

- ✅ Mesures **2 fois moins souvent** (LOG_INTERVAL × 2, soit 20 min par défaut)
- ✅ GPS lu **une mesure sur deux** seulement

---

## 💾 8. Carte SD — ✅ fait

- ✅ Faire répondre la carte : initialisation réussie sur la Nucleo actuelle
- ✅ Écrire les fichiers de mesures (environ 0,3 s par mesure)
- ✅ Changer de fichier quand il est plein (testé avec FILE_MAX_SIZE = 512)
- 🧪 Détecter la carte pleine et les erreurs (LED rouge / blanc) : codé, à tester en retirant la carte en mode standard

---

## 📄 9. Livrables — ❌ à faire

- ❌ Diagrammes (UML / SysML) et algorithmes
- ❌ Architecture du programme (fichiers, fonctions)
- ❌ Démonstration sur la carte
- ❌ Documentation technique et guide utilisateur

---

## 🧪 À tester par vous

1. **Mode configuration** : démarrer avec le rouge appuyé, taper `VERSION`, puis `LOG_INTERVAL=1`, puis redémarrer la carte normalement → une mesure par minute.
2. **Mode économique** : vert 5 s → la position GPS ne s'affiche qu'une mesure sur deux.
3. **Panne de l'horloge** : débrancher la RTC → LED rouge / bleu, fichier `000000_0.LOG`.
4. **Panne d'un capteur** : débrancher le BME280, la lumière ou le GPS → « indisponible », puis LED d'erreur.
5. **GPS** : le poser près d'une fenêtre → la position s'affiche après quelques minutes.
6. **Carte SD** : la retirer, la mettre dans le PC et ouvrir le fichier du jour (ex : `261014_0.LOG`) → une ligne par mesure.
7. **Retrait en maintenance** : rouge 5 s (orange), retirer puis remettre la carte, rouge 5 s → les mesures suivantes sont de nouveau enregistrées.
8. **Carte retirée en standard** : retirer la carte → à la mesure suivante, LED rouge / blanc (blanc 2× plus long).

> 💡 **Tester les pannes rapidement :** un capteur débranché est attendu **TIMEOUT** secondes (30 s par défaut) à chaque mesure, et l'erreur s'affiche après **2 mesures**, soit environ 1 min en maintenance. Pour une démonstration plus rapide, taper `TIMEOUT=5` en mode configuration : l'erreur s'affiche alors en environ 15 s. Remettre `TIMEOUT=30` (ou `RESET`) après.

---

## 📝 Choix à expliquer dans le rapport

- **Boutons en mode économique** : le rouge mène en maintenance, le vert ramène en standard (voir section 2).
- **Luminosité débranchée** : on charge l'entrée vers 0 V puis vers 3,3 V. Si les deux lectures sont très différentes, c'est que rien n'est branché.
- **Mémoire des réglages** : la carte n'a pas d'EEPROM, les réglages sont donc enregistrés dans sa mémoire flash.
- **Maintenance « en direct »** : une mesure toutes les 10 s, pour voir les valeurs bouger.
- **Limites de LOG_INTERVAL, TIMEOUT et FILE_MAX_SIZE** : le PDF ne les donne pas ; nous avons fixé 1-1440 min, 1-300 s et 512-32768 octets pour refuser les valeurs absurdes.
- **Commandes `CONFIG` et `HELP`** (mode configuration) : affichent les réglages actuels et la liste des commandes. Elles ne modifient rien.
- **Fichiers CSV** : le PDF demande « une seule ligne horodatée » sans imposer de format. Les colonnes sont séparées par `;`, ce qui permet d'ouvrir le fichier dans Excel.
- **Archivage d'un fichier plein** : le PDF demande une copie puis une reprise au début de `_0`. Renommer `_0` en `_N` puis créer un `_0` neuf donne le même résultat, plus vite.
- **`000000_0.LOG` si l'heure est indisponible** : si la RTC ne répond pas, la date est inconnue. `000000` garde le format du PDF tout en montrant qu'il n'y a pas de date, et la mesure n'est pas perdue.
- **Mémoire (prosit « Il faut changer la pile ! »)** : la pile mesurée dépassait la réservation du linker script (1 728 o pour 1 Ko). Les tampons de la carte SD ont été déplacés dans le tas (un seul `malloc` au démarrage, test de `NULL`), le tas est protégé contre la collision avec la pile, et le linker script a été ajusté d'après les mesures (2 Ko de pile, 2 Ko de tas). Détails dans la section 8 du README.
- **Nombre de fichiers LOG** : 2 Ko par défaut comme le PDF, soit environ 1 500 fichiers et 6 Mo par an (1 500 × 4 Ko) (la carte de 8 Go ne sera jamais pleine). La carte range les fichiers par blocs de 4 Ko, donc un fichier de 2 Ko gaspille la moitié de son bloc : nous conseillons `FILE_MAX_SIZE=4096`, qui divise le nombre de fichiers par 2 sans espace perdu.

### Petits ajouts non demandés par le PDF (sans effet sur le fonctionnement demandé)

- **Première mesure dès le démarrage** : le PDF dit « à intervalle régulier » sans préciser quand commence la première mesure ; la faire tout de suite montre que la station fonctionne.
- **Messages `/!\`** sous les mesures : la LED n'affiche que la panne la plus prioritaire ; en maintenance, ces messages montrent toutes les pannes en cours.
- **Ligne `[MEM] Tas` au démarrage** : c'est l'« analyse du tas » demandée par le prosit 5.
- **Valeur hors seuils gardée dans le fichier** : le PDF demande que le capteur « se mette en erreur » (LED) mais ne dit rien sur la valeur enregistrée ; on la garde pour ne pas perdre d'information.
- **Code LED pour une valeur hors seuils** : le PDF dit « en erreur » sans préciser lequel de ses deux codes rouge / vert ; nous utilisons « données incohérentes » (vert 2× plus long), qui correspond à une valeur anormale.
