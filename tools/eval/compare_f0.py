#!/usr/bin/env python3
"""Compare a melisma-eval pitch track with a reference f0 annotation.

    tools/eval/compare_f0.py voice.f0.csv reference.csv
    tools/eval/compare_f0.py voice.f0.csv reference.pv --ref-format mir1k

Reference formats:
  csv    two columns: time in seconds, f0 in Hz (0 = unvoiced); header optional
  mir1k  MIR-1K .pv files: one pitch value per line in MIDI semitones (0 = unvoiced),
         frames every 20 ms starting at 20 ms

Metrics (as in MIREX melody extraction):
  RPA  raw pitch accuracy: voiced reference frames estimated within 50 cents
  RCA  raw chroma accuracy: same, ignoring octave errors
  VR   voicing recall: voiced reference frames that we also call voiced
  VFA  voicing false alarm: unvoiced reference frames that we call voiced
  plus the median and 95th percentile absolute error in cents.

Only the Python standard library is needed.
"""
import argparse
import bisect
import csv
import math
import statistics
import sys


def read_estimate(path):
    times, f0s = [], []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            voiced = row.get("class", "voiced") == "voiced"
            times.append(float(row["time_s"]))
            f0s.append(float(row["f0_hz"]) if voiced else 0.0)
    return times, f0s


def read_reference(path, fmt):
    times, f0s = [], []
    with open(path) as f:
        if fmt == "mir1k":
            for i, line in enumerate(f):
                line = line.strip()
                if not line:
                    continue
                midi = float(line)
                times.append(0.02 * (i + 1))
                f0s.append(440.0 * 2 ** ((midi - 69) / 12) if midi > 0 else 0.0)
        else:
            for row in csv.reader(f):
                if not row:
                    continue
                try:
                    t, hz = float(row[0]), float(row[1])
                except ValueError:
                    continue  # header
                times.append(t)
                f0s.append(hz)
    return times, f0s


def value_at(times, values, t):
    """Nearest-frame lookup."""
    i = bisect.bisect_left(times, t)
    if i == 0:
        return values[0]
    if i >= len(times):
        return values[-1]
    return values[i] if times[i] - t < t - times[i - 1] else values[i - 1]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("estimate", help="<name>.f0.csv written by melisma-eval")
    parser.add_argument("reference")
    parser.add_argument("--ref-format", choices=["csv", "mir1k"], default="csv")
    args = parser.parse_args()

    est_t, est_f = read_estimate(args.estimate)
    ref_t, ref_f = read_reference(args.reference, args.ref_format)
    if not est_t or not ref_t:
        sys.exit("empty input")

    voiced_ref = unvoiced_ref = pitch_ok = chroma_ok = recall = false_alarm = 0
    errors = []
    for t, ref in zip(ref_t, ref_f):
        est = value_at(est_t, est_f, t)
        if ref > 0:
            voiced_ref += 1
            if est > 0:
                recall += 1
                cents = 1200 * math.log2(est / ref)
                errors.append(abs(cents))
                if abs(cents) <= 50:
                    pitch_ok += 1
                if abs((cents + 600) % 1200 - 600) <= 50:
                    chroma_ok += 1
        else:
            unvoiced_ref += 1
            if est > 0:
                false_alarm += 1

    pct = lambda a, b: 100.0 * a / b if b else float("nan")
    print(f"frames: {voiced_ref} voiced, {unvoiced_ref} unvoiced in the reference")
    print(f"RPA {pct(pitch_ok, voiced_ref):6.2f} %")
    print(f"RCA {pct(chroma_ok, voiced_ref):6.2f} %")
    print(f"VR  {pct(recall, voiced_ref):6.2f} %")
    print(f"VFA {pct(false_alarm, unvoiced_ref):6.2f} %")
    if errors:
        errors.sort()
        p95 = errors[int(0.95 * (len(errors) - 1))]
        print(f"error: median {statistics.median(errors):.1f} cents, p95 {p95:.1f} cents")


if __name__ == "__main__":
    main()
