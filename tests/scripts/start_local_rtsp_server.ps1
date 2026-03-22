# PowerShell script to start local RTSP server
# Uses custom RTSP server implementation instead of FFmpeg

param(
    [string]$VideoFile = "",  # Empty = synthetic video
    [int]$Port = 8554,
    [string]$Address = "127.0.0.1",
    [int]$FPS = 25,
    [int]$Bitrate = 2000000,
    [switch]$Kill = $false
)

Write-Host "=== Local RTSP Server (Native Implementation) ===" -ForegroundColor Green
Write-Host "Starting native RTSP server for local video streaming" -ForegroundColor Green
Write-Host ""

# Проверка наличия исполняемого файла
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$serverExe = Join-Path $scriptDir "..\..\build\local_rtsp_server.exe"
$serverExe = [System.IO.Path]::GetFullPath($serverExe)

if (-not (Test-Path $serverExe)) {
    Write-Host "❌ Error: local_rtsp_server.exe not found at: $serverExe" -ForegroundColor Red
    Write-Host "   Please build first: cmake --build build --target local_rtsp_server" -ForegroundColor Yellow
    exit 1
}

# Остановка предыдущих процессов
if ($Kill) {
    Write-Host "🔄 Stopping existing RTSP server processes..." -ForegroundColor Yellow
    
    Get-Process -Name local_rtsp_server -ErrorAction SilentlyContinue | ForEach-Object {
        Stop-Process -Id $_.Id -Force
        Write-Host "  Stopped process $($_.Id)" -ForegroundColor Gray
    }
    
    Start-Sleep -Seconds 2
    Write-Host "✅ Previous processes stopped" -ForegroundColor Green
    exit 0
}

# Check video file (if specified)
$videoArg = ""
if (-not [string]::IsNullOrEmpty($VideoFile)) {
    $fullVideoPath = Join-Path $scriptDir "..\..\" $VideoFile
    if (-not (Test-Path $fullVideoPath)) {
        Write-Host "❌ Error: Video file not found: $fullVideoPath" -ForegroundColor Red
        exit 1
    }
    $videoArg = "--video `"$fullVideoPath`""
    Write-Host "📹 Video file: $fullVideoPath" -ForegroundColor White
} else {
    Write-Host "🎥 Using synthetic video source" -ForegroundColor Cyan
}

Write-Host "🌐 Bind Address: $Address" -ForegroundColor White
Write-Host "📡 RTSP Port: $Port" -ForegroundColor White
Write-Host "🎬 FPS: $FPS" -ForegroundColor White
Write-Host "⚡ Bitrate: $Bitrate bps" -ForegroundColor White
Write-Host ""

# Create log directory
$logDir = "logs"
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir -Force | Out-Null
}

# Format command line arguments
$serverArgs = @()
if (-not [string]::IsNullOrEmpty($videoArg)) {
    $serverArgs += $videoArg
}
$serverArgs += "--port", $Port
$serverArgs += "--address", $Address
$serverArgs += "--fps", $FPS
$serverArgs += "--bitrate", $Bitrate

Write-Host "🚀 Starting native RTSP server..." -ForegroundColor Cyan
Write-Host "Command: $serverExe $($serverArgs -join ' ')" -ForegroundColor Gray
Write-Host ""

# Запуск сервера
$serverProcess = Start-Process -FilePath $serverExe -ArgumentList $serverArgs -NoNewWindow -PassThru

if ($null -eq $serverProcess) {
    Write-Host "❌ Failed to start RTSP server" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Native RTSP server started!" -ForegroundColor Green
Write-Host "📡 RTSP URL: rtsp://$($Address):$Port/live" -ForegroundColor Cyan
Write-Host "🎥 Video source: $([string]::IsNullOrEmpty($VideoFile) ? 'Synthetic' : $VideoFile)" -ForegroundColor White
Write-Host ""
Write-Host "📝 Server Information:" -ForegroundColor Yellow
Write-Host "   Process ID: $($serverProcess.Id)" -ForegroundColor White
Write-Host "   Executable: $serverExe" -ForegroundColor White
Write-Host ""
Write-Host "🎮 Test URLs:" -ForegroundColor Cyan
Write-Host "   RTSP: rtsp://$($Address):$Port/live" -ForegroundColor White
Write-Host "   For testing with: .\tests\scripts\test_real_rtsp.ps1 -Source local" -ForegroundColor White
Write-Host ""
Write-Host "🔧 Additional Options:" -ForegroundColor Gray
Write-Host "   VLC: Open Network Stream → rtsp://$($Address):$Port/live" -ForegroundColor Gray
Write-Host "   FFplay: ffplay rtsp://$($Address):$Port/live" -ForegroundColor Gray
Write-Host "   Custom video: .\tests\scripts\start_local_rtsp_server.ps1 -VideoFile `"tests\data\oobe-intro.mp4`"" -ForegroundColor Gray
Write-Host ""
Write-Host "⏹️  Server is running. Press Ctrl+C to stop." -ForegroundColor Green
Write-Host "📋 To stop server: .\tests\scripts\start_local_rtsp_server.ps1 -Kill" -ForegroundColor Gray
Write-Host ""

# Wait for process completion
try {
    $serverProcess | Out-Null
    $serverProcess.WaitForExit()
} catch [System.Management.Automation.StopActionException] {
    Write-Host ""
    Write-Host "🛑 Server stopped by user" -ForegroundColor Yellow
} catch {
    Write-Host ""
    Write-Host "❌ Server process exited with code: $($serverProcess.ExitCode)" -ForegroundColor Red
}

Write-Host ""
Write-Host "🏁 Native RTSP server stopped" -ForegroundColor Red

# Cleanup child processes
Start-Sleep -Seconds 1
Get-Process -Name local_rtsp_server -ErrorAction SilentlyContinue | ForEach-Object {
    Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
}
