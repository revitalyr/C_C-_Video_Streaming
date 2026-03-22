#!/usr/bin/env bash
# run_e2e.sh — end-to-end tests: starts RTP stream, applies netem,
#              starts application, measures latency and PLR.
set -euo pipefail

RESULTS_DIR="/app/results"
PCAP_DIR="/app/pcap"
RTP_HOST="${RTP_HOST:-rtp-source}"
RTP_PORT="${RTP_PORT:-5004}"
RTCP_PORT="${RTCP_PORT:-5005}"
DURATION=30          # seconds per scenario

mkdir -p "${RESULTS_DIR}" "${PCAP_DIR}"

# ── Helper Functions ─────────────────────────────────────

apply_netem() {
    local profile=$1
    # Reset previous rules
    tc qdisc del dev eth0 root 2>/dev/null || true

    case "${profile}" in
      clean)
        echo "[e2e] Network: clean (no emulation)"
        ;;
      wifi_indoor)
        echo "[e2e] Network: WiFi indoor (10ms ±5ms, 0.1% loss)"
        tc qdisc add dev eth0 root netem \
          delay 10ms 5ms distribution normal \
          loss 0.1%
        ;;
      wifi_crowded)
        echo "[e2e] Network: WiFi crowded (50ms ±30ms, 3% loss, reorder)"
        tc qdisc add dev eth0 root netem \
          delay 50ms 30ms distribution normal \
          loss 3% \
          reorder 5% 25%
        ;;
      lte_edge)
        echo "[e2e] Network: LTE edge (100ms ±50ms, 5% burst loss)"
        tc qdisc add dev eth0 root netem \
          delay 100ms 50ms \
          loss gemodel 1% 10% 80% 0%
        ;;
      worst_case)
        echo "[e2e] Network: worst case (200ms ±100ms, 10% loss, corruption)"
        tc qdisc add dev eth0 root netem \
          delay 200ms 100ms distribution normal \
          loss 10% \
          corrupt 2% \
          reorder 10% 50%
        ;;
    esac
}

run_scenario() {
    local profile=$1
    local pcap_file="${PCAP_DIR}/${profile}.pcapng"
    local metrics_file="${RESULTS_DIR}/${profile}_metrics.json"

    echo ""
    echo "[e2e] ══════════════════════════════════════"
    echo "[e2e] Scenario: ${profile}"
    echo "[e2e] ══════════════════════════════════════"

    apply_netem "${profile}"

    # Capture RTP/RTCP traffic in background
    tshark -i eth0 \
        -f "udp port ${RTP_PORT} or udp port ${RTCP_PORT}" \
        -w "${pcap_file}" \
        -s 1500 \
        -q &
    TSHARK_PID=$!

    # Start application (RTP receiver)
    timeout "${DURATION}" /app/video_streaming_app \
        --rtp-port "${RTP_PORT}" \
        --rtcp-port "${RTCP_PORT}" \
        --metrics-out "${metrics_file}" \
        2>&1 | tee "${RESULTS_DIR}/${profile}_app.log" || true

    # Stop capture
    kill "${TSHARK_PID}" 2>/dev/null || true
    wait "${TSHARK_PID}" 2>/dev/null || true

    # Analyze pcap
    python3 /app/tests/scripts/analyze_pcap.py \
        --pcap "${pcap_file}" \
        --rtp-port "${RTP_PORT}" \
        --out "${RESULTS_DIR}/${profile}_pcap_stats.json" || return 1

    # Check thresholds for current profile
    python3 /app/tests/scripts/check_thresholds.py \
        --profile "${profile}" \
        --metrics "${metrics_file}" \
        --pcap-stats "${RESULTS_DIR}/${profile}_pcap_stats.json" || return 1

    echo "[e2e] Scenario ${profile} done"
}

# ── Main Scenario Loop ─────────────────────────────────────

FAILED=0

for profile in clean wifi_indoor wifi_crowded lte_edge worst_case; do
    if run_scenario "${profile}"; then
        echo "[e2e] ✓ ${profile} PASSED"
    else
        echo "[e2e] ✗ ${profile} FAILED" >&2
        FAILED=1
    fi
done

# Reset netem after all tests
tc qdisc del dev eth0 root 2>/dev/null || true

exit ${FAILED}
