# PowerShell скрипт для запуска локального RTSP/RTP сервера
# Использует FFmpeg для стриминга локального видеофайла

param(
    [string]$VideoFile = "tests\data\oobe-intro.mp4",
    [int]$RTSPPort = 8554,
    [int]$RTPPort = 5000,
    [switch]$Kill = $false
)

Write-Host "=== Local RTSP/RTP Server ===" -ForegroundColor Green
Write-Host "Starting FFmpeg RTSP server for local video streaming" -ForegroundColor Green
Write-Host ""

# Проверка наличия видеофайла
$fullVideoPath = Join-Path $PSScriptRoot $VideoFile
if (-not (Test-Path $fullVideoPath)) {
    Write-Host "❌ Error: Video file not found: $fullVideoPath" -ForegroundColor Red
    exit 1
}

Write-Host "📹 Video file: $fullVideoPath" -ForegroundColor White
Write-Host "🌐 RTSP Port: $RTSPPort" -ForegroundColor White
Write-Host "📡 RTP Port: $RTPPort" -ForegroundColor White
Write-Host ""

# Остановка предыдущих процессов
if ($Kill) {
    Write-Host "🔄 Killing existing FFmpeg processes..." -ForegroundColor Yellow
    
    Get-Process -Name ffmpeg -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.Id -Force
        Write-Host "  Stopped process $($_.Id)" -ForegroundColor Gray
    }
    
    Start-Sleep -Seconds 2
    Write-Host "✅ Previous processes stopped" -ForegroundColor Green
    exit 0
}

# Проверка FFmpeg
try {
    &ffmpeg -version 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "FFmpeg not found"
    }
    Write-Host "✅ FFmpeg found" -ForegroundColor Green
} catch {
    Write-Host "❌ Error: FFmpeg not found. Please install FFmpeg:" -ForegroundColor Red
    Write-Host "   Download from: https://ffmpeg.org/download.html" -ForegroundColor Yellow
    Write-Host "   Add to PATH or use full path to ffmpeg.exe" -ForegroundColor Yellow
    exit 1
}

# Создание директории для логов
$logDir = "logs"
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
}

$logFile = Join-Path $logDir "rtsp_server.log"

# Формирование FFmpeg команды для RTSP сервера
$ffmpegArgs = @(
    "-re",                    # Читать файл с правильной скоростью
    "-stream_loop", "-1",     # Бесконечный цикл
    "-i", $fullVideoPath,     # Входной файл
    "-c:v", "libx264",        # Кодек H.264
    "-preset", "ultrafast",    # Быстрая кодировка для стриминга
    "-tune", "zerolatency", # Минимальная задержка
    "-pix_fmt", "yuv420p",   # Формат пикселей
    "-g", "30",               # Размер GOP
    "-keyint_min", "30",       # Минимальный интервал ключевых кадров
    "-sc_threshold", "0",       # Отключить детекцию сцен
    "-b:v", "2000k",          # Битрейт 2 Mbps
    "-maxrate", "2000k",       # Максимальный битрейт
    "-bufsize", "4000k",       # Размер буфера
    "-f", "rtsp",              # Формат RTSP
    "-rtsp_transport", "tcp",   # Транспорт TCP
    "rtsp://:$RTSPPort/live"  # RTSP URL
)

Write-Host "🚀 Starting FFmpeg RTSP server..." -ForegroundColor Cyan
Write-Host "Command: ffmpeg $($ffmpegArgs -join ' ')" -ForegroundColor Gray
Write-Host ""

# Запуск FFmpeg в фоновом режиме
$ffmpegProcess = Start-Process -FilePath "ffmpeg" -ArgumentList $ffmpegArgs -NoNewWindow -PassThru

if ($null -eq $ffmpegProcess) {
    Write-Host "❌ Failed to start FFmpeg" -ForegroundColor Red
    exit 1
}

Write-Host "✅ RTSP server started!" -ForegroundColor Green
Write-Host "📡 RTSP URL: rtsp://localhost:$RTSPPort/live" -ForegroundColor Cyan
Write-Host "📹 Streaming: $fullVideoPath" -ForegroundColor White
Write-Host ""
Write-Host "📝 Server Information:" -ForegroundColor Yellow
Write-Host "   Process ID: $($ffmpegProcess.Id)" -ForegroundColor White
Write-Host "   Log file: $logFile" -ForegroundColor White
Write-Host ""
Write-Host "🎮 Test URLs:" -ForegroundColor Cyan
Write-Host "   RTSP: rtsp://localhost:$RTSPPort/live" -ForegroundColor White
Write-Host "   For testing with: .\scripts\test_real_rtsp.ps1 -Source custom" -ForegroundColor White
Write-Host ""
Write-Host "⏹️  Server is running. Press Ctrl+C to stop." -ForegroundColor Green
Write-Host "📋 To stop server: .\scripts\start_local_rtsp_server.ps1 -Kill" -ForegroundColor Gray
Write-Host ""

# Ожидание завершения процесса
try {
    $ffmpegProcess | Out-Null
    $ffmpegProcess.WaitForExit()
} catch [System.Management.Automation.StopActionException] {
    Write-Host ""
    Write-Host "🛑 Server stopped by user" -ForegroundColor Yellow
} catch {
    Write-Host ""
    Write-Host "❌ FFmpeg process exited with code: $($ffmpegProcess.ExitCode)" -ForegroundColor Red
}

Write-Host ""
Write-Host "🏁 RTSP server stopped" -ForegroundColor Red
