#!/usr/bin/env python3
"""analyze_pcap.py — извлекает RTP/RTCP метрики из pcap файла."""

import argparse
import json
import sys
from pathlib import Path

try:
    import pyshark
except ImportError:
    print("[analyze_pcap] pyshark not available, using tshark CLI fallback")
    pyshark = None

import subprocess


def analyze_with_tshark(pcap_path: str, rtp_port: int) -> dict:
    """Fallback: парсим вывод tshark напрямую."""
    cmd = [
        "tshark", "-r", pcap_path,
        "-Y", f"rtp and udp.port=={rtp_port}",
        "-T", "fields",
        "-e", "frame.time_epoch",
        "-e", "rtp.seq",
        "-e", "rtp.timestamp",
        "-e", "rtp.marker",
        "-E", "separator=,",
        "-E", "header=y",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"[analyze_pcap] tshark error: {result.stderr}", file=sys.stderr)
        return {}

    lines = result.stdout.strip().split("\n")
    if len(lines) < 2:
        return {"error": "no RTP packets captured", "packet_count": 0}

    packets = []
    for line in lines[1:]:  # пропускаем заголовок
        parts = line.split(",")
        if len(parts) >= 3:
            try:
                packets.append({
                    "time": float(parts[0]),
                    "seq": int(parts[1]),
                    "ts": int(parts[2]),
                })
            except ValueError:
                continue

    if not packets:
        return {"error": "failed to parse packets", "packet_count": 0}

    # Считаем метрики
    seqs = [p["seq"] for p in packets]
    times = [p["time"] for p in packets]

    total_expected = (max(seqs) - min(seqs) + 1) if seqs else 0
    total_received = len(packets)
    lost = max(0, total_expected - total_received)
    plr = (lost / total_expected * 100) if total_expected > 0 else 0

    # Межпакетные задержки (jitter)
    ipdvs = []
    for i in range(1, len(times)):
        ipdvs.append(abs(times[i] - times[i-1]) * 1000)  # в мс

    jitter_avg = sum(ipdvs) / len(ipdvs) if ipdvs else 0
    jitter_max = max(ipdvs) if ipdvs else 0

    return {
        "packet_count": total_received,
        "expected_packets": total_expected,
        "lost_packets": lost,
        "packet_loss_rate_pct": round(plr, 3),
        "jitter_avg_ms": round(jitter_avg, 3),
        "jitter_max_ms": round(jitter_max, 3),
        "duration_s": round(times[-1] - times[0], 3) if len(times) > 1 else 0,
    }


def analyze_rtcp(pcap_path: str, rtcp_port: int) -> dict:
    """Извлекаем RTCP Receiver Report статистику."""
    cmd = [
        "tshark", "-r", pcap_path,
        "-Y", f"rtcp and udp.port=={rtcp_port}",
        "-T", "fields",
        "-e", "rtcp.rr.fraction_lost",
        "-e", "rtcp.rr.jitter",
        "-e", "rtcp.rr.dlsr",
        "-E", "separator=,",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True)

    fractions, jitters = [], []
    for line in result.stdout.strip().split("\n"):
        parts = line.split(",")
        if len(parts) >= 2:
            try:
                fractions.append(float(parts[0]))
                jitters.append(float(parts[1]))
            except ValueError:
                continue

    return {
        "rtcp_rr_count": len(fractions),
        "rtcp_fraction_lost_avg": round(sum(fractions)/len(fractions), 4) if fractions else 0,
        "rtcp_jitter_avg": round(sum(jitters)/len(jitters), 2) if jitters else 0,
        "rtcp_jitter_max": round(max(jitters), 2) if jitters else 0,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pcap", required=True)
    parser.add_argument("--rtp-port", type=int, default=5004)
    parser.add_argument("--rtcp-port", type=int, default=5005)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    if not Path(args.pcap).exists():
        print(f"[analyze_pcap] pcap not found: {args.pcap}", file=sys.stderr)
        result = {"error": "pcap file not found"}
    else:
        rtp_stats = analyze_with_tshark(args.pcap, args.rtp_port)
        rtcp_stats = analyze_rtcp(args.pcap, args.rtcp_port)
        result = {**rtp_stats, **rtcp_stats}

    print(json.dumps(result, indent=2))
    Path(args.out).write_text(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
