# TestMAX raw-scan flow

This flow uses the original sequential scan netlist, not an abstract combinational netlist.

## One-command flow

Run one circuit:

```
./tmax/run_flow.sh power s27
```

Choose ATPG mode:

```
./tmax/run_flow.sh normal s27
./tmax/run_flow.sh power s27
./tmax/run_flow.sh --nocomp power s27
```

Run all supported circuits:

```
./tmax/run_flow.sh power all
./tmax/run_flow.sh normal all
```

Supported circuits are the scan netlists in `mod_netlist/`: `s27`, `s208`, `s510`, `s953`, `s1196`, `s1238`, `s5378`, `s9234`, `s15850`, `s35932`, `s38417`, and `s38584`.

The script runs TestMAX LOC/TDF ATPG, converts the generated scan STIL into FAN `.pat`, then runs FAN TDF fault simulation. `power` uses adjacent fill and shift-power reduction. `normal` uses `set_atpg -fill 0`. The default is compression on with `run_atpg -auto_compression`, writing to the selected ATPG mode's regular `tmax/rpt_<mode>` and `tmax/pat_<mode>` outputs. Add `--nocomp` to disable compression; no-compression outputs are written to `tmax/rpt_<mode>_nocomp` and `tmax/pat_<mode>_nocomp`.

`run_flow.sh` creates temporary TestMAX Tcl and FAN fsim scripts under `/tmp`, so adding a circuit usually only requires adding its name to the `CIRCUITS` list in `run_flow.sh`.

## Generated flow

The generated TestMAX Tcl is equivalent to:

```
set_messages -log tmax/rpt_<mode>/<circuit>_tmax.log -replace
read_netlist techlib/NangateOpenCellLibrary.v -format verilog -define TETRAMAX -library
read_netlist mod_netlist/<circuit>.v -format verilog
run_build_model <circuit>
add_clocks 0 CK -shift
add_scan_enables 1 test_se
add_scan_chains chain1 test_si test_so
add_pi_constraints 0 test_se
run_drc
set_faults -model transition
```

Then the selected ATPG mode is applied:

```
# normal
set_atpg -fill 0

# power
set_atpg -fill adjacent
set_atpg -shift_power_effort high -shift_power_budget 100
set_atpg -power_effort high -power_budget 100
```

Finally, by default:

```
add_faults -all
run_atpg -auto_compression
report_power -per_pattern -percentage
write_patterns tmax/pat_<mode>/<circuit>.stil -format stil -replace
exit
```

For `--nocomp` no-compression runs, the final ATPG section is:

```
set_atpg -merge Off
add_faults -all
run_atpg fast_sequential_only
report_power -per_pattern -percentage
write_patterns tmax/pat_<mode>_nocomp/<circuit>.stil -format stil -replace
exit
```

After TestMAX, the script converts and simulates:

```
python3 tmax/utility/scan_stil_to_fan_pat.py tmax/pat_<mode>/<circuit>.stil --out tmax/pat_<mode>/<circuit>.pat --pattern-type loc
./bin/opt/fan -f <temporary-fsim-script>
```

The generated FAN fsim report is `tmax/rpt_<mode>/<circuit>_fsim.rpt`, or `tmax/rpt_<mode>_nocomp/<circuit>_fsim.rpt` for `--nocomp` no-compression runs.
