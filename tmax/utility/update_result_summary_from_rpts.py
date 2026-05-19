#!/usr/bin/env python3
import re
import csv
import glob
import os
from collections import OrderedDict

ROOT = os.path.dirname(os.path.abspath(__file__))
# TMAX dir is parent of this utility folder
TMAX_DIR = os.path.dirname(ROOT)
# Project root is parent of tmax
PROJECT_ROOT = os.path.dirname(TMAX_DIR)
CSV_PATH = os.path.join(TMAX_DIR, 'result_summary.csv')

def parse_rpt(path):
    with open(path, 'r', errors='ignore') as f:
        txt = f.read()

    def find_regex(pattern, flags=0):
        m = re.search(pattern, txt, flags)
        return m.group(1).strip() if m else None

    data = {}
    # circuit name
    circuit = find_regex(r'Circuit name\s+([A-Za-z0-9_\-]+)')
    if not circuit:
        # try from filename
        bn = os.path.basename(path)
        m = re.search(r's(\d+)', bn)
        circuit = ('s' + m.group(1)) if m else bn
    data['circuit'] = circuit

    # fault coverage (percentage)
    fc = find_regex(r'fault coverage\s+([0-9]+\.?[0-9]*)%')
    if fc:
        data['fault_coverage_pct'] = fc

    # patterns (#Patterns or Patterns)
    pats = find_regex(r'(?:#Patterns|Patterns)\s+([0-9]+)')
    if pats:
        data['patterns'] = pats

    # shift FFTC block
    shift_total = find_regex(r'shift[\s\S]{0,120}?total FFTC\s+([0-9]+)')
    shift_avg = find_regex(r'shift[\s\S]{0,120}?average FFTC\s+([0-9]+\.?[0-9]*)')
    shift_max = find_regex(r'shift[\s\S]{0,120}?maximum FFTC\s+([0-9]+)')
    if shift_total:
        data['shift_total_fftc'] = shift_total
    if shift_avg:
        data['shift_avg_fftc_per_cycle'] = shift_avg
    if shift_max:
        data['shift_max_fftc_per_cycle'] = shift_max

    # capture FFTC block
    cap_total = find_regex(r'capture[\s\S]{0,120}?total FFTC\s+([0-9]+)')
    cap_avg = find_regex(r'capture[\s\S]{0,120}?average FFTC\s+([0-9]+\.?[0-9]*)')
    cap_max = find_regex(r'capture[\s\S]{0,120}?maximum FFTC\s+([0-9]+)')
    if cap_total:
        data['capture_total_fftc'] = cap_total
    if cap_avg:
        data['capture_avg_fftc_per_pattern'] = cap_avg
    if cap_max:
        data['capture_max_fftc_per_pattern'] = cap_max

    return data

def method_from_path(path):
    # tmax/rpt_normal -> tmax_normal
    if '/rpt_normal/' in path:
        return 'tmax_normal'
    if '/rpt_power/' in path:
        return 'tmax_power'
    # power_rpt/FAN_power_sXXX_ad.rpt -> adjacent
    bn = os.path.basename(path)
    if bn.endswith('_ad.rpt'):
        return 'adjacent'
    if 'power_rpt' in path:
        return 'power'
    return 'unknown'

def main():
    # load existing CSV
    if not os.path.exists(CSV_PATH):
        print('CSV not found at', CSV_PATH)
        return

    with open(CSV_PATH, newline='') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        header = reader.fieldnames

    # build lookup
    lookup = {(r['circuit'], r['method']): r for r in rows}

    # gather rpt files
    rpt_files = []
    rpt_files += glob.glob(os.path.join(TMAX_DIR, 'rpt_normal', '*.rpt'))
    rpt_files += glob.glob(os.path.join(TMAX_DIR, 'rpt_power', '*.rpt'))
    rpt_files += glob.glob(os.path.join(PROJECT_ROOT, 'power_rpt', '*.rpt'))

    rpt_files = sorted(set(rpt_files))

    updated = 0
    for rpt in rpt_files:
        parsed = parse_rpt(rpt)
        if not parsed.get('circuit'):
            continue
        circuit = parsed.pop('circuit')
        method = method_from_path(rpt)
        key = (circuit, method)
        if key in lookup:
            row = lookup[key]
        else:
            # create new row with empty fields
            row = OrderedDict((h, '') for h in header)
            row['circuit'] = circuit
            row['method'] = method
            rows.append(row)
            lookup[key] = row

        # update fields when parsed
        for k, v in parsed.items():
            if k in row and v is not None:
                row[k] = v
        updated += 1

    with open(CSV_PATH, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        writer.writerows(rows)

    print('Processed', len(rpt_files), 'rpt files, updated or added', updated, 'rows in', CSV_PATH)

if __name__ == '__main__':
    main()
