# 🎥 Локальное тестирование RTSP/RTP стриминга

Это руководство описывает как настроить локальный RTSP сервер с помощью FFmpeg и протестировать видеостриминг с использованием локального видеофайла.

## 📋 Обзор

Локальное тестирование позволяет:
- **Тестировать без интернет соединения**
- **Использовать локальные видеофайлы**
- **Полностью контролировать тестовую среду**
- **Быстро итерировать и отлаживать**
- **Избежать проблем с внешними источниками**

## 🚀 Быстрый старт

### 1. Подготовка окружения

#### **Установка FFmpeg**
```bash
# Windows (скачать с официального сайта)
# https://ffmpeg.org/download.html
# Добавить ffmpeg.exe в PATH

# Проверка установки
ffmpeg -version
```

#### **Проверка видеофайла**
```bash
# Проверка наличия тестового файла
Get-ChildItem tests\data\oobe-intro.mp4

# Информация о файле
ffprobe tests\data\oobe-intro.mp4
```

### 2. Запуск локального RTSP сервера

#### **Базовый запуск**
```powershell
# Запуск сервера с параметрами по умолчанию
.\scripts\start_local_rtsp_server.ps1
```

#### **Параметры запуска**
```powershell
# Указание видеофайла
.\scripts\start_local_rtsp_server.ps1 -VideoFile "tests\data\oobe-intro.mp4"

# Указание портов
.\scripts\start_local_rtsp_server.ps1 -RTSPPort 8554 -RTPPort 5000

# Остановка предыдущих процессов
.\scripts\start_local_rtsp_server.ps1 -Kill
```

#### **Результат запуска**
```
=== Local RTSP/RTP Server ===
Starting FFmpeg RTSP server for local video streaming

📹 Video file: D:\work\Projects\C_C++_Video_Streaming\tests\data\oobe-intro.mp4
🌐 RTSP Port: 8554
📡 RTP Port: 5000

✅ FFmpeg found
🚀 Starting FFmpeg RTSP server...
Command: ffmpeg -re -stream_loop -1 -i tests\data\oobe-intro.mp4 -c:v libx264 ...

✅ RTSP server started!
📡 RTSP URL: rtsp://localhost:8554/live
📹 Streaming: tests\data\oobe-intro.mp4
```

### 3. Тестирование RTSP клиента

#### **Тест только локального источника**
```powershell
# Тестирование только локального RTSP сервера
.\scripts\test_real_rtsp.ps1 -Source local
```

#### **Полное тестирование (включая локальный)**
```powershell
# Все источники включая локальный
.\scripts\test_real_rtsp.ps1 -Source all

# Очистка результатов
.\scripts\test_real_rtsp.ps1 -Clean
```

## 🔧 Подробная конфигурация

### **FFmpeg параметры для RTSP сервера**

```powershell
# Основные параметры FFmpeg:
$ffmpegArgs = @(
    "-re",                    # Читать файл с правильной скоростью
    "-stream_loop", "-1",     # Бесконечный цикл
    "-i", $VideoFile,         # Входной файл
    "-c:v", "libx264",        # Кодек H.264
    "-preset", "ultrafast",    # Быстрая кодировка
    "-tune", "zerolatency", # Минимальная задержка
    "-pix_fmt", "yuv420p",   # Формат пикселей
    "-g", "30",               # Размер GOP
    "-b:v", "2000k",          # Битрейт 2 Mbps
    "-f", "rtsp",              # Формат RTSP
    "-rtsp_transport", "tcp",   # Транспорт TCP
    "rtsp://localhost:8554/live"  # RTSP URL
)
```

### **Настройка качества стриминга**

#### **Высокое качество**
```powershell
# Для тестирования качества видео
"-preset", "slow",
"-crf", "18",
"-b:v", "5000k",
"-maxrate", "5000k",
"-bufsize", "10000k"
```

#### **Низкая задержка**
```powershell
# Для реального времени
"-preset", "ultrafast",
"-tune", "zerolatency",
"-g", "15",
"-b:v", "1000k"
```

#### **Минимальный битрейт**
```powershell
# для экономии трафика
"-b:v", "500k",
"-maxrate", "500k",
"-bufsize", "1000k"
```

## 📊 Тестовые сценарии

### **Сценарий 1: Базовое тестирование**
```powershell
# 1. Запуск RTSP сервера
.\scripts\start_local_rtsp_server.ps1

# 2. В новом терминале - тестирование клиента
.\scripts\test_real_rtsp.ps1 -Source local

# 3. Анализ результатов
python scripts\analyze_rtp_files.py test_results\local_stream.rtp
```

### **Сценарий 2: Сравнительное тестирование**
```powershell
# 1. Тестирование всех источников
.\scripts\test_real_rtsp.ps1 -Source all

# 2. Сравнение результатов
python scripts\analyze_rtp_files.py test_results\*.rtp --compare

# 3. Анализ производительности
python scripts\analyze_rtp_files.py test_results\local_stream.rtp --detailed
```

### **Сценарий 3: Нагрузочное тестирование**
```powershell
# 1. Запуск нескольких клиентов
for ($i=1; $i -le 3; $i++) {
    Start-Job -ScriptBlock { 
        .\scripts\test_real_rtsp.ps1 -Source local 
    }
}

# 2. Ожидание завершения
Wait-Job -Name "*"
Receive-Job -Name "*"

# 3. Анализ результатов
Get-ChildItem test_results\local_stream*.rtp
```

## 🎯 Доступные RTSP URL

### **Локальный сервер**
```
rtsp://localhost:8554/live
```

### **Публичные источники**
```
rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2
rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast
rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov
```

## 📁 Структура файлов

```
tests/
├── data/
│   └── oobe-intro.mp4          # Тестовый видеофайл
├── scripts/
│   ├── start_local_rtsp_server.ps1  # Скрипт запуска сервера
│   └── test_real_rtsp.ps1         # Скрипт тестирования
└── results/
    ├── local_stream.rtp         # Результат локального теста
    ├── wowza_stream.rtp       # Результат Wowza теста
    └── ipvm_camera.rtp        # Результат IPVM теста
```

## 🛠️ Поиск неисправностей

### **Проблемы с FFmpeg**

#### **FFmpeg не найден**
```bash
# Проверка установки
where ffmpeg

# Добавление в PATH
# Windows: Системные свойства → Переменные среды → PATH
```

#### **Ошибка запуска**
```bash
# Проверка видеофайла
ffprobe tests\data\oobe-intro.mp4

# Проверка портов
netstat -an | findstr 8554
```

### **Проблемы с RTSP клиентом**

#### **Нет подключения**
```bash
# Проверка RTSP URL
ffplay rtsp://localhost:8554/live

# Проверка сетевого соединения
telnet localhost 8554
```

#### **Нет RTP пакетов**
```bash
# Проверка FFmpeg логов
Get-Content logs\rtsp_server.log

# Проверка процесса RTP
netstat -an | findstr 5000
```

### **Проблемы с качеством**

#### **Низкий FPS**
```bash
# Увеличение битрейта
"-b:v", "3000k"

# Уменьшение GOP
"-g", "15"
```

#### **Высокая задержка**
```bash
# Настройка для минимальной задержки
"-preset", "ultrafast",
"-tune", "zerolatency",
"-x264opts", "nal-hrd=cbr"
```

## 📈 Анализ результатов

### **Статистика RTP пакетов**
```bash
# Базовый анализ
python scripts\analyze_rtp_files.py test_results\local_stream.rtp

# Сравнительный анализ
python scripts\analyze_rtp_files.py test_results\*.rtp --compare

# Детальный анализ
python scripts\analyze_rtp_files.py test_results\local_stream.rtp --detailed
```

### **Метрики качества**
- **Packet Loss Rate**: < 1% для идеального соединения
- **Jitter**: < 50ms для стабильного стриминга
- **FPS**: 25-30 для плавного видео
- **Bitrate**: Соответствие настройкам FFmpeg

### **Оптимизация**
```bash
# Анализ производительности
ffmpeg -i test_results\local_stream.rtp -c copy -f null -

# Конвертация для проверки
ffmpeg -i test_results\local_stream.rtp -c copy output.mp4
```

## 🚀 Расширенное использование

### **Множественные видеофайлы**
```powershell
# Создание плейлиста для FFmpeg
# playlist.txt
file 'tests\data\video1.mp4'
file 'tests\data\video2.mp4'

# Запуск с плейлистом
ffmpeg -re -f concat -i playlist.txt -c:v libx264 -f rtsp rtsp://localhost:8554/live
```

### **Аудио стриминг**
```powershell
# Добавление аудио потока
"-c:a", "aac",
"-b:a", "128k",
"-ar", "44100"
```

### **Множественные RTSP потоки**
```powershell
# Запуск нескольких серверов на разных портах
Start-Job { .\scripts\start_local_rtsp_server.ps1 -RTSPPort 8554 }
Start-Job { .\scripts\start_local_rtsp_server.ps1 -RTSPPort 8555 }
Start-Job { .\scripts\start_local_rtsp_server.ps1 -RTSPPort 8556 }
```

---

**Примечание**: Локальное тестирование идеально подходит для разработки и отладки видеостриминговых приложений без зависимости от внешних сетевых ресурсов.
