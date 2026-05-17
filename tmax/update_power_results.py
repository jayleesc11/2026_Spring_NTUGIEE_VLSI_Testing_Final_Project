#!/usr/bin/env python3
import argparse
import csv
import re
from pathlib import Path


DEFAULT_CIRCUITS = [
    "s27",
    "s208",
    "s510",
    "s953",
    "s1196",
    "s1238",
    "s5378",
    "s9234",
    "s15850",
    "s35932",
    "s38417",
    "s38584",
]

CSV_FIELDS = [
    "circuit",
    "method",
    "fault_coverage_pct",
    "patterns",
    "shift_total_fftc",
    "shift_avg_fftc_per_cycle",
    "shift_max_fftc_per_cycle",
    "capture_total_fftc",
    "capture_avg_fftc_per_pattern",
    "capture_max_fftc_per_pattern",
]

METHOD_ORDER = ["adjacent", "power", "tmax_normal", "tmax_power"]


def method_rank(method):
    if method in METHOD_ORDER:
        return METHOD_ORDER.index(method)
    return len(METHOD_ORDER)


def parse_report(report_path):
    data = {}
    section = None
    for line in report_path.read_text(errors="ignore").splitlines():
        if "fault coverage" in line:
            match = re.search(r"([0-9]+(?:\.[0-9]+)?)%", line)
            if match:
                data["fault_coverage_pct"] = match.group(1)
        elif "#Patterns" in line:
            match = re.search(r"(\d+)\s*$", line)
            if match:
                data["patterns"] = match.group(1)
        elif "shift FFTC" in line:
            section = "shift"
        elif "capture FFTC" in line:
            section = "capture"
        elif "total FFTC" in line and section:
            match = re.search(r"(\d+)\s*$", line)
            if match:
                data[f"{section}_total_fftc"] = match.group(1)
        elif "average FFTC/cycle" in line:
            match = re.search(r"([0-9]+(?:\.[0-9]+)?)\s*$", line)
            if match:
                data["shift_avg_fftc_per_cycle"] = match.group(1)
        elif "maximum FFTC/cycle" in line:
            match = re.search(r"(\d+)\s*$", line)
            if match:
                data["shift_max_fftc_per_cycle"] = match.group(1)
        elif "average FFTC/pattern" in line:
            match = re.search(r"([0-9]+(?:\.[0-9]+)?)\s*$", line)
            if match:
                data["capture_avg_fftc_per_pattern"] = match.group(1)
        elif "maximum FFTC/pattern" in line:
            match = re.search(r"(\d+)\s*$", line)
            if match:
                data["capture_max_fftc_per_pattern"] = match.group(1)
    return data


def read_summary(csv_path):
    with csv_path.open(newline="") as csv_file:
        reader = csv.DictReader(csv_file)
        rows = list(reader)
        fieldnames = reader.fieldnames or CSV_FIELDS
    return fieldnames, rows


def write_summary(csv_path, fieldnames, rows):
    with csv_path.open("w", newline="") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def insert_row_in_order(rows, row, circuits):
    circuit = row.get("circuit", "")
    circuit_order = {name: index for index, name in enumerate(circuits)}
    new_circuit_rank = circuit_order.get(circuit, len(circuit_order))
    new_method_rank = method_rank(row.get("method", ""))

    same_circuit_indices = [
        index for index, existing_row in enumerate(rows) if existing_row.get("circuit", "") == circuit
    ]
    if same_circuit_indices:
        for index in same_circuit_indices:
            if method_rank(rows[index].get("method", "")) > new_method_rank:
                rows.insert(index, row)
                return
        rows.insert(same_circuit_indices[-1] + 1, row)
        return

    for index, existing_row in enumerate(rows):
        existing_circuit = existing_row.get("circuit", "")
        existing_circuit_rank = circuit_order.get(existing_circuit, len(circuit_order))
        if existing_circuit_rank > new_circuit_rank:
            rows.insert(index, row)
            return

    rows.append(row)


def update_power_rows(csv_path, report_dir, circuits, method):
    fieldnames, rows = read_summary(csv_path)
    missing_reports = []
    updated = 0
    existing_keys = {(row.get("circuit"), row.get("method")) for row in rows}

    for circuit in circuits:
        report_path = report_dir / f"FAN_power_{circuit}.rpt"
        if not report_path.exists():
            missing_reports.append(str(report_path))
            continue

        values = parse_report(report_path)
        key = (circuit, method)
        if key not in existing_keys:
            row = {field: "" for field in fieldnames}
            row["circuit"] = circuit
            row["method"] = method
            insert_row_in_order(rows, row, circuits)
            existing_keys.add(key)

        for row in rows:
            if row.get("circuit") == circuit and row.get("method") == method:
                row.update(values)
                updated += 1
                break

    if missing_reports:
        raise FileNotFoundError("Missing report(s):\n" + "\n".join(missing_reports))

    write_summary(csv_path, fieldnames, rows)
    return updated


def main():
    parser = argparse.ArgumentParser(
        description="Update result_summary.csv power rows from power_rpt/FAN_power_<circuit>.rpt."
    )
    parser.add_argument("--csv", default="tmax/result_summary.csv", type=Path)
    parser.add_argument("--report-dir", default="power_rpt", type=Path)
    parser.add_argument("--method", default="power")
    parser.add_argument("--circuits", nargs="*", default=DEFAULT_CIRCUITS)
    args = parser.parse_args()

    updated = update_power_rows(args.csv, args.report_dir, args.circuits, args.method)
    print(f"updated {updated} {args.method} row(s) in {args.csv}")


if __name__ == "__main__":
    main()
