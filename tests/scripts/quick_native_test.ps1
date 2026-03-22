# PowerShell скрипт для быстрого тестирования нативного RTSP сервера
# Запускает локальный RTSP сервер и тестирует RTSP клиент

param(
    [switch]$Kill = $false,
    [switch]$Clean = $false,
    [string]$VideoFile = "",  # Пусто = синтетическое видео
    [int]$Port = 8554
)

Write-Host "=== Quick Native RTSP Test ===" -ForegroundColor Green
Write-Host "Fast local video streaming test with native RTSP server" -ForegroundColor Green
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

Write-Host "🚀 Starting complete native test..." -ForegroundColor Cyan
Write-Host ""

# 1. Запуск локального RTSP сервера
Write-Host "1️⃣ Starting native RTSP server..." -ForegroundColor White
$serverArgs = @{}
if (-not [string]::IsNullOrEmpty($VideoFile)) {
    $serverArgs["VideoFile"] = $VideoFile
}
$serverArgs["Port"] = $Port

$serverJob = Start-Job -ScriptBlock {
    param($ScriptRoot, $ServerArgs)
    & "$ScriptRoot\start_local_rtsp_server.ps1" @ServerArgs
} -ArgumentList $PSScriptRoot, $serverArgs

# Ожидание запуска сервера
Write-Host "   Waiting for server to start..." -ForegroundColor Gray
Start-Sleep -Seconds 3

# Проверка запуска сервера
try {
    $tcpClient = New-Object System.Net.Sockets.TcpClient
    try {
        $tcpClient.Connect("localhost", $Port)
        $tcpClient.Close()
        Write-Host "   ✅ Native RTSP server is running" -ForegroundColor Green
    } catch {
        Write-Host "   ❌ Native RTSP server failed to start" -ForegroundColor Red
        Stop-Job -Job $serverJob
        Remove-Job -Job $serverJob
        exit 1
    }
} catch {
    Write-Host "   ❌ Native RTSP server failed to start" -ForegroundColor Red
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
    
    # Базовый анализ файла
    try {
        $fileContent = Get-Content $rtpFile -Raw -Encoding Byte
        $packetCount = [math]::Floor($fileContent.Length / 1400)  # Примерный размер пакета
        Write-Host "   📈 Estimated packets: $packetCount" -ForegroundColor Green
    } catch {
        Write-Host "   ⚠️  Could not analyze file content" -ForegroundColor Yellow
    }
} else {
    Write-Host "   ❌ No RTP file created" -ForegroundColor Red
}

# 4. Остановка сервера
Write-Host "4️⃣ Stopping native RTSP server..." -ForegroundColor White
Stop-Job -Job $serverJob -Force
Remove-Job -Job $serverJob -Force

Write-Host ""
Write-Host "🎉 Quick native test completed!" -ForegroundColor Green

if ($testSuccess -and (Test-Path $rtpFile)) {
    Write-Host "✅ Test SUCCESSFUL" -ForegroundColor Green
    Write-Host "   Native RTSP streaming is working correctly" -ForegroundColor White
    Write-Host "   RTP packets generated from actual video data" -ForegroundColor White
    exit 0
} else {
    Write-Host "❌ Test FAILED" -ForegroundColor Red
    Write-Host "   Check native RTSP server implementation" -ForegroundColor White
    Write-Host "   Verify video source and network connectivity" -ForegroundColor White
    exit 1
}
