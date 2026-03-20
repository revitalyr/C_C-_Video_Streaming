#!/usr/bin/env bash
# analyze_rtcp.sh — агрегирует RTCP статистику по всем сценариям в единый отчёт
set -euo pipefail

RESULTS_DIR="/app/results"
PCAP_DIR="/app/pcap"

echo "[rtcp] Aggregating RTCP metrics across all scenarios..."

python3 - <<'EOF'
import json, os, glob
from pathlib import Path

results_dir = "/app/results"
summary_path = f"{results_dir}/summary.json"
summary = json.load(open(summary_path)) if os.path.exists(summary_path) else {}

e2e_results = {}
for stats_file in sorted(glob.glob(f"{results_dir}/*_pcap_stats.json")):
    profile = Path(stats_file).stem.replace("_pcap_stats", "")
    try:
        data = json.loads(Path(stats_file).read_text())
        e2e_results[profile] = data
        plr  = data.get("packet_loss_rate_pct", "n/a")
        javg = data.get("jitter_avg_ms", "n/a")
        jmax = data.get("jitter_max_ms", "n/a")
        rtcp = data.get("rtcp_rr_count", 0)
        print(f"  {profile:15s} | PLR={plr}%  jitter_avg={javg}ms  jitter_max={jmax}ms  RTCP_RR={rtcp}")
    except Exception as e:
        print(f"  {profile}: error reading stats: {e}")

summary["e2e"] = e2e_results
Path(summary_path).write_text(json.dumps(summary, indent=2))
print(f"\n[rtcp] Summary written to {summary_path}")
EOF
