#!/bin/sh

# Сборка образа Docker
docker build -t my-demo-app .

# Запуск контейнера Docker
docker run --rm -p 3000:3000 my-demo-app
