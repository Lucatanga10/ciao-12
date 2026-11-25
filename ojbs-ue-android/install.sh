#!/usr/bin/env bash
# OJBS-UE-ANDROID installer.
#
# Puts a matching frida-inject on the device and checks the host side.
# The frida version is never guessed: it is read from the frida tooling on
# this host, or taken from the first argument.
#
#   ./install.sh                 use the host's frida version
#   ./install.sh 17.2.3          pin a version explicitly
#
set -euo pipefail

OJBS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$OJBS_ROOT"

# shellcheck source=lib/log.sh
. lib/log.sh
CONFIG="${OJBS_CONFIG:-config/target.env}"
[ -f "$CONFIG" ] || die "missing $CONFIG"
# shellcheck disable=SC1090
. "$CONFIG"
# shellcheck source=lib/env.sh
. lib/env.sh

DEV_STAGE="/data/local/tmp/ojbs"
VERSION="${1:-}"

host_checks() {
  local missing=""
  for c in awk sed grep base64 curl; do
    ojbs_have "$c" || missing="$missing $c"
  done
  if [ "$OJBS_TRANSPORT" = "adb" ]; then ojbs_have adb || missing="$missing adb"; fi
  ojbs_have xz || log_warn "xz not found — you will have to unpack frida-inject by hand"
  [ -z "$missing" ] || die "missing host tools:$missing"
}

resolve_version() {
  if [ -n "$VERSION" ]; then printf '%s' "$VERSION"; return 0; fi
  local v
  v="$(host_frida_version)"
  [ -n "$v" ] || die "no frida on this host and no version argument — run: ./install.sh <version>  (see github.com/frida/frida/releases)"
  printf '%s' "$v"
}

install_inject() {
  local ver arch url tmp
  ver="$(resolve_version)"
  arch="$(dev_arch)"
  case "$arch" in
    arm64|arm|x86_64|x86) : ;;
    *) die "unsupported device arch: $arch" ;;
  esac
  url="https://github.com/frida/frida/releases/download/${ver}/frida-inject-${ver}-android-${arch}.xz"
  tmp="$(mktemp -d)"
  log_info "fetching frida-inject ${ver} for android-${arch}"
  curl -fsSL "$url" -o "$tmp/frida-inject.xz" || die "download failed: $url"
  xz -d "$tmp/frida-inject.xz" || die "could not unpack frida-inject.xz"
  dev_root "mkdir -p '$DEV_STAGE'; chmod 777 '$DEV_STAGE'" >/dev/null
  dev_push "$tmp/frida-inject" "$DEV_STAGE/frida-inject"
  dev_root "chmod 755 '$DEV_STAGE/frida-inject'; '$DEV_STAGE/frida-inject' --version" | tr -d '\r'
  rm -rf "$tmp"
  log_info "installed $DEV_STAGE/frida-inject"
}

main() {
  host_checks
  env_preflight
  log_kv "transport" "$OJBS_TRANSPORT"
  log_kv "device arch" "$(dev_arch)"
  log_kv "root" "$(dev_root 'id -u' | tr -d '\r')"
  local present=""
  if present="$(dev_frida_inject)"; then
    log_info "frida-inject already present: $present"
  else
    install_inject
  fi
  if [ ! -f config/watchlist.json ]; then
    cp config/watchlist.example.json config/watchlist.json
    log_info "created config/watchlist.json from the example"
  fi
  chmod +x bin/ojbs.sh install.sh 2>/dev/null || true
  log_info "next: bin/ojbs.sh setup && bin/ojbs.sh derive"
}

main "$@"
