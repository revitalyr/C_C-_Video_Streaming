#!/bin/sh

# Determine script directory and project root
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Сборка образа Docker
docker build -f "$SCRIPT_DIR/Dockerfile" -t my-demo-app "$PROJECT_ROOT"

# Запуск контейнера Docker
docker run --rm -p 8554:8554 my-demo-app
