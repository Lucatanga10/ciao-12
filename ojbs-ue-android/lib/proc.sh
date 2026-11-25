# shellcheck shell=bash
# process and module identity. every value here is read off the live device,
# none of it is assumed.

# resolve the package name by hint, or confirm the configured one
proc_resolve_package() {
  if [ -n "${OJBS_PACKAGE:-}" ]; then
    local hit
    hit="$(dev_sh "pm list packages $OJBS_PACKAGE" | tr -d '\r' | sed -n 's/^package://p' | head -n1)"
    if [ -z "$hit" ]; then
      log_warn "configured package '$OJBS_PACKAGE' not reported by pm — using it anyway"
      printf '%s' "$OJBS_PACKAGE"; return 0
    fi
    printf '%s' "$hit"; return 0
  fi
  local list
  list="$(dev_sh "pm list packages" | tr -d '\r' | sed -n 's/^package://p' | grep -i -- "${OJBS_PKG_HINT:-fortnite}" || true)"
  [ -n "$list" ] || return 1
  printf '%s' "$(printf '%s\n' "$list" | head -n1)"
}

# all pids whose cmdline starts with the package name
proc_pids() {
  local pkg="$1" out
  out="$(dev_root "pidof '$pkg' 2>/dev/null" | tr -d '\r' | tr ' ' '\n' | sed '/^$/d')"
  if [ -z "$out" ]; then
    out="$(dev_root "ps -A -o PID,NAME 2>/dev/null || ps -A" | tr -d '\r' | awk -v p="$pkg" '$2==p || $NF==p {print $1}' | sed '/^$/d')"
  fi
  [ -n "$out" ] || return 1
  printf '%s\n' "$out"
}

# the pid that owns the engine module; falls back to the first pid
proc_main_pid() {
  local pkg="$1" pids p
  pids="$(proc_pids "$pkg")" || return 1
  for p in $pids; do
    if [ -n "$(dev_root "grep -c -- '\.so' /proc/$p/maps 2>/dev/null" | tr -d '\r')" ]; then
      if [ "$(dev_root "grep -c -- 'libUE\|libUnreal\|lib${pkg##*.}' /proc/$p/maps 2>/dev/null || echo 0" | tr -d '\r')" != "0" ]; then
        printf '%s' "$p"; return 0
      fi
    fi
  done
  printf '%s' "$(printf '%s\n' "$pids" | head -n1)"
}

# largest .so mapped out of the package's own lib dir: the engine module
proc_detect_module() {
  local pid="$1"
  dev_root "cat /proc/$pid/maps" | tr -d '\r' | awk '
    {
      path = $NF
      if (path !~ /\.so$/) next
      if (path !~ /\/data\// && path !~ /\/mnt\// && path !~ /\/storage\//) next
      split($1, r, "-")
      lo = r[1]; hi = r[2]
      if (!(path in minlo)) minlo[path] = lo
      maxhi[path] = hi
    }
    END {
      for (p in maxhi) {
        n = length(maxhi[p])
        # textual hex compare is enough to rank spans of the same width
        printf "%s %s %s\n", minlo[p], maxhi[p], p
      }
    }' | while read -r lo hi path; do
      printf '%d %s\n' "$(( 16#$hi - 16#$lo ))" "$path"
    done | sort -rn | head -n1 | awk '{print $2}'
}

# "base size path" for a module substring inside a pid
proc_module_identity() {
  local pid="$1" mod="$2" line a b lo="" hi="" path=""
  while IFS= read -r line; do
    line="${line%$'\r'}"
    case "$line" in *"$mod"*) ;; *) continue ;; esac
    a="${line%% *}"
    b="${a##*-}"
    a="${a%%-*}"
    path="${line##* }"
    if [ -z "$lo" ] || [ "$((16#$a))" -lt "$((16#$lo))" ]; then lo="$a"; fi
    if [ -z "$hi" ] || [ "$((16#$b))" -gt "$((16#$hi))" ]; then hi="$b"; fi
  done <<EOF
$(dev_root "cat /proc/$pid/maps")
EOF
  [ -n "$lo" ] || return 1
  printf '0x%s %d %s\n' "$lo" "$(( 16#$hi - 16#$lo ))" "$path"
}

# build identity: version from the package manager plus the module's own size
proc_build_id() {
  local pkg="$1" pid="$2" mod="$3" vname vcode size
  vname="$(dev_sh "dumpsys package $pkg | grep -m1 versionName" | tr -d '\r' | sed 's/.*versionName=//' | awk '{print $1}')"
  vcode="$(dev_sh "dumpsys package $pkg | grep -m1 versionCode" | tr -d '\r' | sed 's/.*versionCode=//' | awk '{print $1}')"
  size="$(proc_module_identity "$pid" "$mod" | awk '{print $2}')"
  printf '%s-%s-%s' "${vname:-unknownver}" "${vcode:-unknowncode}" "${size:-0}"
}
