# TestMAX raw-scan flow

This flow uses the original sequential scan netlist, not an abstract combinational netlist.

## One-command flow

Run one circuit:

```
./tmax/run_flow.sh s27
```

Choose ATPG mode:

```
./tmax/run_flow.sh --mode normal s27
./tmax/run_flow.sh --mode power s27
```

Run all supported circuits:

```
./tmax/run_flow.sh all
./tmax/run_flow.sh --power-aware all
```

Supported circuits are the scan netlists in `mod_netlist/`: `s27`, `s208`, `s510`, `s953`, `s1196`, `s1238`, `s5378`, `s9234`, `s15850`, `s35932`, `s38417`, and `s38584`.

The script runs TestMAX ATPG, converts the generated scan STIL into FAN `.pat`, then runs FAN fault simulation. The default mode is `power`, which uses adjacent fill and shift-power reduction. `normal` uses `set_atpg -fill 0`.

`run_flow.sh` creates temporary TestMAX Tcl and FAN fsim scripts under `/tmp`, so adding a circuit usually only requires adding its name to the `CIRCUITS` list in `run_flow.sh`.

## Generated flow

The generated TestMAX Tcl is equivalent to:

```
set_messages -log tmax/rpt/<circuit>_tmax.log -replace
read_netlist techlib/NangateOpenCellLibrary.v -format verilog -define TETRAMAX -library
read_netlist mod_netlist/<circuit>.v -format verilog
run_build_model <circuit>
add_clocks 0 CK -shift
add_scan_enables 1 test_se
add_scan_chains chain1 test_si test_so
run_drc
```

Then the selected ATPG mode is applied:

```
# normal
set_atpg -fill 0

# power
set_atpg -fill adjacent
set_atpg -shift_power_effort high -shift_power_budget 100
```

Finally:

```
add_faults -all
run_atpg -auto_compression
report_power -per_pattern -percentage
write_patterns tmax/pat/<circuit>.stil -format stil -replace
exit
```

After TestMAX, the script converts and simulates:

```
python3 tmax/utility/scan_stil_to_fan_pat.py tmax/pat/<circuit>.stil --out tmax/pat/<circuit>.pat
./bin/opt/fan -f <temporary-fsim-script>
```

The generated FAN fsim report is `tmax/rpt/<circuit>_fsim.rpt`.
