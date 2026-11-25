# shellcheck shell=bash
# transport + privilege + tooling layer.
#
# Two transports, one interface:
#   adb    host with the phone attached over USB debugging
#   local  the scripts are running on the phone itself (Termux)
#
# Every device command goes through dev_sh (shell user) or dev_root (root).
# Command strings are base64-wrapped before they cross the transport so that
# quoting, globs and newlines survive adb and su untouched.

ojbs_have() { command -v "$1" >/dev/null 2>&1; }

OJBS_SU_RESOLVED=""

# ---------------------------------------------------------------- transport

_adb() {
  if [ -n "${OJBS_ADB_SERIAL:-}" ]; then
    adb -s "$OJBS_ADB_SERIAL" "$@"
  else
    adb "$@"
  fi
}

_b64() { printf '%s' "$1" | base64 | tr -d '\r\n'; }

# run as the shell user, output captured binary-safe
dev_sh() {
  local b64
  b64="$(_b64 "$1")"
  case "$OJBS_TRANSPORT" in
    adb)   _adb exec-out "echo $b64 | base64 -d | sh" ;;
    local) printf '%s' "$1" | sh ;;
    *)     log_err "unknown OJBS_TRANSPORT: $OJBS_TRANSPORT"; return 2 ;;
  esac
}

# resolve the su binary once
dev_su_bin() {
  if [ -n "$OJBS_SU_RESOLVED" ]; then printf '%s' "$OJBS_SU_RESOLVED"; return 0; fi
  if [ -n "${OJBS_SU:-}" ]; then OJBS_SU_RESOLVED="$OJBS_SU"; printf '%s' "$OJBS_SU_RESOLVED"; return 0; fi
  local c found=""
  for c in su /system/bin/su /system/xbin/su /sbin/su /debug_ramdisk/su; do
    if [ "$(dev_sh "command -v $c >/dev/null 2>&1 && echo yes || echo no")" = "yes" ]; then
      found="$c"; break
    fi
  done
  [ -n "$found" ] || return 1
  OJBS_SU_RESOLVED="$found"
  printf '%s' "$OJBS_SU_RESOLVED"
}

# run as root on the device, output captured binary-safe
dev_root() {
  local su b64
  su="$(dev_su_bin)" || { log_err "no su binary on the device — root is required for /proc access and for frida-inject"; return 127; }
  b64="$(_b64 "$1")"
  case "$OJBS_TRANSPORT" in
    adb)   _adb exec-out "echo $b64 | base64 -d | $su -c sh" ;;
    local) printf '%s' "$1" | "$su" -c sh ;;
    *)     log_err "unknown OJBS_TRANSPORT: $OJBS_TRANSPORT"; return 2 ;;
  esac
}

# run as root, detached, no output capture
dev_root_bg() {
  local su b64
  su="$(dev_su_bin)" || return 127
  b64="$(_b64 "$1")"
  case "$OJBS_TRANSPORT" in
    adb)   _adb shell "echo $b64 | base64 -d | $su -c sh" >/dev/null 2>&1 & ;;
    local) printf '%s' "$1" | "$su" -c sh >/dev/null 2>&1 & ;;
  esac
  return 0
}

dev_push() {
  local src="$1" dst="$2"
  case "$OJBS_TRANSPORT" in
    adb)   _adb push "$src" "$dst" >/dev/null ;;
    local) mkdir -p "$(dirname "$dst")" && cp -f "$src" "$dst" ;;
  esac
}

dev_exists() {
  [ "$(dev_root "[ -e '$1' ] && echo yes || echo no" | tr -d '\r\n')" = "yes" ]
}

# cat a device file to stdout as root, binary-safe
dev_cat() { dev_root "cat '$1'"; }

# ------------------------------------------------------------------- probes

dev_arch() {
  local m
  m="$(dev_sh "uname -m" | tr -d '\r\n')"
  case "$m" in
    aarch64|arm64) printf 'arm64' ;;
    armv7l|armv8l|arm) printf 'arm' ;;
    x86_64) printf 'x86_64' ;;
    i686|i386) printf 'x86' ;;
    *) printf '%s' "$m" ;;
  esac
}

dev_is_root_ok() {
  [ "$(dev_root "id -u" | tr -d '\r\n')" = "0" ]
}

# frida-inject path on the device, or empty
dev_frida_inject() {
  if [ -n "${OJBS_FRIDA_INJECT:-}" ]; then printf '%s' "$OJBS_FRIDA_INJECT"; return 0; fi
  local c
  for c in frida-inject /data/local/tmp/frida-inject /data/local/tmp/ojbs/frida-inject; do
    if [ "$(dev_root "command -v $c >/dev/null 2>&1 || [ -x $c ] && echo yes || echo no" | tr -d '\r\n')" = "yes" ]; then
      printf '%s' "$c"; return 0
    fi
  done
  return 1
}

host_frida_version() {
  if ojbs_have "${OJBS_FRIDA_CLI:-frida}"; then
    "${OJBS_FRIDA_CLI:-frida}" --version 2>/dev/null | tr -d '\r\n'
  fi
}

# ------------------------------------------------------------------- checks

env_preflight() {
  case "$OJBS_TRANSPORT" in
    adb)
      ojbs_have adb || die "adb not on PATH and OJBS_TRANSPORT=adb"
      local n
      n="$(_adb devices | awk 'NR>1 && $2=="device" {c++} END {print c+0}')"
      [ "$n" -ge 1 ] || die "no device in 'adb devices' state 'device' — check the cable and the USB-debugging prompt"
      [ "$n" -eq 1 ] || [ -n "${OJBS_ADB_SERIAL:-}" ] || die "$n devices attached — set OJBS_ADB_SERIAL"
      ;;
    local) : ;;
    *) die "OJBS_TRANSPORT must be adb or local" ;;
  esac
  ojbs_have base64 || die "base64 missing on this host"
  ojbs_have awk || die "awk missing on this host"
  dev_is_root_ok || die "su did not return uid 0 — grant root to the shell and retry"
}
