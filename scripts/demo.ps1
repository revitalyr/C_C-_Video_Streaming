# One-Command Video Streaming Demo for Windows
# This script demonstrates real-time video streaming with metrics

param(
    [double]$Loss = 0,
    [int]$Delay = 0,
    [int]$Jitter = 0,
    [string]$Mode = "basic",
    [switch]$Help = $false
)

if ($Help) {
    Write-Host "Video Streaming Demo - One Command Testing"
    Write-Host ""
    Write-Host "Usage: .\demo.ps1 [options]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Loss <percent>     Packet loss rate (0-100)"
    Write-Host "  -Delay <ms>         Network delay (0-1000ms)"
    Write-Host "  -Jitter <ms>        Network jitter (0-200ms)"
    Write-Host "  -Mode <type>        Demo type: basic, ffplay, visual"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\demo.ps1                                    # Perfect network"
    Write-Host "  .\demo.ps1 -Loss 5                            # 5% packet loss"
    Write-Host "  .\demo.ps1 -Loss 10 -Delay 100 -Jitter 30    # Poor network"
    Write-Host "  .\demo.ps1 -Mode ffplay -Loss 5              # FFplay with 5% loss"
    Write-Host "  .\demo.ps1 -Mode visual                       # Visual ASCII demo"
    Write-Host ""
    Write-Host "Demo Types:"
    Write-Host "  basic    - UDP streaming with console metrics"
    Write-Host "  ffplay   - Pipe to ffplay for real video"
    Write-Host "  visual   - ASCII art visualization"
    exit 0
}

Write-Host "🎬 === VIDEO STREAMING DEMO ==="
Write-Host ""

# Ensure we are running from the project root
$ScriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Definition
Set-Location "$ScriptRoot\.."

# Check if build directory exists
if (!(Test-Path "build")) {
    Write-Host "📦 Build directory not found. Building project..."
    New-Item -ItemType Directory -Force -Path "build"
    Set-Location build
    cmake .. -DCMAKE_TOOLCHAIN_FILE="./vcpkg/scripts/buildsystems/vcpkg.cmake"
    cmake --build .
    Set-Location ..
    Write-Host "✅ Build completed!"
    Write-Host ""
}

# Check if binaries exist
if (!(Test-Path "build\sender.exe") -or !(Test-Path "build\viewer.exe")) {
    Write-Host "❌ Binaries not found. Please build the project first:"
    Write-Host "   mkdir build"
    Write-Host "   cd build"
    Write-Host "   cmake .. -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake"
    Write-Host "   cmake --build ."
    exit 1
}

Write-Host "🔧 Demo Configuration:"
Write-Host "  📉 Packet Loss: $Loss%"
Write-Host "  ⏱️  Delay: $Delay ms"
Write-Host "  🔄 Jitter: $Jitter ms"
Write-Host "  🎬 Mode: $Mode"
Write-Host ""

# Function to cleanup background processes
function Cleanup {
    Write-Host ""
    Write-Host "🛑 Stopping demo..."
    Get-Process | Where-Object { $_.ProcessName -like "*sender*" -or $_.ProcessName -like "*viewer*" } | Stop-Process -Force
    Write-Host "✅ Demo stopped."
    exit 0
}

# Set up signal handlers
$null = Register-ObjectEvent -InputObject ([System.Console]::CancelKeyPress) -Action { Cleanup }

switch ($Mode.ToLower()) {
    "basic" {
        Write-Host "🎬 Starting Basic UDP Streaming Demo..."
        Write-Host "📡 Sender: .\build\network_sender.exe -Loss $Loss -Delay $Delay -Jitter $Jitter"
        Write-Host "📺 Viewer: .\build\viewer.exe"
        Write-Host ""
        Write-Host "🔥 Press Ctrl+C to stop both sender and viewer"
        Write-Host ""
        
        # Start viewer in background
        $viewer = Start-Process -FilePath ".\build\viewer.exe" -PassThru
        
        # Give viewer time to start
        Start-Sleep -Seconds 1
        
        # Start sender in foreground
        & ".\build\network_sender.exe" -Loss $Loss -Delay $Delay -Jitter $Jitter
        
        # Wait for viewer to finish
        $viewer.WaitForExit()
    }
    
    "ffplay" {
        Write-Host "🎬 Starting FFplay Demo..."
        Write-Host "📡 Sender: .\build\network_sender.exe -Loss $Loss -Delay $Delay -Jitter $Jitter"
        Write-Host "📺 Viewer: .\build\ffplay_viewer.exe | ffplay -f h264 -"
        Write-Host ""
        Write-Host "🔥 Press Ctrl+C to stop streaming"
        Write-Host ""
        
        # Check if ffplay is available
        try {
            $null = Get-Command ffplay -ErrorAction Stop
        } catch {
            Write-Host "❌ ffplay not found. Please install FFmpeg:"
            Write-Host "   Download from https://ffmpeg.org/download.html"
            Write-Host "   Add ffmpeg\bin to your PATH"
            exit 1
        }
        
        # Start ffplay viewer and pipe to ffplay
        $ffplay = Start-Process -FilePath "ffplay" -ArgumentList "-f", "h264", "-", "-window_title", "Video Stream Demo" -PassThru
        
        # Give ffplay time to start
        Start-Sleep -Seconds 2
        
        # Start sender
        & ".\build\network_sender.exe" -Loss $Loss -Delay $Delay -Jitter $Jitter
        
        $ffplay.WaitForExit()
    }
    
    "visual" {
        Write-Host "🎬 Starting Visual ASCII Demo..."
        Write-Host "📡 Running: .\build\visual_demo.exe -Loss $Loss -Delay $Delay -Jitter $Jitter"
        Write-Host ""
        Write-Host "🔥 Press Ctrl+C to stop demo"
        Write-Host ""
        
        # Check if visual_demo exists
        if (!(Test-Path "build\visual_demo.exe")) {
            Write-Host "❌ Visual demo not found. Building..."
            Set-Location build
            cmake --build . --target visual_demo
            Set-Location ..
        }
        
        # Run visual demo
        & ".\build\visual_demo.exe" -Loss $Loss -Delay $Delay -Jitter $Jitter
    }
    
    default {
        Write-Host "❌ Unknown mode: $Mode"
        Write-Host "Available modes: basic, ffplay, visual"
        exit 1
    }
}
