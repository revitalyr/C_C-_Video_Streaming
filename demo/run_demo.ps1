# Visual Video Streaming Demo Script (PowerShell)

Write-Host "🎬 === VISUAL VIDEO STREAMING DEMO ===" -ForegroundColor Cyan
Write-Host ""

# Check if visual_demo exists
if (-not (Test-Path "..\build\visual_demo.exe")) {
    Write-Host "🔧 Building visual demo..." -ForegroundColor Yellow
    Set-Location ..
    cmake --build build --target visual_demo
    Set-Location demo
}

Write-Host "📡 Select network conditions:" -ForegroundColor Green
Write-Host "1) Perfect network (0% loss, 0ms delay)"
Write-Host "2) Good network (1% loss, 20ms delay)"
Write-Host "3) Fair network (5% loss, 50ms delay, 10ms jitter)"
Write-Host "4) Poor network (10% loss, 100ms delay, 30ms jitter)"
Write-Host "5) Terrible network (20% loss, 200ms delay, 50ms jitter)"
Write-Host "6) Custom conditions"
Write-Host ""

$choice = Read-Host "Choose option (1-6)"

switch ($choice) {
    "1" {
        Write-Host "🌐 Running with perfect network..." -ForegroundColor Blue
        ..\build\visual_demo.exe
    }
    "2" {
        Write-Host "🌐 Running with good network..." -ForegroundColor Blue
        ..\build\visual_demo.exe --loss 1 --delay 20
    }
    "3" {
        Write-Host "🌐 Running with fair network..." -ForegroundColor Blue
        ..\build\visual_demo.exe --loss 5 --delay 50 --jitter 10
    }
    "4" {
        Write-Host "🌐 Running with poor network..." -ForegroundColor Blue
        ..\build\visual_demo.exe --loss 10 --delay 100 --jitter 30
    }
    "5" {
        Write-Host "🌐 Running with terrible network..." -ForegroundColor Blue
        ..\build\visual_demo.exe --loss 20 --delay 200 --jitter 50
    }
    "6" {
        $loss = Read-Host "Packet loss % (0-100)"
        $delay = Read-Host "Network delay ms (0-1000)"
        $jitter = Read-Host "Network jitter ms (0-200)"
        Write-Host "🌐 Running with custom conditions..." -ForegroundColor Blue
        ..\build\visual_demo.exe --loss $loss --delay $delay --jitter $jitter
    }
    default {
        Write-Host "❌ Invalid option" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "🎬 Demo completed!" -ForegroundColor Green
