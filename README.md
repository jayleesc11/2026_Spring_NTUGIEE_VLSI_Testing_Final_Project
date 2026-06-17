# 2026 Spring NTUGIEE VLSI Testing Final Project - Low-Power ATPG

This repository contains a modified FAN ATPG flow for power-aware launch-on-capture (LOC) transition delay fault (TDF) ATPG experiments.

## Acknowledgement

This project is developed based on [NTU-LaDS-II/FAN_ATPG](https://github.com/NTU-LaDS-II/FAN_ATPG), an open-source FAN ATPG and fault simulation command-line tool from the Laboratory of Dependable Systems, Graduate Institute of Electronics Engineering, National Taiwan University.

The original FAN_ATPG source code, project structure, build system, benchmark support, and command-line framework belong to the original authors and contributors of FAN_ATPG. This repository keeps the original MIT license. Technology library files are covered by `TECHLIB_LICENSE`.

## Build

```sh
make
```

The FAN executable is built at:

```text
bin/opt/fan
```

## Run FAN Adjacent-Fill ATPG

This script uses the provided baseline executable:

```text
bin_old/fan_old
```

Run one circuit:

```sh
script/autoRunScripts/runFANAdjacent.sh s27
```

Run all circuits:

```sh
script/autoRunScripts/runFANAdjacent.sh all
```

Outputs:

```text
power_rpt/FAN_power_<circuit>_ad.rpt
power_pat/FAN_power_<circuit>_ad.pat
power_pat/FAN_power_<circuit>_ad.stil
```

## Run FAN Power-Aware ATPG

Run one circuit:

```sh
script/autoRunScripts/runFANPower.sh s27
```

Run all circuits:

```sh
script/autoRunScripts/runFANPower.sh all
```

Outputs:

```text
power_rpt/FAN_power_<circuit>.rpt
power_pat/FAN_power_<circuit>.pat
power_pat/FAN_power_<circuit>.stil
```

## Run TestMAX Power Flow

This flow requires Synopsys TestMAX/TetraMAX. `TMAX_SETUP` must be set to the local TestMAX setup file before running the script. `TMAX_BIN` defaults to `tmax` and can be overridden if needed.

Run one circuit:

```sh
TMAX_SETUP=/path/to/tmax.cshrc TMAX_BIN=tmax tmax/run_flow.sh power s27
```

Run all circuits:

```sh
TMAX_SETUP=/path/to/tmax.cshrc TMAX_BIN=tmax tmax/run_flow.sh power all
```

Outputs:

```text
tmax/power_rpt/<circuit>_tmax.log
tmax/power_rpt/<circuit>_fsim.rpt
tmax/power_pat/<circuit>.stil
tmax/power_pat/<circuit>.pat
```

## License

This repository follows the MIT license from the original FAN_ATPG project. See `LICENSE` for details.

Technology library files are subject to `TECHLIB_LICENSE`.
