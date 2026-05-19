# Script

<small><i>Wang Wei-Shen Last modified in Jan 2023</i></small>

This folder contains some scripts the users might want to use for convenience.

    .
    |--. autoRunScripts/          # Contains scripts for automating task across all circuits.
    |  |
    |  |-- averageAllFC.sh        # Calculate and log the average Fault Coverage of all the circuits that have report in the rpt/ folder.
    |  |
    |  |-- averageAllTL.sh        # Calculate and log the average Test Length of all the circuits that have report in the rpt/ folder.
    |  |
    |  `-- runFAN.sh              # Execute all the script in a  user specified folder, e.g. fanScripts/ or fanScriptsNoDTC/
    |
    |-- fanScripts/               # Contains scripts for ATPG and Fault Simulation with Dynamic Test Compression turned ON for ATPG.
    |
    |-- fanScriptsNoDTC/          # Contains scripts for ATPG and Fault Simulation with Dynamic Test Compression turned OFF for ATPG.
    |
    `-- powerFANScripts/          # Contains LOC/TDF two-frame power-aware ATPG experiments.

## Power-aware X-Fill

`set_X-Fill` supports these modes:

- `off`: disable X-fill.
- `on` or `random`: use the original random X-fill.
- `adjacent`: use adjacent fill to reduce neighboring input/scan transitions.
- `sfill`: use the original iFill-style probability-based S-filling.
- `power`: use our power-aware fill. It currently optimizes shift FFTC and does not enforce a capture-power limit.

## Power Scripts

`powerFANScripts/` runs transition-delay ATPG in launch-on-capture mode:

```
set_pattern_type LOC
build_circuit --frame 2
set_fault_type tdf
set_static_compression off
set_dynamic_compression off
set_X-Fill power
```

Reports are written to `power_rpt/`, and generated patterns are written to `power_pat/`.

## Contact

Wang Wei-Shen - b08901051@ntu.edu.tw
