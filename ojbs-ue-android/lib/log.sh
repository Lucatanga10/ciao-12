# shellcheck shell=bash
# logging for the OJBS toolkit. sourced, never executed.

if [ -t 2 ] && [ -z "${OJBS_NO_COLOR:-}" ]; then
  OJBS_C_RED=$'\033[31m'
  OJBS_C_YEL=$'\033[33m'
  OJBS_C_GRN=$'\033[32m'
  OJBS_C_DIM=$'\033[2m'
  OJBS_C_OFF=$'\033[0m'
else
  OJBS_C_RED=""
  OJBS_C_YEL=""
  OJBS_C_GRN=""
  OJBS_C_DIM=""
  OJBS_C_OFF=""
fi

log_info() { printf '%s[*]%s %s\n' "$OJBS_C_GRN" "$OJBS_C_OFF" "$*" >&2; }
log_warn() { printf '%s[!]%s %s\n' "$OJBS_C_YEL" "$OJBS_C_OFF" "$*" >&2; }
log_err()  { printf '%s[x]%s %s\n' "$OJBS_C_RED" "$OJBS_C_OFF" "$*" >&2; }
log_dbg()  { [ -n "${OJBS_DEBUG:-}" ] && printf '%s[.] %s%s\n' "$OJBS_C_DIM" "$*" "$OJBS_C_OFF" >&2; return 0; }

die() { log_err "$*"; exit 1; }

# key/value status line, aligned
log_kv() { printf '    %-22s %s\n' "$1" "$2" >&2; }
