# Worldwide Weather Watcher

Station météo embarquée réalisée dans le cadre du projet *Système embarqué* du CESI.
Installée à bord d'un navire, elle mesure la température, l'humidité, la pression,
la luminosité et la position GPS. Elle date chaque mesure et indique son état avec
une LED. Deux boutons permettent de passer d'un mode de fonctionnement à l'autre.

> Ce document explique **comment le programme est organisé** et **à quoi sert chaque fichier et chaque fonction**.
> L'avancement détaillé par rapport au cahier des charges est dans [CHECKLIST.md](CHECKLIST.md).

---

## Sommaire

1. [Matériel et branchements](#1-matériel-et-branchements)
2. [Compiler, téléverser, utiliser](#2-compiler-téléverser-utiliser)
3. [Les modes de fonctionnement](#3-les-modes-de-fonctionnement)
4. [Le code LED](#4-le-code-led)
5. [Organisation du code](#5-organisation-du-code)
6. [Le déroulement du programme](#6-le-déroulement-du-programme)
7. [Fichier par fichier : rôle et fonctions](#7-fichier-par-fichier--rôle-et-fonctions)
8. [Mémoire : pile et tas](#8-mémoire--pile-et-tas)
9. [Choix techniques](#9-choix-techniques)
10. [État du projet](#10-état-du-projet)

---

## 1. Matériel et branchements

Carte **STM32 Nucleo-L476RG** (microcontrôleur ARM Cortex-M4) avec un shield Grove.

| Module | Port du shield | Broches STM32 | Liaison |
|---|---|---|---|
| Capteur **BME280** (température, humidité, pression) | I2C | PB8 (SCL), PB9 (SDA) | I2C, adresse 0x76 |
| Horloge **RTC DS1307** | I2C | PB8 (SCL), PB9 (SDA) | I2C, adresse 0x68 |
| **GPS** Air530Z | A0 | PA0 (réception), PA1 (émission) | UART4, 9600 bauds |
| Capteur de **luminosité** | A2 | PA4 | entrée analogique (ADC) |
| **LED** RGB P9813 | D2 | PB3 (données), PA10 (horloge) | 2 fils |
| **Boutons** rouge et vert | D7 | PA8 (rouge), PA9 (vert) | entrées numériques |
| Lecteur de **carte SD** | SPI | PA5 (SCK), PA6 (MISO), PA7 (MOSI), CS sur PB5 | SPI, carte FAT32 ou exFAT |
| **Moniteur série** (PC) | câble USB | PA2, PA3 | USART2, 115200 bauds |

Toutes les broches sont définies à un seul endroit : [include/board_pins.h](include/board_pins.h).

> Le capteur monté est un **BME280** (identifiant 0x60). Le driver reconnaît aussi le BME680 (identifiant 0x61).

---

## 2. Compiler, téléverser, utiliser

Il faut **VS Code** et l'extension **PlatformIO**.

```bash
pio run                  # compiler
pio run -t upload        # téléverser sur la carte
pio device monitor       # ouvrir le moniteur série (115200 bauds)
```

Au démarrage, le moniteur série affiche :

```
[CONFIG] Parametres charges depuis la memoire (EEPROM)
[BOOT] Demarrage nominal -> MODE STANDARD
[RTC] 2026-10-14 11:09:33
[GPS] Ecoute sur UART4 (port A0, 9600 bauds)
[CAPTEUR] BME280 detecte en 0x76, calibration chargee
[SD] Carte montee : FAT32, 7544 Mo
[MEM] Tas : 1204 octets utilises, 92268 octets disponibles avant la pile

----- Mesure du 14/10/2026 a 11:09:34 -----
  Temperature : 24.1 C
  Humidite    : 52.0 %
  Pression    : 994.5 hPa
  Luminosite  : 412 / 1023 (moyenne)
  Position    : recherche des satellites... (0 en vue)

  -> enregistre dans 261014_0.LOG

  Prochaine mesure dans 10 min
```

---

## 3. Les modes de fonctionnement

| Mode | LED | Comment y entrer | Ce qu'il fait |
|---|---|---|---|
| **Standard** | 🟢 vert | démarrage normal | une mesure toutes les **LOG_INTERVAL** minutes (10 par défaut), enregistrée sur la carte SD |
| **Configuration** | 🟡 jaune | démarrer en maintenant le **bouton rouge** | mesures arrêtées, réglages par commandes série |
| **Économique** | 🔵 bleu | **vert 5 s** depuis le standard | mesures 2 fois moins souvent, GPS lu une mesure sur deux |
| **Maintenance** | 🟠 orange | **rouge 5 s** depuis le standard ou l'économique | mesures en direct toutes les 10 s ; carte SD libérée, elle peut être retirée sans risque |

```
🟢 STANDARD   ──vert 5 s──►  🔵 ÉCONOMIQUE ──vert 5 s──►  🟢 STANDARD
🟢 STANDARD   ──rouge 5 s─►  🟠 MAINTENANCE ──rouge 5 s─► 🟢 STANDARD
🔵 ÉCONOMIQUE ──rouge 5 s─►  🟠 MAINTENANCE ──rouge 5 s─► 🔵 ÉCONOMIQUE
                                             (retour au mode d'avant)

Démarrage avec le bouton rouge ──► 🟡 CONFIGURATION ──(30 min sans activité)──► 🟢 STANDARD
```

### Commandes du mode configuration

À taper dans le moniteur série, puis Entrée :

| Commande | Effet | Exemple |
|---|---|---|
| `NOM=valeur` | change un réglage (valeur contrôlée, puis enregistrée) | `LOG_INTERVAL=5` |
| `RESET` | remet tous les réglages par défaut | |
| `VERSION` | affiche la version et le numéro de lot | |
| `CLOCK=HH:MM:SS` | règle l'heure | `CLOCK=14:30:00` |
| `DATE=MM,JJ,AAAA` | règle la date | `DATE=10,13,2026` |
| `DAY=MON..SUN` | règle le jour de la semaine | `DAY=TUE` |
| `CONFIG` | affiche tous les réglages *(ajout, affichage seulement)* | |
| `HELP` | affiche la liste des commandes *(ajout, affichage seulement)* | |

Pour quitter le mode configuration : attendre 30 min sans activité, ou redémarrer la carte sans appuyer sur le bouton rouge.

Les réglages possibles et leurs limites sont listés dans [CHECKLIST.md](CHECKLIST.md#-5-mode-configuration-led-jaune).

---

## 4. Le code LED

| LED | Signification |
|---|---|
| 🟢 / 🟡 / 🔵 / 🟠 fixe | mode standard / configuration / économique / maintenance |
| 🔴🔵 rouge / bleu | l'horloge RTC ne répond pas |
| 🔴🟡 rouge / jaune | le GPS ne répond pas |
| 🔴🟢 rouge / vert | un capteur ne répond pas |
| 🔴🟢 rouge / vert, vert 2× plus long | une valeur mesurée est hors des seuils |
| 🔴⚪ rouge / blanc | carte SD pleine |
| 🔴⚪ rouge / blanc, blanc 2× plus long | erreur d'écriture sur la carte SD |

Les erreurs clignotent à 1 Hz. Si plusieurs pannes surviennent en même temps, la LED affiche la plus prioritaire (dans l'ordre du tableau).

---

## 5. Organisation du code

Le code est découpé en **3 couches**. Chaque couche n'utilise que celle du dessous.

```
┌──────────────────────────────────────────────────────────────────────┐
│  PROGRAMME PRINCIPAL                                         src/    │
│  main.c : démarrage, boucle principale, rythme des mesures par mode  │
├──────────────────────────────────────────────────────────────────────┤
│  MODULES DE L'APPLICATION                                    src/    │
│  buttons.c        boutons et changements de mode                     │
│  acquisition.c    mesure de tous les capteurs, timeouts, alertes     │
│  datalog.c        enregistrement des mesures sur la carte SD         │
│  console.c        commandes du mode configuration                    │
│  config.c         réglages et sauvegarde en mémoire flash            │
│  system_status.c  couleur et clignotement de la LED                  │
│  setup.c          initialisation de la carte (horloge, I2C, SPI…)    │
│  heap.c           tas protégé contre la collision avec la pile       │
├──────────────────────────────────────────────────────────────────────┤
│  SYSTÈME DE FICHIERS                                 lib/fatfs/      │
│  FatFs (fichiers FAT32 / exFAT) + diskio.c (lien avec sd_logger.c)   │
├──────────────────────────────────────────────────────────────────────┤
│  DRIVERS DES COMPOSANTS                         lib/cesi_grove/      │
│  grove_bme680.c   grove_rtc_ds1307.c   grove_gps_air530z.c           │
│  grove_light.c    grove_rgb.c          sd_logger.c                   │
├──────────────────────────────────────────────────────────────────────┤
│  STM32Cube HAL (bibliothèque de ST pour piloter le microcontrôleur)  │
└──────────────────────────────────────────────────────────────────────┘
```

```text
.
├── include/              en-têtes des modules (.h)
│   ├── board_pins.h      branchements de tous les modules
│   ├── cesi_types.h      types partagés (date, mesures, GPS)
│   ├── modes.h           les 4 modes
│   └── version.h         version et numéro de lot
├── src/                  modules de l'application (.c)
├── lib/cesi_grove/       drivers des composants Grove
├── lib/fatfs/            système de fichiers FatFs R0.12c + diskio.c
├── platformio.ini        configuration de compilation
├── STM32L476RGTX_FLASH.ld  linker script (tailles de la pile et du tas)
├── CHECKLIST.md          avancement par rapport au cahier des charges
└── README.md             ce document
```

---

## 6. Le déroulement du programme

### Au démarrage (`main()`)

1. `Global_Init()` initialise la carte : horloge, I2C, SPI et moniteur série.
2. `Console_Init()`, `GroveRGB_Init()` et `GroveLight_Init()` préparent la console, la LED et le capteur de lumière.
3. `Config_Load()` relit les réglages enregistrés, ou prend les valeurs par défaut.
4. `check_boot_mode()` choisit le mode **configuration** si le bouton rouge est appuyé, sinon **standard**.
5. `rtc_check()` affiche l'heure de l'horloge RTC (réglage avec les commandes `CLOCK`, `DATE` et `DAY`).
6. `GroveGPS_Init()` et `GroveBME680_Init()` démarrent le GPS et le capteur BME280.
7. `DataLog_Init()` monte la carte SD.

### La boucle principale

Le programme ne s'arrête **jamais** pour attendre un capteur : chaque tour de boucle est très court. Les boutons répondent donc toujours, même pendant une mesure.

```
while (1) {
   A. process_button_presses()   lit les boutons, change de mode après 5 s
      Console_Process()          exécute les commandes tapées au clavier
      GroveGPS_Process()         décode les trames GPS reçues en arrière-plan

   B. le mode a changé ?         annonce le nouveau mode, mesure tout de suite

   C. c'est l'heure de mesurer ? Acquisition_Start()
         standard    : toutes les LOG_INTERVAL minutes
         économique  : toutes les LOG_INTERVAL × 2 minutes, GPS une fois sur deux
         maintenance : toutes les 10 secondes
         configuration : jamais

   D. Acquisition_Poll()         la mesure est finie ? -> affichage + mise à jour des alertes
      DataLog_Write()            standard et économique : ajoute la mesure au fichier de la carte SD

   E. SystemStatus_Update()      couleur ou clignotement de la LED
}
```

### Une mesure (`acquisition.c`)

```
Acquisition_Start()
   ├─ lit l'heure sur la RTC si elle est installée (en cas d'échec : alerte RTC)
   └─ prépare la lecture des capteurs

Acquisition_Poll(), à chaque tour de boucle :
   ├─ toutes les 200 ms, réessaie les capteurs qui n'ont pas encore répondu
   │     read_env()    BME280 : température, humidité, pression
   │     read_light()  luminosité (vérifie d'abord que le capteur est branché)
   │     read_gps()    attend une nouvelle position GPS
   ├─ tous ont répondu, ou TIMEOUT (30 s) est dépassé ?
   │     ├─ capteur sans réponse      -> "indisponible" (NA)
   │     ├─ 2 mesures sans réponse    -> capteur en erreur (LED)
   │     └─ valeur hors des seuils    -> donnée incohérente (LED)
   └─ renvoie la mesure complète
```

---

## 7. Fichier par fichier : rôle et fonctions

### Programme principal

#### [src/main.c](src/main.c) — démarrage et boucle principale

| Fonction | Rôle |
|---|---|
| `main()` | initialise tout, puis exécute la boucle principale décrite plus haut |
| `acquisition_period_ms()` | temps entre 2 mesures selon le mode (0 en configuration) |
| `announce_mode()` | affiche un message à l'entrée dans chaque mode |
| `rtc_check()` | vérifie la RTC au démarrage et la met à l'heure si elle est arrêtée |

### Modules de l'application

#### [src/buttons.c](src/buttons.c) — boutons et changements de mode

Chaque bouton a 3 états : *relâché*, *appui en cours* et *action faite*. L'état *action faite* évite de déclencher l'action plusieurs fois tant que le bouton reste appuyé.

| Fonction | Rôle |
|---|---|
| `buttons_init()` | configure PA8 et PA9 en entrées avec résistance de tirage vers le haut |
| `check_boot_mode()` | au démarrage : mode configuration si le rouge est appuyé |
| `process_button_presses()` | détecte les appuis de 5 s et change de mode ; gère le retour automatique après 30 min en configuration |
| `buttons_config_activity()` | relance le délai de 30 min (appelée à chaque commande reçue) |
| `print_mode()` | affiche le mode actif sur le moniteur série |

#### [src/acquisition.c](src/acquisition.c) — mesure des capteurs

| Fonction | Rôle |
|---|---|
| `Acquisition_Start(with_gps)` | lance une mesure ; `with_gps = false` en économique une fois sur deux |
| `Acquisition_Poll(&mesure)` | fait avancer la mesure ; renvoie `true` quand elle est terminée |
| `Acquisition_IsRunning()` | indique si une mesure est en cours |
| `Acquisition_GetHealth()` | état des capteurs : RTC, GPS, capteurs, données cohérentes |
| `Acquisition_Print()` | affiche la mesure de façon lisible sur le moniteur série |
| `Acquisition_FormatLine()` | écrit la mesure sur une ligne CSV `date;temp;hum;pres;lum;niveau;lat;lon`, enregistrée sur la carte SD |
| `read_env()` / `read_light()` / `read_gps()` | lecture de chaque capteur (fonctions internes) |
| `update_timeouts()` | compte les mesures consécutives sans réponse d'un capteur |
| `data_coherent()` | vérifie la température et la pression par rapport aux seuils |

#### [src/datalog.c](src/datalog.c) — enregistrement sur la carte SD

Chaque mesure est ajoutée sur une ligne au format CSV (séparateur `;`), dans le fichier `AAMMJJ_0.LOG` du jour (`000000_0.LOG` si l'heure est indisponible : horloge en panne ou désactivée). Une ligne d'en-tête est écrite au début de chaque fichier :

```
DATE HEURE;TEMP_AIR(C);HYGR(%);PRESSURE(hPa);LUMIN(0-1023);LUMIN_NIVEAU;LATITUDE;LONGITUDE
NA;23.8;56.9;1003.9;514;MOYENNE;NA;NA
```

Quand l'ajout d'une ligne ferait dépasser **FILE_MAX_SIZE**, le fichier est archivé sous le premier numéro libre (`AAMMJJ_1.LOG`, `_2`…) et l'écriture reprend dans un `AAMMJJ_0.LOG` vide.

| Fonction | Rôle |
|---|---|
| `DataLog_Init()` | monte la carte au démarrage et affiche son type et sa taille |
| `DataLog_Write(&mesure)` | ajoute la mesure au fichier ; archive le fichier s'il est plein ; remonte la carte si elle a été retirée puis remise |
| `DataLog_Release()` | libère la carte à l'entrée en maintenance, pour pouvoir la retirer sans risque |
| `DataLog_IsOk()` / `DataLog_HasSpace()` | état de la carte pour la LED : erreur d'accès ou carte pleine |
| `file_name()` / `archive()` | nom `AAMMJJ_N.LOG` et archivage d'un fichier plein (fonctions internes) |

#### [lib/fatfs/](lib/fatfs/) — système de fichiers

**FatFs** (version R0.12c, fournie avec STM32Cube) gère les fichiers et les dossiers sur une carte FAT32 ou exFAT. Le formatage est désactivé : le programme ne peut jamais effacer la carte.
[diskio.c](lib/fatfs/diskio.c) fait le lien entre FatFs et le driver `sd_logger.c` : lecture et écriture de secteurs, capacité, date des fichiers (prise sur la RTC si elle est installée).

#### [src/console.c](src/console.c) — commandes du mode configuration

Les caractères tapés arrivent **par interruption** dans un tampon. La boucle principale les traite ensuite, ce qui évite d'en perdre.

| Fonction | Rôle |
|---|---|
| `Console_Init()` | démarre la réception des caractères sur USART2 |
| `Console_Process()` | renvoie les caractères tapés à l'écran, gère la touche Retour arrière, exécute la ligne à l'appui sur Entrée |
| `Console_PrintHelp()` | affiche la liste des commandes |
| `execute()` | reconnaît la commande et appelle la bonne fonction ; refuse les commandes hors du mode configuration |
| `cmd_param()` / `cmd_clock()` / `cmd_date()` / `cmd_day()` | traitement de chaque commande, avec vérification du format |
| `HAL_UART_RxCpltCallback()` | appelée à chaque caractère reçu : moniteur série (USART2) ou GPS (UART4) |

#### [src/config.c](src/config.c) — réglages et mémoire

Tous les réglages sont décrits dans un tableau `params[]` : nom, emplacement en mémoire, valeur minimale et maximale. Ajouter un réglage revient à ajouter une ligne à ce tableau.

| Fonction | Rôle |
|---|---|
| `Config_LoadDefaults()` | valeurs par défaut du cahier des charges |
| `Config_Load()` | relit les réglages en mémoire flash au démarrage |
| `Config_Save()` | enregistre les réglages en mémoire flash |
| `Config_Set(nom, valeur)` | modifie un réglage après avoir vérifié la valeur et les seuils (seuil bas < seuil haut) |
| `Config_GetRange()` | limites autorisées d'un réglage (pour les messages d'erreur) |
| `Config_Print()` | affiche tous les réglages |

#### [src/system_status.c](src/system_status.c) — LED

| Fonction | Rôle |
|---|---|
| `SystemStatus_Init()` | prépare le suivi du clignotement |
| `SystemStatus_Update()` | affiche le code d'erreur le plus prioritaire, ou la couleur du mode |
| `get_error()` | choisit l'erreur à afficher, sa couleur et la durée de chaque couleur |
| `set_color()` | n'envoie une couleur à la LED que si elle a changé |

#### [src/heap.c](src/heap.c) — tas protégé

| Fonction | Rôle |
|---|---|
| `_sbrk()` | appelée par `malloc()` pour agrandir le tas ; refuse d'entrer dans la zone réservée à la pile |
| `Heap_Used()` / `Heap_Available()` | octets du tas utilisés et encore disponibles (affichés au démarrage) |

#### [src/setup.c](src/setup.c) — initialisation de la carte

| Fonction | Rôle |
|---|---|
| `Global_Init()` | appelle toutes les initialisations ci-dessous |
| `SystemClock_Config()` | horloge du microcontrôleur (MSI, 4 MHz) |
| `MX_I2C1_Init()` / `MX_SPI1_Init()` / `MX_USART2_UART_Init()` | bus I2C, bus SPI et moniteur série |
| `HAL_UART_MspInit()` | broches et interruptions des liaisons série (moniteur et GPS) |
| `_write()` | redirige `printf` vers le moniteur série |

### Drivers des composants (`lib/cesi_grove/`)

| Fichier | Composant | Fonctions principales |
|---|---|---|
| [grove_bme680.c](lib/cesi_grove/src/grove_bme680.c) | capteur BME280 / BME680 | `GroveBME680_Init()` détecte le capteur et lit sa calibration · `GroveBME680_ReadEnvironment()` lance une mesure et calcule les valeurs avec les formules du fabricant (Bosch) |
| [grove_rtc_ds1307.c](lib/cesi_grove/src/grove_rtc_ds1307.c) | horloge RTC | `GroveRTC_GetDateTime()` · `GroveRTC_SetDateTime()` |
| [grove_gps_air530z.c](lib/cesi_grove/src/grove_gps_air530z.c) | GPS | `GroveGPS_Init()` démarre la réception par interruption · `GroveGPS_Process()` vérifie et décode les trames de position (GGA) · `GroveGPS_GetLast()` renvoie la dernière position |
| [grove_light.c](lib/cesi_grove/src/grove_light.c) | luminosité | `GroveLight_ReadRaw()` lecture 0-4095 · `GroveLight_IsConnected()` détecte un capteur débranché |
| [grove_rgb.c](lib/cesi_grove/src/grove_rgb.c) | LED RGB | `GroveRGB_SetColor(r, g, b)` · `GroveRGB_Off()` |
| [sd_logger.c](lib/cesi_grove/src/sd_logger.c) | carte SD (SPI) | `SDLogger_Init()` démarre la carte · `SDLogger_ReadBlocks()` / `SDLogger_WriteBlocks()` lisent et écrivent des secteurs de 512 octets · `SDLogger_GetSectorCount()` capacité de la carte · `SDLogger_BitBangCMD0Test()` diagnostic du câblage |

### Fichiers d'en-tête utiles

| Fichier | Contenu |
|---|---|
| [include/board_pins.h](include/board_pins.h) | toutes les broches utilisées |
| [include/cesi_types.h](include/cesi_types.h) | `RTC_DateTime` (date et heure), `Env_Data` (température, humidité, pression), `GPS_Data` (position) |
| [include/modes.h](include/modes.h) | les 4 modes et la variable globale `mode_actuel` |
| [include/config.h](include/config.h) | la structure `Config` qui contient tous les réglages |
| [include/acquisition.h](include/acquisition.h) | la structure `Measurement` (une mesure complète) |
| [include/version.h](include/version.h) | version du programme et numéro de lot |

---

## 8. Mémoire : pile et tas

Le STM32L476RG a **96 Ko de RAM**. Elle est partagée entre trois zones :

```
0x20000000 ┌──────────────────────────────┐
           │ variables globales/statiques │  .data + .bss : 2,7 Ko
           ├──────────────────────────────┤  <- end
           │ TAS (malloc), grandit ↓      │  1,2 Ko utilisés
           │                              │
           │        RAM libre             │  ~ 90 Ko
           │                              │
           │ PILE, grandit ↑              │  1,1 Ko utilisés au maximum
0x20018000 └──────────────────────────────┘  <- _estack
```

### Mesures réalisées sur la maquette

| Mesure | Avant optimisation | Après |
|---|---|---|
| Variables globales et statiques (`.data` + `.bss`) | 2,7 Ko | 2,7 Ko |
| Pile utilisée au maximum (remplissage de la RAM avec un motif, puis lecture au débogueur) | **1 728 o** | **1 088 o** |
| Tas utilisé (affiché au démarrage : `[MEM] Tas : ...`) | 0 o | 1 204 o |
| Pile réservée par le linker script | 1 Ko (dépassée) | 2 Ko |
| Tas réservé par le linker script | 512 o | 2 Ko |

### Ce qui a été fait

1. **Les gros tampons ont quitté la pile.** Le fichier ouvert sur la carte SD (`FIL`, environ 600 octets) et la ligne CSV (160 octets) étaient déclarés dans `DataLog_Write()`, donc sur la pile. Ils sont maintenant alloués dans le **tas**.
2. **Un seul `malloc`, au démarrage** (`DataLog_Init()`), jamais dans la boucle principale. Des `malloc` / `free` répétés **fragmenteraient le tas**.
3. **Test de `NULL` après chaque `malloc`.** En cas d'échec, l'enregistrement SD est désactivé proprement avec un message, au lieu d'écrire à une adresse invalide.
4. **Pas de `free`** pour ces tampons : ils servent pendant toute la vie du programme, qui ne se termine jamais.
5. **Tas protégé contre la collision avec la pile** ([src/heap.c](src/heap.c)). La fonction `_sbrk()`, appelée par `malloc()`, refuse de faire grandir le tas dans la zone réservée à la pile : `malloc()` renvoie alors `NULL` au lieu d'écraser la pile.
6. **Linker script ajusté d'après les mesures** ([STM32L476RGTX_FLASH.ld](STM32L476RGTX_FLASH.ld)) : 2 Ko pour la pile et 2 Ko pour le tas. Si le programme dépassait un jour la RAM disponible, l'éditeur de liens refuserait de le compiler.

Les petites variables (indices, valeurs lues, structures de quelques octets) restent sur la pile. Les gros tableaux sont passés **par adresse** (pointeur) aux fonctions, et les petites valeurs **par valeur**.

---

## 9. Choix techniques

| Choix | Pourquoi |
|---|---|
| **Programme non bloquant** | une mesure avance petit à petit à chaque tour de boucle ; les boutons et la console répondent toujours |
| **Réception GPS et clavier par interruption** | aucun caractère n'est perdu, même pendant une mesure |
| **Détection du capteur de lumière débranché** | l'entrée est chargée vers 0 V puis vers 3,3 V : si les deux lectures diffèrent, rien n'est branché |
| **Réglages en mémoire flash** | la carte n'a pas d'EEPROM ; un code de reconnaissance et une somme de contrôle protègent les données |
| **Vert pour sortir de l'économique** | le PDF donne deux rôles au bouton rouge en économique ; chaque bouton garde ainsi un seul rôle |
| **Maintenance toutes les 10 s** | le PDF demande un affichage « en direct » sans préciser le rythme |
| **Fichiers de la carte SD au format CSV** (séparateur `;`) | le PDF demande « une seule ligne horodatée » sans imposer de format ; le CSV s'ouvre directement dans Excel (en renommant `.LOG` en `.csv`) |
| **Archivage par renommage** | le PDF demande une copie du fichier plein puis une reprise au début de `_0` : renommer `_0` en `_N` puis créer un `_0` neuf donne exactement le même résultat, plus rapidement |
| **`000000_0.LOG` si l'heure est indisponible** | si la RTC ne répond pas, la date est inconnue : `000000` garde le format `AAMMJJ` du PDF tout en indiquant clairement l'absence de date, et aucune mesure n'est perdue |
| **FILE_MAX_SIZE : 2 Ko par défaut, 4096 conseillé** | le PDF fixe 2 Ko par défaut, valeur conservée. Mais la carte range les fichiers par blocs de 4 Ko : un fichier de 2 Ko laisse la moitié de son bloc vide. Avec `FILE_MAX_SIZE=4096` (l'exemple du PDF), il y a **2 fois moins de fichiers** et plus aucun espace perdu. À 10 min par mesure : environ 1 500 fichiers et 6 Mo par an avec 2 Ko (1 500 × 4 Ko), environ 750 fichiers avec 4 Ko |
| **Limites de LOG_INTERVAL, TIMEOUT et FILE_MAX_SIZE** | non données par le PDF : 1-1440 min, 1-300 s et 512-32768 octets, pour refuser les valeurs absurdes |
| **Commandes `CONFIG` et `HELP`** (mode configuration) | ajoutées pour afficher les réglages actuels et la liste des commandes ; elles ne modifient rien |

---

## 10. État du projet

| Partie | État |
|---|---|
| Capteurs (BME280, GPS, luminosité) | ✅ fonctionnels, pannes détectées |
| Horloge RTC | ✅ branchée et à l'heure : mesures et noms de fichiers datés |
| LED (modes et codes d'erreur) | ✅ |
| Les 4 modes et les boutons | ✅ |
| Mode configuration et sauvegarde des réglages | ✅ |
| **Carte SD** (fichiers `AAMMJJ_0.LOG`, archivage, maintenance) | ✅ écriture, archivage et libération en maintenance testés ; carte pleine codée mais non testée |
| Livrables (diagrammes, documentation, guide utilisateur) | ❌ à faire |

---

## Ressources

- [Documentation STM32 Nucleo-L476RG](https://stm32python.gitlab.io/fr/docs/Kit/nucleo_l476rg)
- [Documentation PlatformIO](https://docs.platformio.org/)
- [Dépôt d'architecture Worldwide Weather Watcher](https://github.com/ValentinMoraine/Worldwide-Weather-Watcher)

Projet réalisé par l'équipe du dépôt [samyr13/Projet-Worldwide-Weather-Watcher](https://github.com/samyr13/Projet-Worldwide-Weather-Watcher).
