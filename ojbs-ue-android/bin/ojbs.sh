#!/usr/bin/env bash
# OJBS-UE-ANDROID — the glue.
#
#   ojbs setup              find the package, the pid and the engine module
#   ojbs status             print the live identity of the target
#   ojbs derive             find the name pool, the object array and the layout
#   ojbs dump               full reflection dump to out/classes.ndjson
#   ojbs resolve            turn config/watchlist.json into out/offsets.json
#   ojbs watch [frida|shellmem]
#   ojbs run script.js      inject your own agent with the toolkit preloaded
#   ojbs missing            list the values that still need finding by hand
#   ojbs clean
#
set -euo pipefail

OJBS_BIN="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OJBS_ROOT="$(cd "$OJBS_BIN/.." && pwd)"
cd "$OJBS_ROOT"

# shellcheck source=../lib/log.sh
. lib/log.sh

CONFIG="${OJBS_CONFIG:-config/target.env}"
[ -f "$CONFIG" ] || die "missing $CONFIG"
# shellcheck disable=SC1090
. "$CONFIG"
if [ -f config/derived.env ]; then
  # shellcheck disable=SC1091
  . config/derived.env
fi
# shellcheck source=../lib/env.sh
. lib/env.sh
# shellcheck source=../lib/proc.sh
. lib/proc.sh
# shellcheck source=../lib/mem.sh
. lib/mem.sh

PKG=""; PID=""; MODNAME=""; MODPATH=""; MODBASE=""; MODSIZE=""; DEVOUT=""; BUILDID=""
DEV_STAGE="/data/local/tmp/ojbs"

# ----------------------------------------------------------------- state

resolve_state() {
  env_preflight
  PKG="$(proc_resolve_package)" || die "no package matching '${OJBS_PKG_HINT}' — set OJBS_PACKAGE in $CONFIG"
  PID="$(proc_main_pid "$PKG")" || die "'$PKG' is not running — start it and get past the loading screen first"
  if [ -n "${UE_MODULE:-}" ]; then
    MODNAME="$UE_MODULE"
  else
    MODPATH="$(proc_detect_module "$PID")"
    [ -n "$MODPATH" ] || die "no .so mapped out of the package storage in pid $PID"
    MODNAME="${MODPATH##*/}"
  fi
  local ident
  ident="$(proc_module_identity "$PID" "$MODNAME")" || die "module '$MODNAME' not mapped in pid $PID"
  MODBASE="$(printf '%s' "$ident" | awk '{print $1}')"
  MODSIZE="$(printf '%s' "$ident" | awk '{print $2}')"
  MODPATH="$(printf '%s' "$ident" | awk '{print $3}')"
  DEVOUT="${OJBS_DEVICE_OUT//__PKG__/$PKG}"
  BUILDID="${UE_BUILD_ID:-}"
  [ -n "$BUILDID" ] || BUILDID="$(proc_build_id "$PKG" "$PID" "$MODNAME")"
  mkdir -p "$OJBS_OUT_DIR" "$OJBS_WORK_DIR"
}

stale_check() {
  [ -n "${UE_MODULE_SIZE:-}" ] || return 0
  [ "$UE_MODULE_SIZE" = "$MODSIZE" ] && return 0
  log_warn "module span changed: config says $UE_MODULE_SIZE, the process maps $MODSIZE"
  log_warn "every stored offset is now a hypothesis — rerun 'ojbs derive' before trusting out/offsets.json"
}

# ----------------------------------------------------------------- agent

json_watchlist() {
  if [ -f "${OJBS_WATCHLIST}" ]; then
    cat "${OJBS_WATCHLIST}"
  else
    printf '{"entries":[]}'
  fi
}

num_or() { if [ -n "${1:-}" ]; then printf '%s' "$1"; else printf '%s' "$2"; fi; }

build_agent() {
  local mode="$1" user="${2:-}"
  local out="$OJBS_WORK_DIR/agent.js"
  mkdir -p "$OJBS_WORK_DIR"
  {
    printf "'use strict';\n"
    printf 'var OJBS = {\n'
    printf '  mode: "%s",\n' "$mode"
    printf '  out_dir: "%s",\n' "$DEVOUT"
    printf '  target: { package: "%s", module: "%s", build_id: "%s", base: "%s", size: %s },\n' \
      "$PKG" "$MODNAME" "$BUILDID" "$MODBASE" "$MODSIZE"
    printf '  limits: { min_pointer: "%s", max_pointer: "%s", max_abs_float: %s },\n' \
      "$UE_MIN_POINTER" "$UE_MAX_POINTER" "$UE_MAX_ABS_FLOAT"
    printf '  scan: { all_ranges: %s, stride: %s, max_hits: %s },\n' \
      "$( [ "${UE_SCAN_ALL_RANGES}" = "1" ] && echo true || echo false )" \
      "$UE_SCAN_STRIDE" "$UE_SCAN_MAX_HITS"
    printf '  derive: { samples: %s, max_field_offset: "%s" },\n' \
      "$UE_DERIVE_SAMPLES" "$UE_DERIVE_MAX_FIELD_OFFSET"
    printf '  dump: { max_classes: %s, max_props: %s, class_filter: "%s" },\n' \
      "$UE_DUMP_MAX_CLASSES" "$UE_DUMP_MAX_PROPS" "$UE_DUMP_CLASS_FILTER"
    printf '  watch: { period_ms: %s, duration_s: %s },\n' \
      "$OJBS_WATCH_PERIOD_MS" "$OJBS_WATCH_DURATION_S"
    printf '  offsets: {\n'
    printf '    fnamepool_blocks_rva: "%s",\n' "${UE_FNAMEPOOL_BLOCKS_RVA}"
    printf '    fname_block_bits: %s,\n' "$(num_or "${UE_FNAME_BLOCK_BITS}" 16)"
    printf '    gobjects_rva: "%s",\n' "${UE_GOBJECTS_RVA}"
    printf '    fuobjectitem_size: %s,\n' "$(num_or "${UE_FUOBJECTITEM_SIZE}" 0)"
    printf '    gworld_rva: "%s",\n' "${UE_GWORLD_RVA}"
    printf '    uobject_index: "%s",\n' "${UE_OFF_UOBJECT_INDEX}"
    printf '    uobject_class: "%s",\n' "${UE_OFF_UOBJECT_CLASS}"
    printf '    uobject_name: "%s",\n' "${UE_OFF_UOBJECT_NAME}"
    printf '    uobject_outer: "%s",\n' "${UE_OFF_UOBJECT_OUTER}"
    printf '    ustruct_super: "%s",\n' "${UE_OFF_USTRUCT_SUPER}"
    printf '    ustruct_childprops: "%s",\n' "${UE_OFF_USTRUCT_CHILDPROPS}"
    printf '    ustruct_children: "%s",\n' "${UE_OFF_USTRUCT_CHILDREN}"
    printf '    ustruct_size: "%s",\n' "${UE_OFF_USTRUCT_SIZE}"
    printf '    ffield_next: "%s",\n' "${UE_OFF_FFIELD_NEXT}"
    printf '    ffield_name: "%s",\n' "${UE_OFF_FFIELD_NAME}"
    printf '    fproperty_offset: "%s",\n' "${UE_OFF_FPROPERTY_OFFSET}"
    printf '    fproperty_elemsize: "%s",\n' "${UE_OFF_FPROPERTY_ELEMSIZE}"
    printf '    fstructprop_struct: "%s",\n' "${UE_OFF_FSTRUCTPROP_STRUCT:-}"
    printf '    farrayprop_inner: "%s"\n' "${UE_OFF_FARRAYPROP_INNER:-}"
    printf '  },\n'
    printf '  watchlist: '
    json_watchlist
    printf '\n};\n'
    local f
    for f in agent/00-compat.js agent/10-module.js agent/20-read.js agent/30-fname.js \
             agent/40-objects.js agent/50-reflect.js agent/60-world.js agent/70-export.js \
             agent/80-dump.js agent/85-watch.js; do
      printf '\n// ---- %s\n' "$f"
      cat "$f"
    done
    if [ -n "$user" ]; then
      printf '\n// ---- user script: %s\n' "$user"
      cat "$user"
    fi
    printf '\n// ---- agent/90-main.js\n'
    cat agent/90-main.js
  } > "$out"
  printf '%s' "$out"
}

prepare_device_out() {
  dev_root "mkdir -p '$DEVOUT' '$DEV_STAGE'
own=\$(stat -c %u:%g /data/data/$PKG 2>/dev/null || echo 0:0)
chown \$own '$DEVOUT' 2>/dev/null
chmod 777 '$DEVOUT' '$DEV_STAGE' 2>/dev/null
restorecon -R '$DEVOUT' 2>/dev/null
rm -f '$DEVOUT'/done.json '$DEVOUT'/derived.json '$DEVOUT'/offsets.json '$DEVOUT'/chains.txt '$DEVOUT'/classes.ndjson '$DEVOUT'/samples.ndjson
echo ok" >/dev/null
}

inject_agent() {
  local script="$1" timeout="$2"
  local injector=""
  if injector="$(dev_frida_inject)"; then
    log_info "driver: frida-inject ($injector)"
    dev_push "$script" "$DEV_STAGE/agent.js"
    dev_root "chmod 644 '$DEV_STAGE/agent.js'" >/dev/null
    dev_root_bg "$injector -p $PID -s '$DEV_STAGE/agent.js' > '$DEV_STAGE/agent.log' 2>&1"
  elif ojbs_have "${OJBS_FRIDA_CLI}"; then
    log_info "driver: frida CLI against $OJBS_FRIDA_HOST:$OJBS_FRIDA_PORT (frida-server must be listening there)"
    ( sleep "$timeout" | "${OJBS_FRIDA_CLI}" -H "$OJBS_FRIDA_HOST:$OJBS_FRIDA_PORT" -p "$PID" -l "$script" -q \
        > "$OJBS_OUT_DIR/agent.log" 2>&1 ) &
  else
    die "no injector: put frida-inject on the device (see install.sh) or frida on this host"
  fi

  local waited=0
  while [ "$waited" -lt "$timeout" ]; do
    if dev_exists "$DEVOUT/done.json"; then
      log_info "agent reported done after ${waited}s"
      return 0
    fi
    sleep 2
    waited=$(( waited + 2 ))
    if [ $(( waited % 20 )) -eq 0 ]; then log_info "waiting on the agent (${waited}s/${timeout}s)"; fi
  done
  log_warn "agent did not finish inside ${timeout}s — pulling whatever it wrote"
  return 1
}

pull_results() {
  local f
  for f in derived.json offsets.json chains.txt classes.ndjson samples.ndjson user-actors.ndjson done.json; do
    if dev_exists "$DEVOUT/$f"; then
      dev_cat "$DEVOUT/$f" > "$OJBS_OUT_DIR/$f"
      log_kv "$f" "$(wc -c < "$OJBS_OUT_DIR/$f" | tr -d ' ') bytes"
    fi
  done
  if dev_exists "$DEV_STAGE/agent.log"; then
    dev_cat "$DEV_STAGE/agent.log" > "$OJBS_OUT_DIR/agent.log" || true
  fi
}

# ---------------------------------------------------------------- commands

cmd_setup() {
  resolve_state
  cat > config/derived.env <<EOF
# written by 'ojbs setup' on $(date -u +%Y-%m-%dT%H:%M:%SZ)
# [observed from the live process — not a guess, not portable to another build]
OJBS_PACKAGE="$PKG"
UE_MODULE="$MODNAME"
UE_MODULE_SIZE="$MODSIZE"
UE_BUILD_ID="$BUILDID"
EOF
  log_info "wrote config/derived.env"
  cmd_status
}

cmd_status() {
  [ -n "$PKG" ] || resolve_state
  stale_check
  log_info "target"
  log_kv "package" "$PKG"
  log_kv "pid" "$PID"
  log_kv "module" "$MODNAME"
  log_kv "module path" "$MODPATH"
  log_kv "base" "$MODBASE"
  log_kv "size" "$MODSIZE"
  log_kv "build id" "$BUILDID"
  log_kv "device out" "$DEVOUT"
  log_kv "arch" "$(dev_arch)"
  log_kv "mem backend" "$(mem_detect_backend || echo none)"
  local injector=""
  if injector="$(dev_frida_inject)"; then log_kv "injector" "$injector"; else log_kv "injector" "absent — run install.sh"; fi
}

cmd_derive() {
  resolve_state
  stale_check
  local script
  script="$(build_agent derive)"
  log_info "agent built: $script ($(wc -l < "$script" | tr -d ' ') lines)"
  prepare_device_out
  inject_agent "$script" "$OJBS_DERIVE_TIMEOUT" || true
  pull_results
  cmd_missing || true
}

cmd_dump() {
  resolve_state
  stale_check
  local script
  script="$(build_agent dump)"
  prepare_device_out
  inject_agent "$script" "$OJBS_DUMP_TIMEOUT" || true
  pull_results
  if [ -f "$OJBS_OUT_DIR/classes.ndjson" ]; then
    log_info "classes dumped: $(wc -l < "$OJBS_OUT_DIR/classes.ndjson" | tr -d ' ')"
  fi
}

cmd_resolve() {
  resolve_state
  stale_check
  [ -f "$OJBS_WATCHLIST" ] || die "no $OJBS_WATCHLIST — copy config/watchlist.example.json and edit it"
  local script
  script="$(build_agent resolve)"
  prepare_device_out
  inject_agent "$script" "$OJBS_DERIVE_TIMEOUT" || true
  pull_results
  if [ -f "$OJBS_OUT_DIR/chains.txt" ]; then
    log_info "replayable chains:"
    while IFS='|' read -r name kind rva type ops; do
      [ -n "$name" ] || continue
      log_kv "$name" "$kind+$rva $type [$ops]"
    done < "$OJBS_OUT_DIR/chains.txt"
  fi
}

cmd_watch() {
  local driver="${1:-$OJBS_WATCH_DRIVER}"
  resolve_state
  stale_check
  case "$driver" in
    frida)
      local script
      script="$(build_agent watch)"
      prepare_device_out
      inject_agent "$script" "$(( OJBS_WATCH_DURATION_S + 60 ))" || true
      pull_results
      ;;
    shellmem)
      [ -f "$OJBS_OUT_DIR/chains.txt" ] || die "no out/chains.txt — run 'ojbs resolve' first"
      watch_shellmem
      ;;
    *) die "watch driver must be frida or shellmem" ;;
  esac
}

# replay the resolved chains from outside the process. nothing is injected;
# every hop is a /proc/<pid>/mem read. the achievable period is whatever the
# transport allows, so the real timestamp is recorded per line.
watch_shellmem() {
  local out="$OJBS_OUT_DIR/samples.ndjson"
  : > "$out"
  printf '{"meta":true,"driver":"shellmem","started":"%s","assumption":"asynchronous reads, no tick alignment, one syscall per hop"}\n' \
    "$(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$out"
  local deadline=$(( $(date +%s) + OJBS_WATCH_DURATION_S ))
  log_info "sampling for ${OJBS_WATCH_DURATION_S}s into $out"
  while [ "$(date +%s)" -lt "$deadline" ]; do
    local line='{"t":'"$(date +%s)"',"values":{'
    local first=1
    while IFS='|' read -r name kind rva type ops; do
      [ -n "$name" ] || continue
      local addr val
      addr="$(replay_chain "$PID" "$MODBASE" "$rva" "$ops")" || addr=""
      if [ -n "$addr" ]; then
        val="$(mem_read "$PID" "$addr" "$type" 2>/dev/null)" || val=""
      else
        val=""
      fi
      [ "$first" -eq 1 ] || line="$line,"
      first=0
      if [ -n "$val" ]; then
        line="$line\"$name\":\"$val\""
      else
        line="$line\"$name\":null"
      fi
    done < "$OJBS_OUT_DIR/chains.txt"
    printf '%s}}\n' "$line" >> "$out"
    sleep "$(awk -v ms="$OJBS_WATCH_PERIOD_MS" 'BEGIN{printf "%.3f", ms/1000}')"
  done
  log_info "wrote $(wc -l < "$out" | tr -d ' ') lines"
}

# replay_chain <pid> <modbase> <rva> <ops>
replay_chain() {
  local pid="$1" base="$2" rva="$3" ops="$4"
  local addr=$(( base + rva ))
  local op v idx stride
  local IFS=','
  for op in $ops; do
    case "$op" in
      add:*)
        addr=$(( addr + ${op#add:} ))
        ;;
      deref)
        v="$(mem_read "$pid" "$addr" ptr)" || return 1
        mem_plausible_ptr "$v" || return 1
        addr=$(( v ))
        ;;
      array:*)
        idx="$(printf '%s' "$op" | cut -d: -f2)"
        stride="$(printf '%s' "$op" | cut -d: -f3)"
        v="$(mem_read "$pid" "$addr" ptr)" || return 1
        mem_plausible_ptr "$v" || return 1
        addr=$(( v + idx * stride ))
        ;;
      *) return 1 ;;
    esac
  done
  printf '0x%x' "$addr"
}

cmd_run() {
  local user="${1:-}"
  [ -n "$user" ] || die "usage: ojbs run <script.js>  (define ojbs_user_main(api) in it)"
  [ -f "$user" ] || die "no such script: $user"
  resolve_state
  stale_check
  local script
  script="$(build_agent run "$user")"
  log_info "agent + user script built: $script"
  prepare_device_out
  inject_agent "$script" "${OJBS_RUN_TIMEOUT:-120}" || true
  pull_results
}

cmd_missing() {
  local f="$OJBS_OUT_DIR/derived.json"
  if [ ! -f "$f" ]; then
    log_warn "no $f — the agent wrote nothing; check $OJBS_OUT_DIR/agent.log"
    return 0
  fi
  log_info "values this build still needs:"
  awk '
    /"key"/ { key = $0; sub(/.*"key"[^"]*"/, "", key); sub(/".*/, "", key); k = key; next }
    /"what"/ && k != "" { w = $0; sub(/.*"what"[^"]*"/, "", w); sub(/".*/, "", w); printf "    %-30s %s\n", k, w; k = "" }
  ' "$f" >&2
  if ! grep -q '"key"' "$f"; then
    log_info "nothing missing — every global and offset was derived"
  fi
}

cmd_clean() {
  rm -rf "$OJBS_WORK_DIR" "$OJBS_OUT_DIR"
  log_info "removed $OJBS_WORK_DIR and $OJBS_OUT_DIR"
}

usage() {
  sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
}

main() {
  local cmd="${1:-}"
  shift || true
  case "$cmd" in
    setup)   cmd_setup "$@" ;;
    status)  cmd_status "$@" ;;
    derive)  cmd_derive "$@" ;;
    dump)    cmd_dump "$@" ;;
    resolve) cmd_resolve "$@" ;;
    watch)   cmd_watch "$@" ;;
    run)     cmd_run "$@" ;;
    missing) cmd_missing "$@" ;;
    clean)   cmd_clean "$@" ;;
    ""|-h|--help|help) usage ;;
    *) log_err "unknown command: $cmd"; usage; exit 2 ;;
  esac
}

main "$@"
