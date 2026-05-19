#!/usr/bin/env python3
"""Convert a TestMAX scan STIL file into FAN .pat format."""

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


def iter_pattern_segments(text):
    pattern_re = re.compile(r'"pattern\s+(\d+)":\s*Call\s+"load_unload"\s*\{(.*?)\}', re.S)
    matches = list(pattern_re.finditer(text))
    for pos, match in enumerate(matches):
        end = matches[pos + 1].start() if pos + 1 < len(matches) else len(text)
        final_unload = re.search(r'"end\s+\d+\s+unload":', text[match.end():end], re.S)
        if final_unload:
            end = match.end() + final_unload.start()
        yield int(match.group(1)), match.group(2), text[match.end():end]


def extract_capture_events(segment):
    event_re = re.compile(
        r'V\s*\{(.*?)\}|Call\s+"(capture(?:_CK)?)"\s*\{(.*?)\}',
        re.S,
    )
    events = []
    for match in event_re.finditer(segment):
        if match.group(1) is not None:
            if extract_assignment(match.group(1), "_pi"):
                events.append({"kind": "V", "body": match.group(1), "clock": False})
        else:
            events.append(
                {
                    "kind": match.group(2),
                    "body": match.group(3),
                    "clock": match.group(2) == "capture_CK",
                }
            )
    return events


def select_capture_slots(events):
    calls = [event for event in events if event["kind"] != "V"]
    if not calls:
        return []
    if events and events[0]["kind"] == "V":
        slots = [events[0], calls[0]]
    else:
        slots = calls[:2]
    return slots[:2]


def require_width(pattern_index, field, bits, expected):
    if len(bits) != expected:
        raise SystemExit(
            "Pattern %d has %s width %d, expected %d"
            % (pattern_index, field, len(bits), expected)
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("stil")
    parser.add_argument("--out", required=True)
    parser.add_argument("--clk", default="CK")
    parser.add_argument("--scan-enable", default="test_se")
    parser.add_argument("--scan-in", default="test_si")
    parser.add_argument("--scan-out", default="test_so")
    parser.add_argument("--pattern-type", choices=("auto", "basic", "loc"), default="auto")
    args = parser.parse_args()

    text = Path(args.stil).read_text()
    pi_names = extract_signal_group(text, "_pi")
    po_names = extract_signal_group(text, "_po")
    scan_cells = extract_scan_cells(text)

    excluded_inputs = {args.clk, args.scan_enable, args.scan_in}
    functional_inputs = [name for name in pi_names if name not in excluded_inputs]
    functional_outputs = [name for name in po_names if name != args.scan_out]

    patterns = []
    saw_multicycle_capture = False
    for index, load_body, pattern_segment in iter_pattern_segments(text):
        capture_events = extract_capture_events(pattern_segment)
        capture_slots = select_capture_slots(capture_events)
        if not capture_slots:
            raise SystemExit("Pattern %d has no capture call" % index)
        capture_calls = [event for event in capture_events if event["kind"] != "V"]
        saw_multicycle_capture = saw_multicycle_capture or len(capture_calls) > 1 or any(
            event["kind"] == "V" for event in capture_events
        )
        si_bits = extract_assignment(load_body, args.scan_in)
        so_bits = extract_assignment(load_body, args.scan_out)
        pi1_bits = extract_assignment(capture_slots[0]["body"], "_pi")
        pi2_bits = extract_assignment(capture_slots[1]["body"], "_pi") if len(capture_slots) > 1 else ""
        po1_bits = extract_assignment(capture_slots[0]["body"], "_po")
        po2_bits = extract_assignment(capture_slots[1]["body"], "_po") if len(capture_slots) > 1 else ""
        capture_clock = "".join("1" if slot["clock"] else "0" for slot in capture_slots)
        require_width(index, "PI1", pi1_bits, len(pi_names))
        if pi2_bits:
            require_width(index, "PI2", pi2_bits, len(pi_names))
        if po1_bits:
            require_width(index, "PO1", po1_bits, len(po_names))
        if po2_bits:
            require_width(index, "PO2", po2_bits, len(po_names))
        require_width(index, "scan-in", si_bits, len(scan_cells))
        patterns.append(
            {
                "index": index,
                "pi1": map_input_bits(select_bits(pi_names, pi1_bits, functional_inputs)),
                "pi2": map_input_bits(select_bits(pi_names, pi2_bits, functional_inputs)) if pi2_bits else "",
                "po1": map_output_bits(select_bits(po_names, po1_bits, functional_outputs)) if po1_bits else "",
                "po2": map_output_bits(select_bits(po_names, po2_bits, functional_outputs)) if po2_bits else "",
                "ppi": map_input_bits(si_bits[::-1]),
                "capture_clock": capture_clock,
                "next_so": so_bits,
            }
        )

    if not patterns:
        raise SystemExit("No scan patterns found in STIL")

    final_match = re.search(r'"end\s+\d+\s+unload":\s*Call\s+"load_unload"\s*\{(.*?)\}', text, re.S)
    if not final_match:
        raise SystemExit("Final unload not found")
    final_so = extract_assignment(final_match.group(1), args.scan_out)

    for idx, pattern in enumerate(patterns):
        so_bits = patterns[idx + 1]["next_so"] if idx + 1 < len(patterns) else final_so
        if len(so_bits) != len(scan_cells):
            raise SystemExit("Pattern %d has scan-out width %d, expected %d" % (pattern["index"], len(so_bits), len(scan_cells)))
        pattern["ppo"] = map_output_bits(so_bits[::-1])

    pattern_type = args.pattern_type
    if pattern_type == "auto":
        pattern_type = "loc" if saw_multicycle_capture else "basic"

    lines = []
    lines.append(" ".join(functional_inputs) + "  |")
    lines.append(" ".join(scan_cells) + "  |")
    lines.append(" ".join(functional_outputs) + " ")
    lines.append("LAUNCH_ON_CAPTURE" if pattern_type == "loc" else "BASIC_SCAN")
    lines.append("_num_of_pattern_%d" % len(patterns))
    for index, pattern in enumerate(patterns, 1):
        if pattern_type == "loc":
            lines.append(
                "_pattern_%d %s | %s | %s | %s | %s | %s | %s"
                % (
                    index,
                    pattern["pi1"],
                    pattern["pi2"],
                    pattern["ppi"],
                    pattern["capture_clock"],
                    pattern["po1"],
                    pattern["po2"],
                    pattern["ppo"],
                )
            )
        else:
            po = pattern["po2"] or pattern["po1"]
            lines.append(
                "_pattern_%d %s |  | %s |  | %s |  | %s"
                % (index, pattern["pi1"], pattern["ppi"], po, pattern["ppo"])
            )
    Path(args.out).write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
