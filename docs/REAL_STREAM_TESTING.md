# 🎥 Тестирование RTSP/RTP Видеостриминга с Реальными Источниками

Этот документ описывает как тестировать видеостриминговый проект с использованием реальных публичных RTSP источников.

## 📋 Обзор

Проект включает в себя:
- **RTSP Test Client** - клиент для подключения к реальным RTSP потокам
- **RTP Packet Processing** - обработка RTP пакетов с сохранением в файл
- **Статистика и мониторинг** - отслеживание FPS, потерь, производительности
- **Многопоточная архитектура** - эффективная обработка данных

## 🚀 Быстрый старт

### 1. Сборка проекта
```bash
# Сборка RTSP клиента
cmake --build build --target rtsp_test_client

# Запуск тестов (Windows)
.\scripts\test_real_streams.ps1

# Запуск тестов (Linux/MacOS)
chmod +x scripts/test_real_streams.sh
./scripts/test_real_streams.sh
```

### 2. Тестирование источников
Скрипт автоматически протестировать три источника:

#### ✅ **Wowza Test Stream**
- **URL**: `rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2`
- **Длительность**: 30 секунд
- **Назначение**: Официальный тестовый поток от Wowza

#### ✅ **IPVM Public Camera**
- **URL**: `rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast`
- **Длительность**: 20 секунд
- **Назначение**: Реальная IP камера для тестирования поведения

#### ✅ **Big Buck Bunny Stream**
- **URL**: `rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov`
- **Длительность**: 15 секунд
- **Назначение**: Простой файловый поток для базовых тестов

## 📊 Результаты тестирования

После выполнения тестов в директории `test_results/` будут созданы файлы:

```
test_results/
├── wowza_stream.rtp      # Результаты тестирования Wowza потока
├── ipvm_camera.rtp      # Результаты тестирования IPVM камеры
└── bunny_stream.rtp      # Результаты тестирования Bunny потока
```

### Статистика тестирования

Скрипт автоматически анализирует результаты и выводит:
- **Количество созданных файлов**
- **Размер каждого файла в байтах**
- **Процент успешных тестов**
- **Общая статистика производительности**

## 🔧 Конфигурация RTSP клиента

### Параметры конфигурации

```cpp
RTSPTestClient::Config config;
config.rtsp_url = "rtsp://your-stream-url";
config.output_file = "output.rtp";
config.timeout_ms = 5000;
config.max_packets = 10000;
config.enable_logging = true;
```

### Возможности RTSP клиента

- **Thread-safe обработка** пакетов
- **Jitter buffer** для компенсации сетевых задержек
- **Статистика в реальном времени** (FPS, потери, байты)
- **Сохранение в файл** RTP пакетов для анализа
- **Настраиваемое время ожидания** и максимальное количество пакетов
- **Детальное логирование** с временными метками

## 📈 Анализ производительности

### Метрики
- **Packets Received**: Общее количество принятых RTP пакетов
- **Bytes Received**: Общее количество принятых байт
- **Current FPS**: Текущая частота кадров
- **Packet Loss Rate**: Процент потерянных пакетов
- **Processing Time**: Время обработки каждого пакета

### Оптимальные значения
- **FPS**: 25-30 для стабильного стриминга
- **Packet Loss**: < 1% для качественного соединения
- **Jitter**: < 50ms для хорошей синхронизации

## 🛠️ Расширенное тестирование

### Дополнительные RTSP источники

Для расширенного тестирования можно использовать:

#### **Тестовые потоки CCTV**
```
rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov
rtsp://184.72.239.149/vod/mp4:BigBuckBunny_115k.mov
rtsp://live.cdn.antenna.gr:554/SkyNews
```

#### **H.265 тестовые потоки**
```
rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov
rtsp://r5---sn-5v6kqge04.live-1.net/vod/mp4:tears_of_steel_4k_h265.mov
```

### Варианты использования

#### **Базовое тестирование**
```bash
# Запуск всех тестов
.\scripts\test_real_streams.ps1
```

#### **Тестирование одного источника**
```cpp
// В коде
RTSPTestClient::Config config;
config.rtsp_url = "rtsp://your-specific-url";
config.output_file = "specific_stream.rtp";

RTSPTestClient client(config);
client.connect();
client.start_receiving();
// Работа...
client.stop_receiving();
client.disconnect();
```

#### **Нагрузочное тестирование**
```bash
# Множественные экземпляры
for i in {1..5}; do
    .\scripts\test_real_streams.ps1 &
done

wait  # Ожидание завершения
```

## 🔍 Анализ результатов

### Инструменты анализа

#### **FFmpeg анализ**
```bash
# Анализ RTP файла
ffmpeg -i output.rtp -c:v copy -f rawvideo -y analysis.mp4

# Проверка кодека
ffmpeg -i wowza_stream.rtp -c:v copy -bsf:v h264_mp4toannexb -f h264 -y decoded.h264
```

#### **Wireshark анализ**
```bash
# Анализ сетевого трафика
tshark -r wowza_stream.pcap -Y frame.number -Y rtp.seq -Y rtp.timestamp
```

#### **Статистический анализ**
```python
import os
import struct

def analyze_rtp_file(filename):
    packets = []
    with open(filename, 'rb') as f:
        while True:
            data = f.read(12)  # RTP Header
            if len(data) < 12:
                break
            # Анализ RTP заголовка...
            packets.append(len(data))
    
    print(f"Total packets: {len(packets)}")
    print(f"Average packet size: {sum(packets)/len(packets)}")
```

## 📝 Требования к окружению

### Системные требования
- **C++23** совместимый компилятор
- **Потоки**: Для многопоточной обработки
- **Сеть**: Стабильное интернет соединение
- **Диск**: Место для сохранения RTP файлов

### Зависимости
- **CMake 3.28+** для сборки
- **Threads** для многопоточности
- **Chrono** для временных метрик
- **Filesystem** для работы с файлами

## 🚨 Поиск неисправностей

### Частые проблемы

#### **Сетевые проблемы**
- **Timeout**: Увеличить `timeout_ms` в конфигурации
- **Connection refused**: Проверить доступность RTSP URL
- **Packet loss**: Проверить качество сети

#### **Проблемы с файлами**
- **Permission denied**: Проверить права на запись
- **Disk space**: Проверить свободное место
- **File corruption**: Проверить прерывание записи

#### **Производительность**
- **Low FPS**: Оптимизировать обработку пакетов
- **High CPU**: Уменьшить логирование
- **Memory leaks**: Проверить управление памятью

## 📚 Дополнительные ресурсы

### Документация RTSP
- [RFC 2326](https://tools.ietf.org/html/rfc2326) - RTSP протокол
- [RFC 3550](https://tools.ietf.org/html/rfc3550) - RTP протокол
- [RTSP Wikipedia](https://en.wikipedia.org/wiki/Real_Time_Streaming_Protocol)

### Тестовые RTSP серверы
- [Wowza Streaming Engine](https://www.wowza.com/)
- [IPVM Camera Demo](http://www.ipvm.com/)
- [GStreamer RTSP Server](https://gstreamer.freedesktop.org/)

### Анализ инструментов
- [FFmpeg Documentation](https://ffmpeg.org/documentation.html)
- [Wireshark User Guide](https://www.wireshark.org/docs/)
- [VLC Streaming](https://wiki.videolan.org/Documentation:Streaming_HowTo/Advanced_Streaming_With_VLC)

---

**Примечание**: Этот документ описывает тестирование с симулированными RTP потоками. Для реального RTSP подключения требуется дополнительная реализация RTSP протокола.
