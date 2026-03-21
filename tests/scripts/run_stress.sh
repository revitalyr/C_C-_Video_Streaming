#!/usr/bin/env bash
# run_stress.sh — IoT стресс-тест: ограниченный CPU/RAM + плохая сеть
set -euo pipefail

RESULTS_DIR="/app/results"
PROFILE="${STRESS_PROFILE:-lte_edge}"
DURATION=60

mkdir -p "${RESULTS_DIR}"

echo "[stress] IoT stress test: profile=${PROFILE} duration=${DURATION}s"
echo "[stress] CPU limit applied by Docker (0.25 cores)"
echo "[stress] Memory limit applied by Docker (128MB)"

# Применяем netem внутри контейнера
tc qdisc del dev eth0 root 2>/dev/null || true
case "${PROFILE}" in
  lte_edge)
    tc qdisc add dev eth0 root netem \
      delay 100ms 50ms loss gemodel 1% 10% 80% 0%
    ;;
  wifi_crowded)
    tc qdisc add dev eth0 root netem \
      delay 50ms 30ms loss 3% reorder 5% 25%
    ;;
esac

# Мониторинг ресурсов в фоне
{
  while true; do
    TS=$(date +%s%3N)
    CPU=$(grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$3+$4+$5)} END {print usage}')
    MEM=$(cat /proc/meminfo | awk '/MemAvailable/{avail=$2} /MemTotal/{total=$2} END {print (total-avail)*100/total}')
    echo "{\"ts\":${TS},\"cpu\":${CPU},\"mem\":${MEM}}"
    sleep 1
  done
} > "${RESULTS_DIR}/stress_resources.jsonl" &
MONITOR_PID=$!

# Запуск приложения под нагрузкой
timeout "${DURATION}" /app/video_streaming_app \
    --rtp-port 5004 \
    --rtcp-port 5005 \
    --metrics-out "${RESULTS_DIR}/stress_metrics.json" \
    2>&1 | tee "${RESULTS_DIR}/stress_app.log" || true

kill "${MONITOR_PID}" 2>/dev/null || true

# Анализ ресурсов
python3 - <<'EOF'
import json

lines = open("/app/results/stress_resources.jsonl").readlines()
data = [json.loads(l) for l in lines if l.strip()]
if data:
    cpus = [d["cpu"] for d in data]
    mems = [d["mem"] for d in data]
    stats = {
        "cpu_avg": sum(cpus)/len(cpus),
        "cpu_max": max(cpus),
        "mem_avg": sum(mems)/len(mems),
        "mem_max": max(mems),
        "samples": len(data),
    }
    print(f"[stress] CPU avg={stats['cpu_avg']:.1f}% max={stats['cpu_max']:.1f}%")
    print(f"[stress] MEM avg={stats['mem_avg']:.1f}% max={stats['mem_max']:.1f}%")

    # Проверяем: под 25% CPU нагрузка не должна превышать 95%
    if stats["cpu_max"] > 95:
        print("[stress] WARN: CPU saturation detected")
        exit(1)
    if stats["mem_max"] > 90:
        print("[stress] WARN: Memory pressure detected")
        exit(1)
    print("[stress] Resource usage within limits")
EOF

tc qdisc del dev eth0 root 2>/dev/null || true
