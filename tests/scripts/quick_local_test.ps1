# PowerShell скрипт для быстрого локального тестирования
# Запускает локальный RTSP сервер и тестирует RTSP клиент

param(
    [switch]$Kill = $false,
    [switch]$Clean = $false,
    [string]$VideoFile = "tests\data\oobe-intro.mp4"
)

Write-Host "=== Quick Local RTSP Test ===" -ForegroundColor Green
Write-Host "Fast local video streaming test" -ForegroundColor Green
Write-Host ""

# Остановка процессов
if ($Kill) {
    Write-Host "🔄 Stopping all processes..." -ForegroundColor Yellow
    & "$PSScriptRoot\start_local_rtsp_server.ps1" -Kill
    exit 0
}

# Очистка результатов
if ($Clean) {
    Write-Host "🧹 Cleaning results..." -ForegroundColor Yellow
    & "$PSScriptRoot\test_real_rtsp.ps1" -Clean
}

Write-Host "🚀 Starting complete local test..." -ForegroundColor Cyan
Write-Host ""

# 1. Запуск локального RTSP сервера
Write-Host "1️⃣ Starting local RTSP server..." -ForegroundColor White
$serverJob = Start-Job -ScriptBlock {
    param($ScriptRoot, $VideoFile)
    & "$ScriptRoot\start_local_rtsp_server.ps1" -VideoFile $VideoFile
} -ArgumentList $PSScriptRoot, $VideoFile

# Ожидание запуска сервера
Write-Host "   Waiting for server to start..." -ForegroundColor Gray
Start-Sleep -Seconds 5

# Проверка запуска сервера
try {
    # RTSP не поддерживает HTTP, но проверка порта
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    try {
        $tcpClient.Connect("localhost", 8554)
        $tcpClient.Close()
        Write-Host "   ✅ RTSP server is running" -ForegroundColor Green
    } catch {
        Write-Host "   ❌ RTSP server failed to start" -ForegroundColor Red
        Stop-Job -Job $serverJob
        Remove-Job -Job $serverJob
        exit 1
    }
} catch {
    Write-Host "   ❌ RTSP server failed to start" -ForegroundColor Red
    Stop-Job -Job $serverJob
    Remove-Job -Job $serverJob
    exit 1
}

# 2. Тестирование RTSP клиента
Write-Host "2️⃣ Testing RTSP client..." -ForegroundColor White
try {
    & "$PSScriptRoot\test_real_rtsp.ps1" -Source local
    $testSuccess = $true
} catch {
    Write-Host "   ❌ RTSP client test failed" -ForegroundColor Red
    $testSuccess = $false
}

# 3. Анализ результатов
Write-Host "3️⃣ Analyzing results..." -ForegroundColor White
$rtpFile = "test_results\local_stream.rtp"

if (Test-Path $rtpFile) {
    $fileSize = (Get-Item $rtpFile).Length
    Write-Host "   📊 RTP file size: $fileSize bytes" -ForegroundColor Cyan
    
    # Запуск Python анализа если доступен
    try {
        $analysis = & python "$PSScriptRoot\analyze_rtp_files.py" $rtpFile 2>&1
        Write-Host "   📈 Analysis completed" -ForegroundColor Green
        Write-Host $analysis -ForegroundColor Gray
    } catch {
        Write-Host "   ⚠️  Python analysis not available" -ForegroundColor Yellow
    }
} else {
    Write-Host "   ❌ No RTP file created" -ForegroundColor Red
}

# 4. Остановка сервера
Write-Host "4️⃣ Stopping RTSP server..." -ForegroundColor White
Stop-Job -Job $serverJob -Force
Remove-Job -Job $serverJob -Force

Write-Host ""
Write-Host "🎉 Quick local test completed!" -ForegroundColor Green

if ($testSuccess -and (Test-Path $rtpFile)) {
    Write-Host "✅ Test SUCCESSFUL" -ForegroundColor Green
    Write-Host "   Local RTSP streaming is working correctly" -ForegroundColor White
    exit 0
} else {
    Write-Host "❌ Test FAILED" -ForegroundColor Red
    Write-Host "   Check FFmpeg installation and video file" -ForegroundColor White
    exit 1
}
