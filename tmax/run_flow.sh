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
ATPG_FLOW=""
NO_COMPRESSION="${NO_COMPRESSION:-0}"
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
  $0 [--nocomp] <normal|power> <circuit|all>

Circuits:
  ${CIRCUITS[*]}

Modes:
  normal       TestMAX LOC/TDF ATPG: set_atpg -fill 0
  power        TestMAX LOC/TDF power-aware ATPG: adjacent fill + shift power effort
  --nocomp
               Disable TestMAX ATPG merge/compression and run fast-sequential ATPG directly.

Environment overrides:
  TMAX_SETUP=/path/to/tmax.cshrc
  TMAX_BIN=tmax
  PYTHON=python3
  FAN_BIN=./bin/opt/fan
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
  local rpt_dir="tmax/rpt_${ATPG_MODE}"
  local pat_dir="tmax/pat_${ATPG_MODE}"

  [[ -f "${netlist}" ]] || die "missing ${netlist}"
  script="$(mktemp "${TMPDIR:-/tmp}/tmax_${circuit}.tcl.XXXXXX")"
  TMP_FILES+=("${script}")
  cat > "${script}" <<TCL
set_messages -log ${rpt_dir}/${circuit}_tmax.log -replace
read_netlist techlib/NangateOpenCellLibrary.v -format verilog -define TETRAMAX -library
read_netlist mod_netlist/${circuit}.v -format verilog
run_build_model ${circuit}
add_clocks 0 CK -shift
add_scan_enables 1 test_se
add_scan_chains chain1 test_si test_so
add_pi_constraints 0 test_se
run_drc
set_faults -model transition
set_delay -launch_cycle system_clock
set_atpg -capture_cycles 2

TCL

  case "${ATPG_MODE}" in
    normal)
      cat >> "${script}" <<TCL
puts "ATPG mode: normal LOC/TDF"
set_atpg -fill 0

TCL
      ;;
    power)
      cat >> "${script}" <<TCL
puts "ATPG mode: power-aware LOC/TDF"
set_atpg -fill adjacent
set_atpg -shift_power_effort high -shift_power_budget 100

TCL
      ;;
  esac

  if [[ "${NO_COMPRESSION}" == "1" ]]; then
    cat >> "${script}" <<TCL
set_atpg -merge Off

TCL
  fi

  cat >> "${script}" <<TCL
add_faults -all
TCL

  if [[ "${NO_COMPRESSION}" == "1" ]]; then
    cat >> "${script}" <<TCL
run_atpg fast_sequential_only
TCL
  else
    cat >> "${script}" <<TCL
run_atpg -auto_compression
TCL
  fi

  cat >> "${script}" <<TCL
report_power -per_pattern -percentage
write_patterns ${pat_dir}/${circuit}.stil -format stil -replace
exit
TCL

  TMAX_SCRIPT="${script}"
}

write_fsim_script() {
  local circuit="$1"
  local script
  local rpt_dir="tmax/rpt_${ATPG_MODE}"
  local pat_dir="tmax/pat_${ATPG_MODE}"

  script="$(mktemp "${TMPDIR:-/tmp}/fsim_${circuit}.script.XXXXXX")"
  TMP_FILES+=("${script}")
  cat > "${script}" <<TCL
read_lib techlib/mod_nangate45.mdt
read_netlist mod_netlist/${circuit}.v
set_pattern_type LOC
build_circuit --frame 2
read_pattern ${pat_dir}/${circuit}.pat
set_fault_type tdf
add_fault -a
run_fault_sim
run_logic_sim
report_statistics > ${rpt_dir}/${circuit}_fsim.rpt
exit
TCL

  FSIM_SCRIPT="${script}"
}

run_one() {
  local circuit="$1"
  local tmax_script
  local fsim_script
  local pat_dir="tmax/pat_${ATPG_MODE}"
  local rpt_dir="tmax/rpt_${ATPG_MODE}"
  local stil="${pat_dir}/${circuit}.stil"
  local pat="${pat_dir}/${circuit}.pat"

  [[ -x "${FAN_BIN}" ]] || die "missing executable FAN binary: ${FAN_BIN}. Run make first or set FAN_BIN."
  mkdir -p "${pat_dir}" "${rpt_dir}"
  write_tmax_script "${circuit}"
  tmax_script="${TMAX_SCRIPT}"
  write_fsim_script "${circuit}"
  fsim_script="${FSIM_SCRIPT}"

  if [[ "${NO_COMPRESSION}" == "1" ]]; then
    echo "==> [${circuit}] TestMAX ATPG (${ATPG_MODE}, no compression)"
  else
    echo "==> [${circuit}] TestMAX ATPG (${ATPG_MODE})"
  fi
  tcsh -f -c "source ${TMAX_SETUP}; ${TMAX_BIN} -shell ${tmax_script}"

  echo "==> [${circuit}] Convert STIL to FAN PAT"
  "${PYTHON}" tmax/utility/scan_stil_to_fan_pat.py "${stil}" --out "${pat}" --pattern-type loc

  echo "==> [${circuit}] FAN fault simulation"
  "${FAN_BIN}" -f "${fsim_script}"
}

main() {
  local args=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --nocomp)
        NO_COMPRESSION="1"
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

  set -- "${args[@]}"
  [[ $# -eq 2 ]] || {
    usage
    exit 1
  }

  ATPG_MODE="$1"
  case "${ATPG_MODE}" in
    normal|power)
      ;;
    *)
      usage
      die "unknown ATPG: ${ATPG_MODE}"
      ;;
  esac

  cd "${ROOT_DIR}"
  mkdir -p "tmax/pat_${ATPG_MODE}" "tmax/rpt_${ATPG_MODE}"

  local target="$2"
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
