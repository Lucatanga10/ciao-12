#!/data/data/com.termux/files/usr/bin/bash
# OJBS setup completo per Termux su S25 Ultra con KernelSU
# Lancia questo script UNA VOLTA per preparare tutto.
# Dopo, segui le istruzioni che stampa alla fine.
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { printf "${GREEN}[+]${NC} %s\n" "$1"; }
warn()  { printf "${YELLOW}[!]${NC} %s\n" "$1"; }
err()   { printf "${RED}[-]${NC} %s\n" "$1"; }
die()   { err "$1"; exit 1; }

# ---- check Termux
[ -d /data/data/com.termux ] || die "non stai su Termux"

# ---- check root
if ! su -c "id -u" 2>/dev/null | grep -q '^0$'; then
    die "root non disponibile — controlla KernelSU"
fi
info "root OK"

# ---- install dependencies
info "installo pacchetti necessari..."
pkg update -y 2>/dev/null || true
pkg install -y python git wget curl 2>/dev/null
pip install frida-tools 2>/dev/null || pip install frida-tools --break-system-packages 2>/dev/null

FRIDA_VER="$(frida --version 2>/dev/null)" || die "frida non installato"
info "frida $FRIDA_VER installato"

# ---- detect arch
ARCH="$(uname -m)"
case "$ARCH" in
    aarch64|arm64) FRIDA_ARCH="arm64" ;;
    armv7l|armv8l) FRIDA_ARCH="arm" ;;
    *) die "arch non supportata: $ARCH" ;;
esac
info "arch: $FRIDA_ARCH"

# ---- download frida-inject
INJECT="/data/local/tmp/ojbs/frida-inject"
if [ -x "$INJECT" ]; then
    info "frida-inject gia' presente"
else
    info "scarico frida-inject $FRIDA_VER per android-$FRIDA_ARCH..."
    mkdir -p /data/local/tmp/ojbs
    URL="https://github.com/frida/frida/releases/download/${FRIDA_VER}/frida-inject-${FRIDA_VER}-android-${FRIDA_ARCH}.xz"
    wget -q "$URL" -O /tmp/frida-inject.xz || die "download fallito: $URL"
    xz -d /tmp/frida-inject.xz
    su -c "cp /tmp/frida-inject '$INJECT' && chmod 755 '$INJECT'"
    rm -f /tmp/frida-inject.xz /tmp/frida-inject
    info "frida-inject installato in $INJECT"
fi

# ---- setup OJBS
OJBS_DIR="$(cd "$(dirname "$0")" && pwd)/ojbs-ue-android"
[ -d "$OJBS_DIR" ] || die "cartella ojbs-ue-android non trovata"

cd "$OJBS_DIR"

# set up config for local transport
cp config/target-43.11.env config/target.env 2>/dev/null || cp ../ojbs-ue-android/config/target-43.11.env config/target.env
cp config/watchlist.example.json config/watchlist.json 2>/dev/null || true

chmod +x bin/ojbs.sh install.sh

info "setup completato"
printf "\n"
printf "============================================\n"
printf "  PROSSIMI PASSI\n"
printf "============================================\n"
printf "\n"
printf "1. Apri Fortnite 43.11 e arriva alla lobby\n"
printf "\n"
printf "2. Torna su Termux e lancia:\n"
printf "   cd %s\n" "$OJBS_DIR"
printf "   bin/ojbs.sh setup\n"
printf "\n"
printf "3. Se setup va a buon fine, lancia il derive:\n"
printf "   bin/ojbs.sh derive\n"
printf "\n"
printf "4. Poi il dump:\n"
printf "   bin/ojbs.sh dump\n"
printf "\n"
printf "5. Controlla cosa manca:\n"
printf "   bin/ojbs.sh missing\n"
printf "\n"
printf "6. I risultati stanno in:\n"
printf "   out/derived.json    <- gli offset derivati\n"
printf "   out/offsets.json    <- le chain risolte\n"
printf "   out/classes.ndjson  <- il dump SDK completo\n"
printf "\n"
printf "7. Manda i file out/ a Onyx e lui\n"
printf "   riempie gli offset e ricompila il daemon.\n"
printf "============================================\n"
