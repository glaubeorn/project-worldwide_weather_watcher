#!/bin/bash
# =============================================================================
# auto.sh — Script d'automatisation de compilation et televersement STM32
# =============================================================================
# Cible       : NUCLEO-L476RG (STM32L476RG — Cortex-M4)
# Outils      : arm-none-eabi-gcc, arm-none-eabi-objcopy, st-flash
# Usage       : bash auto.sh f=<dossier> [init]
#   f=<dossier>  : dossier contenant les fichiers sources .c/.h
#   init         : (optionnel) lance l'initialisation du fichier de configuration
#
# Exemple     : bash auto.sh f=projet
#               bash auto.sh f=projet init
# =============================================================================

##############################################
# SECTION 1 : FONCTIONS UTILITAIRES
##############################################

# cleanInsideFolder — Vide le contenu d'un dossier sans supprimer le dossier
# Parametre $1 : chemin du dossier a vider
function cleanInsideFolder() {
    if [ "$1" != "" ]; then
        rm -rf "$1"/*
    else
        echo "error: folder is null"
    fi
}

##############################################
# SECTION 2 : BANNIERE ET LECTURE DES ARGUMENTS
##############################################

printf "#####################\n"
printf "#   COMPIL STM32    #\n"
printf "#####################\n"

FOLDER=""
SINIT=0

# Parcourir les arguments passes au script
# f=* : matche tout argument commencant par "f="
# ${i#f=} : supprime le prefixe "f=" → ne garde que le nom du dossier
for i in "$@"; do
    case $i in
        f=*) FOLDER="${i#f=}" ;;
        init) SINIT=1 ;;
    esac
done

##############################################
# SECTION 3 : FICHIER DE CONFIGURATION (.stm32cesi)
##############################################
# Format :
#   {
#     type: "STM32L476RG",
#     tool: "st-flash",
#   }

if [ "$SINIT" == "1" ]; then
    printf "INITIALISATION EN COURS\n"

    rm -f "$FOLDER/.stm32cesi"
    echo "{" >> "$FOLDER/.stm32cesi"

    printf 'Quel type de microcontroleur utilisez-vous ? [STM32L476RG] '
    read -r TYPE
    [[ "$TYPE" == "" ]] && TYPE="STM32L476RG"
    echo '  type: "'"$TYPE"'",' >> "$FOLDER/.stm32cesi"

    printf 'Quel outil de televersement utilisez-vous ? [st-flash] '
    read -r FLASHTOOL
    [[ "$FLASHTOOL" == "" ]] && FLASHTOOL="st-flash"
    echo '  tool: "'"$FLASHTOOL"'",' >> "$FOLDER/.stm32cesi"

    echo "}" >> "$FOLDER/.stm32cesi"
fi

# Lire la configuration avec grep + regex PCRE
# (?<=type: ") : lookbehind — ce qui suit 'type: "'
# [^"]*        : capturer tout sauf les guillemets
CONF_TYPE=$(grep -oP '(?<=type: ")[^"]*' "$FOLDER/.stm32cesi" 2>/dev/null)
CONF_TOOL=$(grep -oP '(?<=tool: ")[^"]*' "$FOLDER/.stm32cesi" 2>/dev/null)

[[ -z "$CONF_TYPE" ]] && CONF_TYPE="STM32L476RG"
[[ -z "$CONF_TOOL" ]] && CONF_TOOL="st-flash"

##############################################
# SECTION 4 : VERIFICATION ET PREPARATION DES DOSSIERS
##############################################
# IMPORTANT : verifier FOLDER vide EN PREMIER
# car [ ! -d "" ] est VRAI → afficherait le mauvais message

if [ "$FOLDER" == "" ]; then
    echo 'Aucun dossier specifie, indiquer f=<nom_du_dossier>'
    exit 1
elif [ ! -d "$FOLDER" ]; then
    echo "Le dossier $FOLDER n'existe pas"
    exit 1
else
    printf 'P1'
    cd "$FOLDER" || exit 1

    if [ ! -d ".tmp" ]; then
        mkdir .tmp
    else
        cleanInsideFolder ".tmp/"
    fi
    printf '.'

    if [ ! -d "build" ]; then
        mkdir build
    else
        cleanInsideFolder "build/"
    fi
    printf '.'

    printf ' done [env ready]\n'
fi

##############################################
# SECTION 5 : DETECTION DU MODE DEBUG
##############################################
# Regex : (?<=#define DEBUG )[0-9]+
# Capture uniquement le chiffre apres #define DEBUG

DEBUG=$(grep -oP '(?<=#define DEBUG )[0-9]+' main.c)
echo "Mode debug detecte : ${DEBUG:-0}"

##############################################
# SECTION 6 : MODE DEBUG — COMPILATION ETAPE PAR ETAPE
##############################################
# Phase 1 — Preprocesseur  (.c → .i)  : -E
# Phase 2 — Compilation    (.i → .s)  : -S
# Phase 3 — Assemblage     (.i → .o)  : -c

if [ "$DEBUG" == "1" ]; then
    printf '######### DEBUG MODE ENABLE #########\n'

    [ ! -d ".debug" ] && mkdir .debug || cleanInsideFolder ".debug/"

    # Phase 1 : PREPROCESSEUR (.c → .i)
    printf 'Preprocesseur uniquement [/.debug/1_preproc/<files.i>]\n'
    [ ! -d ".debug/1_preproc" ] && mkdir .debug/1_preproc || cleanInsideFolder ".debug/1_preproc/"
    for c in *.c; do
        [ -f "$c" ] || break; printf '.'
        arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -DSTM32L476xx -I. \
            -E "$c" -o ".debug/1_preproc/${c%.c*}.i" 2>/dev/null
    done
    printf ' done [.i ready]\n'

    # Phase 2 : COMPILATION (.i → .s)
    printf 'Compilation uniquement [/.debug/2_compil/<files.s>]\n'
    [ ! -d ".debug/2_compil" ] && mkdir .debug/2_compil || cleanInsideFolder ".debug/2_compil/"
    for i in .debug/1_preproc/*.i; do
        [ -f "$i" ] || break; printf '.'
        arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -DSTM32L476xx \
            -S "$i" -o "${i%.i*}.s"
    done
    mv .debug/1_preproc/*.s .debug/2_compil
    printf ' done [.s ready]\n'

    # Phase 3 : ASSEMBLAGE (.i → .o)
    printf 'Compilation et Assemblage uniquement [/.debug/3_assembly/<files.o>]\n'
    [ ! -d ".debug/3_assembly" ] && mkdir .debug/3_assembly || cleanInsideFolder ".debug/3_assembly/"
    for i in .debug/1_preproc/*.i; do
        [ -f "$i" ] || break; printf '.'
        arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -DSTM32L476xx \
            -c "$i" -o "${i%.i*}.o"
    done
    mv .debug/1_preproc/*.o .debug/3_assembly
    printf ' done [.o ready]\n'
fi

##############################################
# SECTION 7 : COMPILATION NORMALE (.c → .o)
##############################################
# Options arm-none-eabi-gcc :
#   -mcpu=cortex-m4      : processeur Cortex-M4
#   -mthumb              : code Thumb-2 (16/32 bits)
#   -mfpu=fpv4-sp-d16    : FPU materielle
#   -mfloat-abi=hard     : convention d'appel hard float
#   -DSTM32L476xx        : macro pour les registres HAL
#   -Os                  : optimisation en taille
#   -c                   : compilation sans edition de liens

filesC=""
for c in *.c; do
    [ -f "$c" ] || break
    arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb \
        -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
        -DSTM32L476xx -Os -I. \
        -c "$c" -o ".tmp/${c%.c*}.o" 2>/dev/null
    filesC="$filesC $c"
done
printf 'P2... done [.o ready]\n'

##############################################
# SECTION 8 : EDITION DES LIENS (.o → ELF)
##############################################

printf 'P3'
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb \
    -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
    -DSTM32L476xx .tmp/*.o -o "build/$FOLDER" 2>/dev/null
printf '. done [links]\n'

##############################################
# SECTION 9 : CONVERSION ELF → HEX et BIN
##############################################

printf 'P4'
arm-none-eabi-objcopy -O ihex   "build/$FOLDER" "build/$FOLDER.hex"
arm-none-eabi-objcopy -O binary "build/$FOLDER" "build/$FOLDER.bin"
printf '. done [.hex]\n'

##############################################
# SECTION 10 : TELEVERSEMENT SUR LA CARTE
##############################################
# st-flash ecrit le .bin a l'adresse 0x08000000 (debut Flash STM32)

printf "Voulez-vous commencer le televersement ? [O/N] : "
read -r TELEV
TELEV=$(echo "$TELEV" | tr '[:lower:]' '[:upper:]')

if [[ "$TELEV" == 'N' ]]; then
    printf "Bye bye\n"
else
    printf "Demarrage du televersement\n"
    $CONF_TOOL --reset write "build/$FOLDER.bin" 0x08000000
fi

# Nettoyage des fichiers temporaires
printf "Clean datas\n"
cleanInsideFolder ".tmp/"

exit 0
