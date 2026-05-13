#!/usr/bin/env python3
"""Convert a TestMAX basic-scan STIL file into FAN .pat format."""

import argparse
import re
from pathlib import Path


def clean_value(value):
    value = value.strip()
    value = re.sub(r"\\r\d+", "", value)
    value = value.replace("#", "")
    return re.sub(r"\s+", "", value)


def map_input_bits(bits):
    return "".join(bit if bit in "01" else "X" for bit in bits)


def map_output_bits(bits):
    mapped = []
    for bit in bits:
        if bit in "1H":
            mapped.append("1")
        elif bit in "0L":
            mapped.append("0")
        else:
            mapped.append("X")
    return "".join(mapped)


def extract_signal_group(text, name):
    match = re.search(r'"?%s"?\s*=\s*(.*?);' % re.escape(name), text, re.S)
    if not match:
        raise SystemExit("Signal group %s not found" % name)
    return re.findall(r'"([^"]+)"', match.group(1))


def extract_scan_cells(text):
    match = re.search(r"ScanCells\s*\{(.*?)\}", text, re.S)
    if not match:
        raise SystemExit("ScanCells not found")
    cells = []
    for cell in re.findall(r'"([^"]+)"', match.group(1)):
        parts = cell.split(".")
        cells.append(parts[1] if len(parts) > 1 else parts[0])
    if not cells:
        raise SystemExit("No scan cells found")
    return cells


def extract_assignment(body, signal):
    match = re.search(r'"%s"\s*=\s*([^;]+);' % re.escape(signal), body)
    return clean_value(match.group(1)) if match else ""


def select_bits(names, bits, selected):
    by_name = dict(zip(names, bits))
    return "".join(by_name[name] for name in selected)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stil")
    parser.add_argument("--out", required=True)
    parser.add_argument("--clk", default="CK")
    parser.add_argument("--scan-enable", default="test_se")
    parser.add_argument("--scan-in", default="test_si")
    parser.add_argument("--scan-out", default="test_so")
    args = parser.parse_args()

    text = Path(args.stil).read_text()
    pi_names = extract_signal_group(text, "_pi")
    po_names = extract_signal_group(text, "_po")
    scan_cells = extract_scan_cells(text)

    excluded_inputs = {args.clk, args.scan_enable, args.scan_in}
    functional_inputs = [name for name in pi_names if name not in excluded_inputs]
    functional_outputs = [name for name in po_names if name != args.scan_out]

    pattern_re = re.compile(
        r'"pattern\s+(\d+)":\s*Call\s+"load_unload"\s*\{(.*?)\}\s*'
        r'Call\s+"(capture(?:_CK)?)"\s*\{(.*?)\}',
        re.S,
    )
    patterns = []
    for match in pattern_re.finditer(text):
        index = int(match.group(1))
        load_body = match.group(2)
        capture_body = match.group(4)
        si_bits = extract_assignment(load_body, args.scan_in)
        so_bits = extract_assignment(load_body, args.scan_out)
        pi_bits = extract_assignment(capture_body, "_pi")
        po_bits = extract_assignment(capture_body, "_po")
        if len(pi_bits) != len(pi_names):
            raise SystemExit("Pattern %d has PI width %d, expected %d" % (index, len(pi_bits), len(pi_names)))
        if len(po_bits) != len(po_names):
            raise SystemExit("Pattern %d has PO width %d, expected %d" % (index, len(po_bits), len(po_names)))
        if len(si_bits) != len(scan_cells):
            raise SystemExit("Pattern %d has scan-in width %d, expected %d" % (index, len(si_bits), len(scan_cells)))
        patterns.append(
            {
                "index": index,
                "pi": map_input_bits(select_bits(pi_names, pi_bits, functional_inputs)),
                "po": map_output_bits(select_bits(po_names, po_bits, functional_outputs)),
                "ppi": map_input_bits(si_bits[::-1]),
                "next_so": so_bits,
            }
        )

    if not patterns:
        raise SystemExit("No basic-scan patterns found in STIL")

    final_match = re.search(r'"end\s+\d+\s+unload":\s*Call\s+"load_unload"\s*\{(.*?)\}', text, re.S)
    if not final_match:
        raise SystemExit("Final unload not found")
    final_so = extract_assignment(final_match.group(1), args.scan_out)

    for idx, pattern in enumerate(patterns):
        so_bits = patterns[idx + 1]["next_so"] if idx + 1 < len(patterns) else final_so
        if len(so_bits) != len(scan_cells):
            raise SystemExit("Pattern %d has scan-out width %d, expected %d" % (pattern["index"], len(so_bits), len(scan_cells)))
        pattern["ppo"] = map_output_bits(so_bits[::-1])

    lines = []
    lines.append(" ".join(functional_inputs) + "  |")
    lines.append(" ".join(scan_cells) + "  |")
    lines.append(" ".join(functional_outputs) + " ")
    lines.append("BASIC_SCAN")
    lines.append("_num_of_pattern_%d" % len(patterns))
    for index, pattern in enumerate(patterns, 1):
        lines.append(
            "_pattern_%d %s |  | %s |  | %s |  | %s"
            % (index, pattern["pi"], pattern["ppi"], pattern["po"], pattern["ppo"])
        )
    Path(args.out).write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
