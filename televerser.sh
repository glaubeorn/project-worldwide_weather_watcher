#!/usr/bin/env bash
#
# Station météo 3W : compile le programme, le téléverse sur la carte
# NUCLEO-L476RG, puis ouvre le moniteur série.
#
# Utilisation sous Windows (Git Bash) :
#   - double-cliquer sur televerser.sh ;
#   - ou, dans un terminal Git Bash : ./televerser.sh
#
# Quitter le moniteur série : Ctrl+C

# Toujours travailler dans le dossier du projet, d'où que le script soit lancé
cd "$(dirname "$0")" || exit 1

# Garde la fenêtre ouverte en cas d'erreur (utile après un double-clic)
fin_erreur() {
    echo
    echo "ERREUR : $1"
    read -r -p "Appuyer sur Entree pour fermer..."
    exit 1
}

# 1. Recherche de PlatformIO : commande pio, sinon installation de l'extension VS Code
if command -v pio >/dev/null 2>&1; then
    PIO="pio"
elif [ -x "$HOME/.platformio/penv/Scripts/pio.exe" ]; then
    PIO="$HOME/.platformio/penv/Scripts/pio.exe"
else
    fin_erreur "PlatformIO introuvable. Installer l'extension PlatformIO IDE dans VS Code."
fi

echo "=============================================="
echo "   STATION METEO 3W - COMPILATION ET ENVOI"
echo "=============================================="

# 2. La carte doit être branchée en USB (port série du ST-Link)
echo
echo "[1/4] Recherche de la carte..."
if ! "$PIO" device list | grep -q "VID:PID=0483:374B"; then
    fin_erreur "carte NUCLEO non detectee. Brancher le cable USB et reessayer."
fi
echo "Carte detectee."

# 3. Compilation
echo
echo "[2/4] Compilation..."
"$PIO" run || fin_erreur "la compilation a echoue (voir les messages ci-dessus)."

# 4. Téléversement par le ST-Link
echo
echo "[3/4] Televersement sur la carte..."
"$PIO" run --target upload || fin_erreur "le televersement a echoue. Fermer les logiciels qui utilisent le ST-Link (STM32CubeProgrammer, session de debug) et reessayer."

# 5. Moniteur série (115200 bauds, réglé dans platformio.ini)
echo
echo "[4/4] Ouverture du moniteur serie (Ctrl+C pour quitter)..."
echo
"$PIO" device monitor || fin_erreur "moniteur serie inaccessible. Fermer le moniteur deja ouvert (VS Code, PuTTY...) et reessayer."
