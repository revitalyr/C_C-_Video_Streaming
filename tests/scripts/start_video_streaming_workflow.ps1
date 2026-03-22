# PowerShell скрипт для запуска полного Video Streaming Workflow
# 📷 Embedded camera simulator -> 🌐 Streaming server -> 🖥 Viewer client -> 📊 Metrics

param(
    [switch]$Kill = $false,
    [switch]$NoMetrics = $false,
    [int]$RelayPort = 8555,
    [int]$MetricsPort = 8080,
    [string]$TargetBitrate = "1000",
    [string]$MinBitrate = "100",
    [string]$MaxBitrate = "5000"
)

Write-Host "🎬 Video Streaming Workflow Launcher" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""

# Проверка наличия исполняемых файлов
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $scriptDir "..\..\build"

$components = @{
    "RTSP Server" = Join-Path $buildDir "local_rtsp_server.exe"
    "Viewer Client" = Join-Path $buildDir "viewer_client.exe"
    "Streaming Server" = Join-Path $buildDir "streaming_server.exe"
    "Metrics System" = Join-Path $buildDir "metrics_adaptive.exe"
}

$allBuilt = $true
foreach ($component in $components.GetEnumerator()) {
    if (-not (Test-Path $component.Value)) {
        Write-Host "❌ Missing component: $($component.Key)" -ForegroundColor Red
        Write-Host "   Path: $($component.Value)" -ForegroundColor Yellow
        $allBuilt = $false
    } else {
        Write-Host "✅ Found component: $($component.Key)" -ForegroundColor Green
    }
}

if (-not $allBuilt) {
    Write-Host ""
    Write-Host "❌ Some components are missing. Please build first:" -ForegroundColor Red
    Write-Host "   cmake --build build" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
Write-Host "🚀 Starting Video Streaming Workflow..." -ForegroundColor Cyan
Write-Host ""

# Остановка предыдущих процессов
if ($Kill) {
    Write-Host "🔄 Stopping previous processes..." -ForegroundColor Yellow
    
    Get-Process -Name local_rtsp_server -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.Id -Force
        Write-Host "  Stopped local_rtsp_server (PID: $($_.Id))" -ForegroundColor Gray
    }
    
    Get-Process -Name viewer_client -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.Id -Force
        Write-Host "  Stopped viewer_client (PID: $($_.Id))" -ForegroundColor Gray
    }
    
    Get-Process -Name streaming_server -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.Id -Force
        Write-Host "  Stopped streaming_server (PID: $($_.Id))" -ForegroundColor Gray
    }
    
    Get-Process -Name metrics_adaptive -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.Id -Force
        Write-Host "  Stopped metrics_adaptive (PID: $($_.Id))" -ForegroundColor Gray
    }
    
    Start-Sleep -Seconds 2
    Write-Host "✅ Previous processes stopped" -ForegroundColor Green
    Write-Host ""
}

# Компоненты workflow
$workflowComponents = @()

# 1. 📷 Embedded Camera Simulator (RTSP Server)
Write-Host "📷 Starting Embedded Camera Simulator (RTSP Server)..." -ForegroundColor Cyan
$rtspServer = Start-Process -FilePath $components["RTSP Server"] -NoNewWindow -PassThru
$workflowComponents += @{
    Name = "RTSP Server"
    Process = $rtspServer
    URL = "rtsp://127.0.0.1:8554/live"
    Description = "Local RTSP server with synthetic H.264 streaming"
}
Write-Host "   ✅ RTSP Server started (PID: $($rtspServer.Id))" -ForegroundColor Green
Write-Host "   📡 URL: rtsp://127.0.0.1:8554/live" -ForegroundColor White

# Ждем запускания RTSP сервера
Start-Sleep -Seconds 3

# 2. 🌐 Streaming Server (Relay/NAT Traversal)
Write-Host "🌐 Starting Streaming Server (Relay/NAT Traversal)..." -ForegroundColor Cyan
$streamingArgs = @("--port", $RelayPort)
if (-not $NoMetrics) {
    $streamingArgs += @("--metrics", $MetricsPort)
}
$streamingServer = Start-Process -FilePath $components["Streaming Server"] -ArgumentList $streamingArgs -NoNewWindow -PassThru
$workflowComponents += @{
    Name = "Streaming Server"
    Process = $streamingServer
    URL = "rtsp://127.0.0.1:$RelayPort/live"
    Description = "Relay server with NAT traversal and metrics"
}
Write-Host "   ✅ Streaming Server started (PID: $($streamingServer.Id))" -ForegroundColor Green
Write-Host "   📡 Relay URL: rtsp://127.0.0.1:$RelayPort/live" -ForegroundColor White
if (-not $NoMetrics) {
    Write-Host "   📊 Metrics: http://127.0.0.1:$MetricsPort" -ForegroundColor White
}

# Ждем запускания streaming сервера
Start-Sleep -Seconds 3

# 3. 🖥 Viewer Client
Write-Host "🖥 Starting Viewer Client..." -ForegroundColor Cyan
$viewerClient = Start-Process -FilePath $components["Viewer Client"] -ArgumentList $workflowComponents[1].URL -NoNewWindow -PassThru
$workflowComponents += @{
    Name = "Viewer Client"
    Process = $viewerClient
    URL = $workflowComponents[1].URL
    Description = "RTSP viewer client for receiving stream"
}
Write-Host "   ✅ Viewer Client started (PID: $($viewerClient.Id))" -ForegroundColor Green
Write-Host "   📡 Connected to: $($workflowComponents[1].URL)" -ForegroundColor White

# Ждем запускания viewer клиента
Start-Sleep -Seconds 3

# 4. 📊 Metrics + Adaptive Streaming
if (-not $NoMetrics) {
    Write-Host "📊 Starting Metrics + Adaptive Streaming..." -ForegroundColor Cyan
    $metricsArgs = @(
        "--target-bitrate", $TargetBitrate,
        "--min-bitrate", $MinBitrate,
        "--max-bitrate", $MaxBitrate
    )
    $metricsSystem = Start-Process -FilePath $components["Metrics System"] -ArgumentList $metricsArgs -NoNewWindow -PassThru
    $workflowComponents += @{
        Name = "Metrics System"
        Process = $metricsSystem
        URL = ""
        Description = "Adaptive streaming with network monitoring"
    }
    Write-Host "   ✅ Metrics System started (PID: $($metricsSystem.Id))" -ForegroundColor Green
    Write-Host "   📊 Target Bitrate: $TargetBitrate kbps" -ForegroundColor White
    Write-Host "   📊 Min/Max Bitrate: $MinBitrate/$MaxBitrate kbps" -ForegroundColor White
    
    Start-Sleep -Seconds 3
}

Write-Host ""
Write-Host "🎉 Video Streaming Workflow Started Successfully!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""

# Вывод информации о запущенных компонентах
Write-Host "📋 Active Components:" -ForegroundColor Yellow
for ($i = 0; $i -lt $workflowComponents.Count; $i++) {
    $component = $workflowComponents[$i]
    Write-Host "   $($i + 1). $($component.Name)" -ForegroundColor White
    Write-Host "      📡 URL: $($component.URL)" -ForegroundColor Cyan
    Write-Host "      📝 PID: $($component.Process.Id)" -ForegroundColor Gray
    Write-Host "      📄 Description: $($component.Description)" -ForegroundColor Gray
    Write-Host ""
}

Write-Host "🔗 Workflow Connections:" -ForegroundColor Yellow
Write-Host "   📷 Camera -> 🌐 Relay: rtsp://127.0.0.1:8554/live -> rtsp://127.0.0.1:$RelayPort/live" -ForegroundColor White
Write-Host "   🌐 Relay -> 🖥 Viewer: rtsp://127.0.0.1:$RelayPort/live -> rtsp://127.0.0.1:$RelayPort/live" -ForegroundColor White
Write-Host ""

Write-Host "📊 Monitoring Options:" -ForegroundColor Yellow
Write-Host "   🌐 Streaming Server Metrics: http://127.0.0.1:$MetricsPort" -ForegroundColor White
Write-Host "   📊 Adaptive Streaming: Automatic bitrate adjustment" -ForegroundColor White
Write-Host ""

Write-Host "🎮 Control Commands:" -ForegroundColor Yellow
Write-Host "   Press Ctrl+C in this terminal to stop all components" -ForegroundColor White
Write-Host "   Each component has its own console window" -ForegroundColor White
Write-Host ""

Write-Host "🕒 Runtime Information:" -ForegroundColor Yellow
Write-Host "   All components are running in separate windows" -ForegroundColor White
Write-Host "   Check individual component windows for detailed logs" -ForegroundColor White
Write-Host "   Metrics will be updated automatically every 10 seconds" -ForegroundColor White
Write-Host ""

Write-Host "✨ Workflow is now running. Enjoy your video streaming system!" -ForegroundColor Green

# Ожидание пользовательского ввода для остановки
Write-Host "Press Enter to stop all components..." -ForegroundColor Cyan
Read-Host

Write-Host ""
Write-Host "🛑 Stopping all components..." -ForegroundColor Yellow

# Остановка всех компонентов в обратном порядке
for ($i = $workflowComponents.Count - 1; $i -ge 0; $i--) {
    $component = $workflowComponents[$i]
    try {
        if ($component.Process.HasExited -eq $false) {
            Write-Host "🛑 Stopping $($component.Name) (PID: $($component.Process.Id))..." -ForegroundColor Gray
            Stop-Process -Id $component.Process.Id -Force
            Write-Host "   ✅ $($component.Name) stopped" -ForegroundColor Green
        } else {
            Write-Host "   ℹ️ $($component.Name) already stopped" -ForegroundColor Gray
        }
    } catch {
        Write-Host "   ⚠️ Error stopping $($component.Name): $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "🎉 Video Streaming Workflow stopped!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
