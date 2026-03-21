#!/usr/bin/env bash
# init-netem.sh — Подготовка сети на хосте для корректной работы тестов
# Запускать на хосте (где установлен Docker) с правами root/sudo.

BRIDGE="lossy0"

# Проверяем, существует ли интерфейс (создается Docker Compose)
if ! ip link show "$BRIDGE" > /dev/null 2>&1; then
    echo "Warning: Interface $BRIDGE not found."
    echo "This is expected on Docker Desktop (Windows/Mac). Host-side optimizations will be skipped."
    echo "Tests will continue using container-level emulation."
    exit 0
fi

echo "Configuring $BRIDGE for accurate network emulation..."

# Отключаем offloading (TSO, GSO, GRO).
# Если этого не сделать, сетевая карта/драйвер могут объединять пакеты в большие сегменты,
# и tc netem будет задерживать их "пачками", а не поштучно, что исказит тесты.
ethtool -K "$BRIDGE" tso off gso off gro off > /dev/null 2>&1 || echo "Warning: Could not disable offloading (ethtool missing?)"

# Очищаем возможные старые правила на самом мосту (правила для тестов накладываются внутри контейнера)
tc qdisc del dev "$BRIDGE" root 2>/dev/null || true

echo "Network $BRIDGE is ready."
