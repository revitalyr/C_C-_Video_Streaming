# Shortcuts для работы с Docker окружением тестов
# Использование: make <target>

.PHONY: build test test-unit test-e2e test-stress clean logs results

# ── Сборка ────────────────────────────────────────────────────
build:
	docker compose build

# ── Запуск всех тестов (unit + e2e) ──────────────────────────
test:
	docker compose run --rm unit-tests

# ── Только unit-тесты (быстро, без сети) ─────────────────────
test-unit:
	docker compose run --rm unit-tests \
		bash /app/tests/scripts/run_unit.sh

# ── Только e2e с поднятым RTP источником ─────────────────────
test-e2e:
	docker compose up --abort-on-container-exit \
		rtp-source receiver analyzer

# ── Стресс-тест IoT сценария (25% CPU, 128MB RAM) ────────────
test-stress:
	docker compose --profile stress up --abort-on-container-exit \
		rtp-source stress-iot

# ── Показать результаты последнего прогона ────────────────────
results:
	@docker compose run --rm --no-deps unit-tests \
		sh -c 'cat /app/results/summary.json 2>/dev/null || echo "No results yet"'

# ── Логи всех сервисов ────────────────────────────────────────
logs:
	docker compose logs --tail=100 -f

# ── Очистка ───────────────────────────────────────────────────
clean:
	docker compose down -v --remove-orphans
	docker image rm videostreaming-builder videostreaming-tests 2>/dev/null || true
