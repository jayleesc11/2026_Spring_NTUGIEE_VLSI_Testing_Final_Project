#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

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

TMAX_SETUP="${TMAX_SETUP:-/work/cad/synopsys/CIC/tmax.cshrc}"
TMAX_BIN="${TMAX_BIN:-tmax}"
PYTHON="${PYTHON:-python3}"
FAN_BIN="${FAN_BIN:-./bin/opt/fan}"
ATPG_MODE="${ATPG_MODE:-power}"
TMP_FILES=()

cleanup() {
  if [[ ${#TMP_FILES[@]} -gt 0 ]]; then
    rm -f "${TMP_FILES[@]}"
  fi
}
trap cleanup EXIT

usage() {
  cat <<USAGE
Usage:
  $0 [--mode normal|power] <circuit|all>
  $0 --normal <circuit|all>
  $0 --power-aware <circuit|all>

Circuits:
  ${CIRCUITS[*]}

Modes:
  normal       TestMAX normal ATPG: set_atpg -fill 0
  power        TestMAX power-aware ATPG: adjacent fill + shift power effort

Environment overrides:
  TMAX_SETUP=/path/to/tmax.cshrc
  TMAX_BIN=tmax
  PYTHON=python3
  FAN_BIN=./bin/opt/fan
  ATPG_MODE=normal|power
USAGE
}

die() {
  echo "error: $*" >&2
  exit 1
}

contains_circuit() {
  local query="$1"
  local circuit
  for circuit in "${CIRCUITS[@]}"; do
    [[ "${circuit}" == "${query}" ]] && return 0
  done
  return 1
}

write_tmax_script() {
  local circuit="$1"
  local script
  local netlist="mod_netlist/${circuit}.v"

  [[ -f "${netlist}" ]] || die "missing ${netlist}"
  script="$(mktemp "${TMPDIR:-/tmp}/tmax_${circuit}.tcl.XXXXXX")"
  TMP_FILES+=("${script}")
  cat > "${script}" <<TCL
set_messages -log tmax/rpt/${circuit}_tmax.log -replace
read_netlist techlib/NangateOpenCellLibrary.v -format verilog -define TETRAMAX -library
read_netlist mod_netlist/${circuit}.v -format verilog
run_build_model ${circuit}
add_clocks 0 CK -shift
add_scan_enables 1 test_se
add_scan_chains chain1 test_si test_so
run_drc

TCL

  case "${ATPG_MODE}" in
    normal)
      cat >> "${script}" <<TCL
puts "ATPG mode: normal"
set_atpg -fill 0

TCL
      ;;
    power)
      cat >> "${script}" <<TCL
puts "ATPG mode: power-aware"
set_atpg -fill adjacent
set_atpg -shift_power_effort high -shift_power_budget 100

TCL
      ;;
  esac

  cat >> "${script}" <<TCL
add_faults -all
run_atpg -auto_compression
report_power -per_pattern -percentage
write_patterns tmax/pat/${circuit}.stil -format stil -replace
exit
TCL

  TMAX_SCRIPT="${script}"
}

write_fsim_script() {
  local circuit="$1"
  local script

  script="$(mktemp "${TMPDIR:-/tmp}/fsim_${circuit}.script.XXXXXX")"
  TMP_FILES+=("${script}")
  cat > "${script}" <<TCL
read_lib techlib/mod_nangate45.mdt
read_netlist mod_netlist/${circuit}.v
build_circuit --frame 1
read_pattern tmax/pat/${circuit}.pat
set_fault_type saf
add_fault -a
run_fault_sim
report_statistics > tmax/rpt/${circuit}_fsim.rpt
exit
TCL

  FSIM_SCRIPT="${script}"
}

run_one() {
  local circuit="$1"
  local tmax_script
  local fsim_script
  local stil="tmax/pat/${circuit}.stil"
  local pat="tmax/pat/${circuit}.pat"

  [[ -x "${FAN_BIN}" ]] || die "missing executable FAN binary: ${FAN_BIN}. Run make first or set FAN_BIN."
  write_tmax_script "${circuit}"
  tmax_script="${TMAX_SCRIPT}"
  write_fsim_script "${circuit}"
  fsim_script="${FSIM_SCRIPT}"

  echo "==> [${circuit}] TestMAX ATPG (${ATPG_MODE})"
  tcsh -f -c "source ${TMAX_SETUP}; ${TMAX_BIN} -shell ${tmax_script}"

  echo "==> [${circuit}] Convert STIL to FAN PAT"
  "${PYTHON}" tmax/utility/scan_stil_to_fan_pat.py "${stil}" --out "${pat}"

  echo "==> [${circuit}] FAN fault simulation"
  "${FAN_BIN}" -f "${fsim_script}"
}

main() {
  local args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --mode)
        shift
        [[ $# -gt 0 ]] || die "--mode requires normal or power"
        ATPG_MODE="$1"
        ;;
      --normal)
        ATPG_MODE="normal"
        ;;
      --power|--power-aware)
        ATPG_MODE="power"
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      --)
        shift
        args+=("$@")
        break
        ;;
      -*)
        usage
        die "unknown option: $1"
        ;;
      *)
        args+=("$1")
        ;;
    esac
    shift
  done

  case "${ATPG_MODE}" in
    normal|power)
      ;;
    power-aware)
      ATPG_MODE="power"
      ;;
    *)
      usage
      die "unknown ATPG mode: ${ATPG_MODE}"
      ;;
  esac

  set -- "${args[@]}"
  [[ $# -eq 1 ]] || {
    usage
    exit 1
  }

  cd "${ROOT_DIR}"
  mkdir -p tmax/pat tmax/rpt

  local target="$1"
  if [[ "${target}" == "all" ]]; then
    local circuit
    for circuit in "${CIRCUITS[@]}"; do
      run_one "${circuit}"
    done
  elif contains_circuit "${target}"; then
    run_one "${target}"
  else
    usage
    die "unknown circuit: ${target}"
  fi
}

main "$@"
