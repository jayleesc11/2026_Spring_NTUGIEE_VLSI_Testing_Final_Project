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

def ensure_header(header, rows):
    derived_columns = (
        'overall_avg_fftc',
        'runtime_sec',
    )
    old_derived_columns = (
        'total_fftc',
        'power_total_fftc_delta_pct_vs_method',
        'total_fftc_delta_pct_vs_power',
        'runtime_delta_pct_vs_power',
        'powerpatterns_delta_pct_vs_method',
        'poweroverall_avg_fftc_delta_pct_vs_method',
        'power_runtime_delta_pct_vs_method',
    )
    header = [column for column in list(header or []) if column not in derived_columns and column not in old_derived_columns]
    header.extend(derived_columns)

    for row in rows:
        for column in old_derived_columns:
            row.pop(column, None)
        for column in header:
            row.setdefault(column, '')
        for column in list(row.keys()):
            if column not in header:
                header.append(column)
    return header

def parse_rpt(path):
    with open(path, 'r', errors='ignore') as f:
        txt = f.read()

    def find_regex(pattern, flags=0):
        m = re.search(pattern, txt, flags)
        return m.group(1).strip() if m else None

    def find_block(start_pattern, end_pattern):
        start = re.search(start_pattern, txt, re.IGNORECASE)
        if not start:
            return ''
        end = re.search(end_pattern, txt[start.end():], re.IGNORECASE)
        if not end:
            return txt[start.end():]
        return txt[start.end():start.end() + end.start()]

    def find_in_text(source, pattern, flags=0):
        m = re.search(pattern, source, flags)
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
    shift_block = find_block(r'shift FFTC', r'capture FFTC|ATPG runtime')
    shift_total = find_in_text(shift_block, r'total FFTC\s+([0-9]+)')
    shift_avg = find_in_text(shift_block, r'average FFTC\s+([0-9]+\.?[0-9]*)')
    shift_max = find_in_text(shift_block, r'maximum FFTC\s+([0-9]+)')
    if shift_total:
        data['shift_total_fftc'] = shift_total
    if shift_avg:
        data['shift_avg_fftc_per_cycle'] = shift_avg
    if shift_max:
        data['shift_max_fftc_per_cycle'] = shift_max

    # capture FFTC block
    capture_block = find_block(r'capture FFTC', r'ATPG runtime')
    cap_total = find_in_text(capture_block, r'total FFTC\s+([0-9]+)')
    cap_avg = find_in_text(capture_block, r'average FFTC\s+([0-9]+\.?[0-9]*)')
    cap_max = find_in_text(capture_block, r'maximum FFTC\s+([0-9]+)')
    if cap_total:
        data['capture_total_fftc'] = cap_total
    if cap_avg:
        data['capture_avg_fftc_per_pattern'] = cap_avg
    if cap_max:
        data['capture_max_fftc_per_pattern'] = cap_max

    overall_avg = find_regex(r'overall average FFTC\s+([0-9]+(?:\.[0-9]+)?(?:e[+\-]?[0-9]+)?)', re.IGNORECASE)
    if overall_avg:
        data['overall_avg_fftc'] = overall_avg

    runtime = find_regex(r'ATPG runtime\s+([0-9]+(?:\.[0-9]+)?(?:e[+\-]?[0-9]+)?)\s*s', re.IGNORECASE)
    if runtime:
        data['runtime_sec'] = runtime

    return data

def parse_tmax_log_runtime(path):
    if not os.path.exists(path):
        return None

    atpg_runtime = None
    atpg_row = re.compile(
        r'^\s*\d+\s+\d+\s+\d+\s+\d+/\d+/\d+\s+\d+(?:\.\d+)?%\s+([0-9]+(?:\.[0-9]+)?)\s*$'
    )
    with open(path, 'r', errors='ignore') as f:
        for line in f:
            match = atpg_row.match(line)
            if match:
                atpg_runtime = match.group(1)
    return atpg_runtime

def tmax_log_path_for_rpt(rpt):
    directory = os.path.dirname(rpt)
    basename = os.path.basename(rpt)
    if basename.endswith('_fsim.rpt'):
        return os.path.join(directory, basename.replace('_fsim.rpt', '_tmax.log'))
    match = re.search(r'(s\d+)', basename)
    if match:
        return os.path.join(directory, match.group(1) + '_tmax.log')
    return None

def method_from_path(path):
    normalized_path = path.replace(os.sep, '/')
    # tmax/power_rpt/ -> tmax_power
    if '/tmax/power_rpt/' in normalized_path or normalized_path.startswith('tmax/power_rpt/'):
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
    header = ensure_header(header, rows)

    # build lookup
    lookup = {(r['circuit'], r['method']): r for r in rows}

    # gather rpt files
    rpt_files = []
    rpt_files += glob.glob(os.path.join(TMAX_DIR, 'power_rpt', '*.rpt'))
    rpt_files += glob.glob(os.path.join(PROJECT_ROOT, 'power_rpt', '*.rpt'))

    rpt_files = sorted(set(rpt_files))

    updated = 0
    for rpt in rpt_files:
        parsed = parse_rpt(rpt)
        if not parsed.get('circuit'):
            continue
        circuit = parsed.pop('circuit')
        method = method_from_path(rpt)
        if method.startswith('tmax_'):
            parsed.pop('runtime_sec', None)
            log_path = tmax_log_path_for_rpt(rpt)
            runtime = parse_tmax_log_runtime(log_path) if log_path else None
            if runtime is not None:
                parsed['runtime_sec'] = runtime
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
        writer = csv.DictWriter(f, fieldnames=header, lineterminator='\n')
        writer.writeheader()
        writer.writerows(rows)

    print('Processed', len(rpt_files), 'rpt files, updated or added', updated, 'rows in', CSV_PATH)

if __name__ == '__main__':
    main()
