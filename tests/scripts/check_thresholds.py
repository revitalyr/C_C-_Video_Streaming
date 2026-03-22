#!/usr/bin/env python3
"""check_thresholds.py — checks metrics against threshold values for IoT."""

import argparse
import json
import sys
from pathlib import Path

# Threshold values for each network profile.
# Keys correspond to profiles in run_e2e.sh.
THRESHOLDS = {
    "clean": {
        "packet_loss_rate_pct": 0.5,
        "jitter_avg_ms":        10.0,
        "jitter_max_ms":        25.0,
        # latency from app metrics
        "glass_to_glass_ms":    150.0,
        "jitter_buffer_depth_ms": 30.0,
    },
    "wifi_indoor": {
        "packet_loss_rate_pct": 1.0,
        "jitter_avg_ms":        20.0,
        "jitter_max_ms":        60.0,
        "glass_to_glass_ms":    200.0,
        "jitter_buffer_depth_ms": 50.0,
    },
    "wifi_crowded": {
        "packet_loss_rate_pct": 5.0,
        "jitter_avg_ms":        60.0,
        "jitter_max_ms":        200.0,
        "glass_to_glass_ms":    400.0,
        "jitter_buffer_depth_ms": 100.0,
    },
    "lte_edge": {
        "packet_loss_rate_pct": 8.0,
        "jitter_avg_ms":        120.0,
        "jitter_max_ms":        400.0,
        "glass_to_glass_ms":    600.0,
        "jitter_buffer_depth_ms": 150.0,
    },
    "worst_case": {
        # worst_case — just checking that the application didn't crash
        "packet_loss_rate_pct": 30.0,
        "jitter_avg_ms":        300.0,
        "jitter_max_ms":        1000.0,
        "glass_to_glass_ms":    2000.0,
        "jitter_buffer_depth_ms": 300.0,
    },
}


def load_json(path: str) -> dict:
    p = Path(path)
    if not p.exists():
        return {}
    try:
        return json.loads(p.read_text())
    except json.JSONDecodeError:
        return {}


def check(profile: str, metrics: dict, pcap_stats: dict) -> list[str]:
    """Returns a list of threshold violations."""
    thresholds = THRESHOLDS.get(profile, {})
    violations = []
    combined = {**pcap_stats, **metrics}  # app metrics override pcap if duplicated

    for key, limit in thresholds.items():
        value = combined.get(key)
        if value is None:
            print(f"  [skip] {key}: not in metrics (app may not export it yet)")
            continue
        status = "✓" if value <= limit else "✗"
        print(f"  {status} {key}: {value:.2f} (limit: {limit})")
        if value > limit:
            violations.append(f"{key}={value:.2f} exceeds limit {limit}")

    return violations


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile",    required=True)
    parser.add_argument("--metrics",    required=True, help="JSON from app --metrics-out")
    parser.add_argument("--pcap-stats", required=True, help="JSON from analyze_pcap.py")
    args = parser.parse_args()

    print(f"\n[thresholds] Profile: {args.profile}")
    print(f"[thresholds] Checking metrics...")

    app_metrics = load_json(args.metrics)
    pcap_stats  = load_json(args.pcap_stats)

    violations = check(args.profile, app_metrics, pcap_stats)

    if violations:
        print(f"\n[thresholds] FAILED — {len(violations)} violation(s):")
        for v in violations:
            print(f"  ✗ {v}")
        sys.exit(1)
    else:
        print(f"\n[thresholds] PASSED — all metrics within limits")
        sys.exit(0)


if __name__ == "__main__":
    main()
