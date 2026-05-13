# Report

<small><i>Wang Wei-Shen Last modified in Jan 2023</i></small>

Contains the reports after ATPG or Fault Simulation.
A report records the important statistics:

- FaultModel: SAF (Stuck At Fault) or TDF (Transition Delay Fault)

- FU (full): Full fault list extracted

- FU (collapsed): Full fault list after fault collapsing

- DT (detected): Detected faults

- test coverage, fault coverage, atpg effectiveness<br>

- \#Patterns: Test Length

- Power metric: FFTC (Flip-Flop Transition Count)
    Note that pessimistic unknown policy is used: unknown transitions are counted as possible toggles

  - Shift FFTC: assuming all initial scan state 0. Final unload is not included, matching TestMAX `report_power` shift switching.
  - Capture FFTC

- Runtime


## Contact

Wang Wei-Shen - b08901051@ntu.edu.tw
