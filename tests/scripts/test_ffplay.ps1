# PowerShell скрипт для тестирования FFplay с локальным RTSP сервером

Write-Host "=== FFplay RTSP Test Script ===" -ForegroundColor Green
Write-Host "Testing FFplay connection to local RTSP server" -ForegroundColor Green
Write-Host ""

# Проверка наличия FFplay
$ffplayPath = "ffplay"
if (-not (Get-Command $ffplayPath -ErrorAction SilentlyContinue)) {
    Write-Host "❌ Error: ffplay not found in PATH" -ForegroundColor Red
    Write-Host "   Please install FFmpeg and add to PATH" -ForegroundColor Yellow
    exit 1
}

Write-Host "✅ Found ffplay at: $ffplayPath" -ForegroundColor Green
Write-Host ""

# Проверка работы сервера
$serverUrl = "rtsp://127.0.0.1:8554/live"
Write-Host "🎥 Connecting to: $serverUrl" -ForegroundColor White
Write-Host ""

# Запуск FFplay
Write-Host "🚀 Starting FFplay..." -ForegroundColor Cyan
Write-Host "   Press 'q' in FFplay window to quit" -ForegroundColor Gray
Write-Host ""

try {
    Start-Process -FilePath $ffplayPath -ArgumentList $serverUrl -NoNewWindow -Wait
    Write-Host "✅ FFplay completed successfully" -ForegroundColor Green
} catch {
    Write-Host "❌ FFplay failed with error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "🎉 FFplay test completed!" -ForegroundColor Green
