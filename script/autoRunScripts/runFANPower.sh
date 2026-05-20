#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

FAN_BIN="${FAN_BIN:-./bin/opt/fan}"
NO_COMPRESSION=1
CIRCUITS=(
  s27
  s208
  s510
  s953
  s1196
  s1238
  s5378
  s9234
  s15850
  s35932
  s38417
  s38584
)

usage() {
  echo "Usage: $0 [--comp] [all|circuit ...]"
  echo "Examples:"
  echo "  $0 all"
  echo "  $0 --comp all"
  echo "  $0 s27"
  echo "  $0 s27 s208 s9234"
}

ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --comp)
      NO_COMPRESSION=0
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      ARGS+=("$@")
      break
      ;;
    -*)
      usage
      echo "ERROR: unknown option: $1" >&2
      exit 1
      ;;
    *)
      ARGS+=("$1")
      ;;
  esac
  shift
done
set -- "${ARGS[@]}"

if [[ ! -x "$FAN_BIN" ]]; then
  echo "ERROR: FAN binary not found or not executable: $FAN_BIN" >&2
  echo "Build it first, or set FAN_BIN=/path/to/fan." >&2
  exit 1
fi

if [[ $# -eq 0 || "${1:-}" == "all" ]]; then
  SELECTED=("${CIRCUITS[@]}")
else
  SELECTED=("$@")
fi

STATIC_COMPRESSION="on"
DYNAMIC_COMPRESSION="on"
if [[ "$NO_COMPRESSION" -eq 1 ]]; then
  STATIC_COMPRESSION="off"
  DYNAMIC_COMPRESSION="off"
fi

mkdir -p power_rpt power_pat
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/fan_power.XXXXXX")"
trap 'rm -rf "$TMP_DIR"' EXIT

for circuit in "${SELECTED[@]}"; do
  netlist="mod_netlist/${circuit}.v"
  if [[ ! -f "$netlist" ]]; then
    echo "ERROR: netlist not found: $netlist" >&2
    exit 1
  fi

  script_file="$TMP_DIR/atpg_${circuit}_power.script"
  cat > "$script_file" <<EOF
read_lib techlib/mod_nangate45.mdt
read_netlist ${netlist}
report_netlist
set_pattern_type LOC
build_circuit --frame 2
report_circuit
set_fault_type tdf
add_fault --all
set_static_compression ${STATIC_COMPRESSION}
set_dynamic_compression ${DYNAMIC_COMPRESSION}
set_X-Fill power
run_atpg -power
report_statistics > power_rpt/FAN_power_${circuit}.rpt
write_pattern power_pat/FAN_power_${circuit}.pat
write_to_STIL power_pat/FAN_power_${circuit}.stil
exit
EOF

  echo "[power] ${circuit}"
  "$FAN_BIN" -f "$script_file"
  echo "  report: power_rpt/FAN_power_${circuit}.rpt"
  echo "  pattern: power_pat/FAN_power_${circuit}.pat"
  echo "  stil: power_pat/FAN_power_${circuit}.stil"
done
