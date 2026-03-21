#!/usr/bin/env bash
# run_all.sh — главный entrypoint: unit → e2e → отчёт
set -euo pipefail

RESULTS_DIR="/app/results"
mkdir -p "${RESULTS_DIR}"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log()  { echo -e "${GREEN}[run_all]${NC} $*"; }
warn() { echo -e "${YELLOW}[run_all]${NC} $*"; }
fail() { echo -e "${RED}[run_all]${NC} $*" >&2; }

FAILED=0

# ── 1. Unit-тесты (Catch2) ──────────────────────────────────────
log "=== Stage 1: Unit tests ==="
if bash /app/tests/scripts/run_unit.sh; then
    log "Unit tests PASSED"
else
    fail "Unit tests FAILED"
    FAILED=1
fi

# ── 2. E2E тесты (только если unit прошли) ──────────────────────
if [[ ${FAILED} -eq 0 ]]; then
    log "=== Stage 2: E2E / network tests ==="
    if bash /app/tests/scripts/run_e2e.sh; then
        log "E2E tests PASSED"
    else
        fail "E2E tests FAILED"
        FAILED=1
    fi
fi

# ── 3. Анализ RTCP метрик ───────────────────────────────────────
if [[ ${FAILED} -eq 0 ]]; then
    log "=== Stage 3: RTCP analysis ==="
    bash /app/tests/scripts/analyze_rtcp.sh || warn "RTCP analysis had warnings"
fi

# ── 4. Итоговый отчёт ───────────────────────────────────────────
log "=== Summary ==="
if [[ -f "${RESULTS_DIR}/summary.json" ]]; then
    jq '.' "${RESULTS_DIR}/summary.json"
fi

exit ${FAILED}
