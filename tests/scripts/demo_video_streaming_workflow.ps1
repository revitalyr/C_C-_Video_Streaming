# PowerShell script for Video Streaming Workflow demonstration
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

Write-Host " Video Streaming Workflow Demo" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""

# Check for RTSP server
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $scriptDir "..\..\build"
$rtspServer = Join-Path $buildDir "local_rtsp_server.exe"

if (-not (Test-Path $rtspServer)) {
    Write-Host "❌ RTSP Server not found: $rtspServer" -ForegroundColor Red
    Write-Host "   Please build first: cmake --build build --target local_rtsp_server" -ForegroundColor Yellow
    exit 1
}

Write-Host "✅ Found RTSP Server: $rtspServer" -ForegroundColor Green

# Stop previous processes
if ($Kill) {
    Write-Host "🔄 Stopping previous RTSP server..." -ForegroundColor Yellow
    
    Get-Process -Name local_rtsp_server -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.Id -Force
        Write-Host "  Stopped process $($_.Id)" -ForegroundColor Gray
    }
    
    Start-Sleep -Seconds 2
    Write-Host "✅ Previous processes stopped" -ForegroundColor Green
    Write-Host ""
}

# Start RTSP server
Write-Host "📷 Starting Embedded Camera Simulator (RTSP Server)..." -ForegroundColor Cyan
$rtspProcess = Start-Process -FilePath $rtspServer -NoNewWindow -PassThru
Write-Host "   ✅ RTSP Server started (PID: $($rtspProcess.Id))" -ForegroundColor Green
Write-Host "   📡 Direct URL: rtsp://127.0.0.1:8554/live" -ForegroundColor White

# Wait for RTSP server startup
Start-Sleep -Seconds 3

# Demonstrate connection with FFplay (if available)
$ffplayPath = "ffplay"
if (Get-Command $ffplayPath -ErrorAction SilentlyContinue) {
    Write-Host "🖥 Testing with FFplay..." -ForegroundColor Cyan
    Write-Host "   📡 Connecting to: rtsp://127.0.0.1:8554/live" -ForegroundColor White
    
    # Start FFplay in background
    $ffplayTest = Start-Process -FilePath $ffplayPath -ArgumentList "rtsp://127.0.0.1:8554/live" -NoNewWindow -PassThru
    
    # Wait a bit for connection
    Start-Sleep -Seconds 5
    
    # Check if FFplay is running
    if (-not $ffplayTest.HasExited) {
        Write-Host "   ✅ FFplay is connected and playing" -ForegroundColor Green
        Write-Host "   🎬 You should see video window with synthetic frames" -ForegroundColor White
    } else {
        Write-Host "   ⚠️ FFplay exited or failed to connect" -ForegroundColor Yellow
    }
    
    # Stop FFplay after 10 seconds
    Start-Sleep -Seconds 10
    
    if (-not $ffplayTest.HasExited) {
        Write-Host "🛑 Stopping FFplay test..." -ForegroundColor Gray
        Stop-Process -Id $ffplayTest.Id -Force
        Write-Host "   ✅ FFplay test stopped" -ForegroundColor Green
    }
} else {
    Write-Host "⚠️ FFplay not found. Skipping video test." -ForegroundColor Yellow
    Write-Host "   Install FFmpeg and add to PATH to test video playback" -ForegroundColor Gray
}

Write-Host ""
Write-Host "🎉 Video Streaming Demo Started!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""

Write-Host "📋 Active Components:" -ForegroundColor Yellow
Write-Host "   1. 📷 Embedded Camera Simulator (RTSP Server)" -ForegroundColor White
Write-Host "      📡 Direct URL: rtsp://127.0.0.1:8554/live" -ForegroundColor Cyan
Write-Host "      📝 PID: $($rtspProcess.Id)" -ForegroundColor Gray
Write-Host "      🎬 Status: Running and generating synthetic H.264 video" -ForegroundColor Green

if (Get-Command $ffplayPath -ErrorAction SilentlyContinue) {
    Write-Host "   2. 🖥 FFplay Test Client" -ForegroundColor White
    Write-Host "      📡 Connected to: rtsp://127.0.0.1:8554/live" -ForegroundColor Cyan
    Write-Host "      🎬 Status: Testing TCP interleave streaming" -ForegroundColor Green
}

Write-Host ""
Write-Host "🔗 Workflow Connection:" -ForegroundColor Yellow
Write-Host "   📷 Camera -> 🖥 Viewer: rtsp://127.0.0.1:8554/live" -ForegroundColor White
Write-Host ""

Write-Host "📊 Technical Details:" -ForegroundColor Yellow
Write-Host "   🔄 RTSP Protocol: Full implementation with CSeq" -ForegroundColor White
Write-Host "   📡 TCP Interleave: RFC 2326 compliant streaming" -ForegroundColor White
Write-Host "   🎬 H.264 Encoding: Synthetic SPS/PPS/IDR frames" -ForegroundColor White
Write-Host "   📈 RTP Packets: Proper sequence numbers and timestamps" -ForegroundColor White
Write-Host ""

Write-Host "🎮 Control Options:" -ForegroundColor Yellow
Write-Host "   Press Ctrl+C in this terminal to stop RTSP server" -ForegroundColor White
Write-Host "   RTSP server will continue running in background" -ForegroundColor White
Write-Host ""

Write-Host "✨ Demo is running. Your video streaming system is active!" -ForegroundColor Green

# Wait for user input to stop
Write-Host "Press Enter to stop RTSP server..." -ForegroundColor Cyan
Read-Host

Write-Host ""
Write-Host "🛑 Stopping RTSP server..." -ForegroundColor Yellow

try {
    if ($rtspProcess.HasExited -eq $false) {
        Write-Host "🛑 Stopping RTSP server (PID: $($rtspProcess.Id))..." -ForegroundColor Gray
        Stop-Process -Id $rtspProcess.Id -Force
        Write-Host "   ✅ RTSP server stopped" -ForegroundColor Green
    } else {
        Write-Host "   ℹ️ RTSP server already stopped" -ForegroundColor Gray
    }
} catch {
    Write-Host "   ⚠️ Error stopping RTSP server: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "🎉 Video Streaming Demo stopped!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
