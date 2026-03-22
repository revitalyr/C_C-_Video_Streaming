# PowerShell script for testing real RTSP client
# Connects to real RTSP sources and receives real RTP packets

param(
    [switch]$Clean = $false,
    [string]$Source = "local"  # local only
)

Write-Host "=== Real RTSP Client Test Script ===" -ForegroundColor Green
Write-Host "Connecting to real RTSP video sources" -ForegroundColor Green
Write-Host ""

# Check for executable
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rtspClient = Join-Path $scriptDir "..\..\build\test_rtsp_client.exe"
$rtspClient = [System.IO.Path]::GetFullPath($rtspClient)

if (-not (Test-Path $rtspClient)) {
    Write-Host "❌ Error: test_rtsp_client.exe not found at: $rtspClient" -ForegroundColor Red
    Write-Host "   Please build first: cmake --build build --target test_rtsp_client" -ForegroundColor Yellow
    exit 1
}

# Create results directory
$resultsDir = "test_results"
if ($Clean) {
    if (Test-Path $resultsDir) {
        Remove-Item -Path $resultsDir -Recurse -Force
        Write-Host "🧹 Cleaned previous results" -ForegroundColor Yellow
    }
}

New-Item -ItemType Directory -Path $resultsDir -Force | Out-Null
Set-Location $resultsDir

Write-Host "🎥 Starting real RTSP stream tests..." -ForegroundColor Green
Write-Host ""

# Function to test a single source
function Test-RTSPSource($name, $url, $outputFile, $duration) {
    Write-Host "=== Test: $name ===" -ForegroundColor Cyan
    Write-Host "URL: $url" -ForegroundColor White
    Write-Host "Duration: $duration seconds" -ForegroundColor White
    Write-Host "Output: $outputFile" -ForegroundColor White
    Write-Host ""
    
    $process = Start-Process -FilePath $rtspClient -NoNewWindow -Wait -PassThru
    if ($null -ne $process) {
        $process.WaitForExit()
    } else {
        Write-Host "❌ Failed to start real RTSP client" -ForegroundColor Red
        return $false
    }
    
    # Check result
    if (Test-Path $outputFile) {
        $size = (Get-Item $outputFile).Length
        Write-Host "✅ $name test completed: $size bytes saved" -ForegroundColor Green
        return $true
    } else {
        Write-Host "❌ $name test failed: no output file" -ForegroundColor Red
        return $false
    }
}

# Test sources based on parameter
$successfulTests = 0
$totalTests = 0

# Local test only
if ($Source -eq "all" -or $Source -eq "local") {
    $totalTests++
    if (Test-RTSPSource "Local RTSP Server" "rtsp://localhost:8554/live" "local_stream.rtp" 10) {
        $successfulTests++
    }
    Write-Host ""
    Start-Sleep -Seconds 2
} else {
    Write-Host "❌ Only 'local' source is supported. Use -Source local" -ForegroundColor Red
    Write-Host "   Available sources: local" -ForegroundColor White
    exit 1
}

Write-Host "🎉 Real RTSP tests completed!" -ForegroundColor Green
Write-Host ""

# Analyze results
Write-Host "=== Results Summary ===" -ForegroundColor Yellow
$totalFiles = 0

$files = @()
if ($Source -eq "local") { $files += "local_stream.rtp" }

foreach ($file in $files) {
    if (Test-Path $file) {
        $totalFiles++
        $size = (Get-Item $file).Length
        Write-Host "📊 $file`: $size bytes" -ForegroundColor White
    }
}

Write-Host ""
Write-Host "📈 Test Results:" -ForegroundColor Yellow
Write-Host "   Total files created: $totalFiles" -ForegroundColor White
Write-Host "   Successful tests: $successfulTests/$totalTests" -ForegroundColor White
if ($totalTests -gt 0) {
    $successRate = [math]::Round(($successfulTests * 100) / $totalTests, 2)
    Write-Host "   Success rate: $successRate%" -ForegroundColor White
}

if ($successfulTests -gt 0) {
    Write-Host ""
    Write-Host "🔍 Next steps:" -ForegroundColor Cyan
    Write-Host "   1. Analyze RTP files: python analyze_rtp_files.py *.rtp --compare" -ForegroundColor White
    Write-Host "   2. Convert to MP4: ffmpeg -i *.rtp -c copy output.mp4" -ForegroundColor White
    Write-Host "   3. Check packet quality and loss rates" -ForegroundColor White
    
    if ($totalFiles -gt 0) {
        Write-Host ""
        Write-Host "🎥 Real RTSP streaming test successful!" -ForegroundColor Green
        exit 0
    }
} else {
    Write-Host "⚠️  No RTSP tests succeeded" -ForegroundColor Yellow
    Write-Host "   This could be due to network issues or unavailable sources" -ForegroundColor White
    Write-Host "   Try individual sources with: -Source wowza, -Source ipvm, -Source bunny, or -Source local" -ForegroundColor White
    exit 1
}