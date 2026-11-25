# shellcheck shell=bash
# external memory reader: /proc/<pid>/mem through a root shell.
# nothing is injected into the target by this path.
#
# three byte-pump backends are probed once and the first working one is kept.
# all decoding happens here on the host in awk, so the device needs nothing
# beyond a root shell and one of dd / od / busybox hexdump.

OJBS_MEM_BACKEND=""

mem_detect_backend() {
  [ -n "$OJBS_MEM_BACKEND" ] && { printf '%s' "$OJBS_MEM_BACKEND"; return 0; }
  local probe
  probe="$(dev_root "dd if=/proc/self/maps bs=1 count=4 iflag=skip_bytes skip=0 2>/dev/null | od -An -tx1 2>/dev/null | head -n1")"
  if [ -n "$(printf '%s' "$probe" | tr -d ' \r\n')" ]; then OJBS_MEM_BACKEND="dd"; printf 'dd'; return 0; fi
  probe="$(dev_root "od -An -tx1 -N4 -j0 /proc/self/maps 2>/dev/null | head -n1")"
  if [ -n "$(printf '%s' "$probe" | tr -d ' \r\n')" ]; then OJBS_MEM_BACKEND="od"; printf 'od'; return 0; fi
  probe="$(dev_root "busybox hexdump -s 0 -n 4 -e '16/1 \"%02x \"' /proc/self/maps 2>/dev/null | head -n1")"
  if [ -n "$(printf '%s' "$probe" | tr -d ' \r\n')" ]; then OJBS_MEM_BACKEND="busybox"; printf 'busybox'; return 0; fi
  return 1
}

# mem_read_hex <pid> <addr-decimal-or-0x> <len> -> "aa bb cc ..."
mem_read_hex() {
  local pid="$1" addr="$2" len="$3" off be out
  case "$addr" in
    0x*|0X*) off="$(( addr ))" ;;
    *)       off="$addr" ;;
  esac
  be="$(mem_detect_backend)" || { log_err "no usable byte pump on the device (dd/od/busybox)"; return 1; }
  case "$be" in
    dd)      out="$(dev_root "dd if=/proc/$pid/mem bs=1 count=$len iflag=skip_bytes skip=$off 2>/dev/null | od -An -tx1")" ;;
    od)      out="$(dev_root "od -An -tx1 -N$len -j$off /proc/$pid/mem 2>/dev/null")" ;;
    busybox) out="$(dev_root "busybox hexdump -s $off -n $len -e '16/1 \"%02x \"' /proc/$pid/mem 2>/dev/null")" ;;
  esac
  out="$(printf '%s' "$out" | tr -d '\r' | tr '\n' ' ' | tr -s ' ' | sed 's/^ *//; s/ *$//')"
  # a partial read is a failed read, never a zero
  local n
  n="$(printf '%s' "$out" | tr ' ' '\n' | sed '/^$/d' | wc -l | tr -d ' ')"
  [ "$n" -eq "$len" ] || return 1
  printf '%s' "$out"
}

mem_type_size() {
  case "$1" in
    i8|u8|bool) printf 1 ;;
    i16|u16)    printf 2 ;;
    i32|u32|f32) printf 4 ;;
    i64|u64|f64|ptr) printf 8 ;;
    vec2)       printf 8 ;;
    vec3)       printf 12 ;;
    vec4)       printf 16 ;;
    bytes:*)    printf '%s' "${1#bytes:}" ;;
    *)          return 1 ;;
  esac
}

# mem_decode <type> "<hex bytes>" -> decoded value on stdout
# little endian. 64-bit quantities are printed as hex to avoid the double
# rounding that awk would otherwise inflict on them.
mem_decode() {
  local type="$1" hex="$2"
  printf '%s' "$hex" | awk -v type="$type" '
    function hv(c,   d) { d = index("0123456789abcdef", tolower(c)); return d - 1 }
    function byte(i,   s) { s = b[i]; return hv(substr(s,1,1)) * 16 + hv(substr(s,2,1)) }
    function uint(off, n,   i, v) { v = 0; for (i = n - 1; i >= 0; i--) v = v * 256 + byte(off + i); return v }
    function sint(off, n,   v, lim) { v = uint(off, n); lim = 2 ^ (n * 8 - 1); if (v >= lim) v -= 2 * lim; return v }
    function hexstr(off, n,   i, s) { s = ""; for (i = n - 1; i >= 0; i--) s = s b[off + i]; return "0x" s }
    function f32(off,   v, s, e, m, r) {
      v = uint(off, 4)
      s = int(v / 2147483648) % 2
      e = int(v / 8388608) % 256
      m = v % 8388608
      if (e == 255) return (m == 0 ? (s ? "-inf" : "inf") : "nan")
      if (e == 0) return (s ? "-0" : "0")
      r = (1 + m / 8388608) * 2 ^ (e - 127)
      return (s ? -r : r)
    }
    function f64(off,   v, hi, lo, s, e, m, r) {
      hi = uint(off + 4, 4); lo = uint(off, 4)
      s = int(hi / 2147483648) % 2
      e = int(hi / 1048576) % 2048
      m = (hi % 1048576) * 4294967296 + lo
      if (e == 2047) return (m == 0 ? (s ? "-inf" : "inf") : "nan")
      if (e == 0) return (s ? "-0" : "0")
      r = (1 + m / 4503599627370496) * 2 ^ (e - 1023)
      return (s ? -r : r)
    }
    {
      n = split($0, b, " ")
      for (i = 1; i <= n; i++) if (b[i] == "") { n = i - 1; break }
      # re-index from 0
      for (i = 0; i < n; i++) b[i] = b[i + 1]
    }
    END {
      if (type == "i8")   { print sint(0, 1); exit }
      if (type == "u8")   { print uint(0, 1); exit }
      if (type == "bool") { print (uint(0, 1) ? "true" : "false"); exit }
      if (type == "i16")  { print sint(0, 2); exit }
      if (type == "u16")  { print uint(0, 2); exit }
      if (type == "i32")  { print sint(0, 4); exit }
      if (type == "u32")  { print uint(0, 4); exit }
      if (type == "f32")  { print f32(0); exit }
      if (type == "f64")  { print f64(0); exit }
      if (type == "i64" || type == "u64" || type == "ptr") { print hexstr(0, 8); exit }
      if (type == "vec2") { printf "%s,%s\n", f32(0), f32(4); exit }
      if (type == "vec3") { printf "%s,%s,%s\n", f32(0), f32(4), f32(8); exit }
      if (type == "vec4") { printf "%s,%s,%s,%s\n", f32(0), f32(4), f32(8), f32(12); exit }
      s = ""
      for (i = 0; i < n; i++) s = s b[i]
      print s
    }'
}

# mem_read <pid> <addr> <type> -> decoded value, or non-zero on a failed read
mem_read() {
  local pid="$1" addr="$2" type="$3" size hex
  size="$(mem_type_size "$type")" || { log_err "unknown type: $type"; return 2; }
  hex="$(mem_read_hex "$pid" "$addr" "$size")" || return 1
  mem_decode "$type" "$hex"
}

# plausibility gate shared with the agent, applied host-side
mem_plausible_ptr() {
  local v="$1" n
  case "$v" in 0x*) ;; *) return 1 ;; esac
  n="$(( v ))"
  [ "$n" -ge "$(( ${UE_MIN_POINTER:-0x10000} ))" ] || return 1
  [ "$n" -le "$(( ${UE_MAX_POINTER:-0x7fffffffffff} ))" ] || return 1
  return 0
}
