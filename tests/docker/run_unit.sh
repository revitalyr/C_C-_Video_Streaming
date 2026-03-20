#!/usr/bin/env bash
# run_unit.sh — запускает Catch2 бинарник, пишет JUnit XML
set -euo pipefail

RESULTS_DIR="/app/results"
mkdir -p "${RESULTS_DIR}"

echo "[unit] Running Catch2 tests..."

/app/video_streaming_tests \
    --reporter junit \
    --out "${RESULTS_DIR}/unit-tests.xml" \
    --reporter console \
    -v normal

EXIT_CODE=$?

# Извлекаем краткую статистику из XML для summary.json
TOTAL=$(grep -oP 'tests="\K[0-9]+' "${RESULTS_DIR}/unit-tests.xml" | head -1 || echo 0)
FAILED=$(grep -oP 'failures="\K[0-9]+' "${RESULTS_DIR}/unit-tests.xml" | head -1 || echo 0)
ERRORS=$(grep -oP 'errors="\K[0-9]+' "${RESULTS_DIR}/unit-tests.xml" | head -1 || echo 0)

echo "[unit] Results: total=${TOTAL} failed=${FAILED} errors=${ERRORS}"

# Пишем в общий summary
python3 - <<EOF
import json, os

summary_path = "${RESULTS_DIR}/summary.json"
summary = json.load(open(summary_path)) if os.path.exists(summary_path) else {}
summary["unit"] = {
    "total": int("${TOTAL}" or 0),
    "failed": int("${FAILED}" or 0),
    "errors": int("${ERRORS}" or 0),
    "passed": int("${TOTAL}" or 0) - int("${FAILED}" or 0) - int("${ERRORS}" or 0),
}
json.dump(summary, open(summary_path, "w"), indent=2)
EOF

exit ${EXIT_CODE}
