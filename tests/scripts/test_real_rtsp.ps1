# PowerShell скрипт для тестирования настоящего RTSP клиента
# Подключается к реальным RTSP источникам и получает настоящие RTP пакеты

param(
    [switch]$Clean = $false,
    [string]$Source = "all"  # all, wowza, ipvm, bunny
)

Write-Host "=== Real RTSP Client Test Script ===" -ForegroundColor Green
Write-Host "Connecting to real RTSP video sources" -ForegroundColor Green
Write-Host ""

# Проверка наличия исполняемого файла
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rtspClient = Join-Path $scriptDir "..\..\build\test_rtsp_client.exe"
$rtspClient = [System.IO.Path]::GetFullPath($rtspClient)

if (-not (Test-Path $rtspClient)) {
    Write-Host "❌ Error: test_rtsp_client.exe not found at: $rtspClient" -ForegroundColor Red
    Write-Host "   Please build first: cmake --build build --target test_rtsp_client" -ForegroundColor Yellow
    exit 1
}

# Создание директории для результатов
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

# Функция для тестирования одного источника
function Test-RTSPSource($name, $url, $outputFile, $duration) {
    Write-Host "=== Test: $name ===" -ForegroundColor Cyan
    Write-Host "URL: $url" -ForegroundColor White
    Write-Host "Duration: $duration seconds" -ForegroundColor White
    Write-Host "Output: $outputFile" -ForegroundColor White
    Write-Host ""
    
    $process = Start-Process -FilePath $rtspClient -ArgumentList "--url", $url, "--output", $outputFile, "--duration", $duration -NoNewWindow -Wait -PassThru
    if ($null -ne $process) {
        $process.WaitForExit()
    } else {
        Write-Host "❌ Failed to start real RTSP client" -ForegroundColor Red
        return $false
    }
    
    # Проверка результата
    if (Test-Path $outputFile) {
        $size = (Get-Item $outputFile).Length
        Write-Host "✅ $name test completed: $size bytes saved" -ForegroundColor Green
        return $true
    } else {
        Write-Host "❌ $name test failed: no output file" -ForegroundColor Red
        return $false
    }
}

# Тестирование источников в зависимости от параметра
$successfulTests = 0
$totalTests = 0

if ($Source -eq "all" -or $Source -eq "wowza") {
    $totalTests++
    if (Test-RTSPSource "Wowza Test Stream" "rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2" "wowza_stream.rtp" 30) {
        $successfulTests++
    }
    Write-Host ""
    Start-Sleep -Seconds 2
}

if ($Source -eq "all" -or $Source -eq "ipvm") {
    $totalTests++
    if (Test-RTSPSource "IPVM Public Camera" "rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast" "ipvm_camera.rtp" 20) {
        $successfulTests++
    }
    Write-Host ""
    Start-Sleep -Seconds 2
}

if ($Source -eq "all" -or $Source -eq "bunny") {
    $totalTests++
    if (Test-RTSPSource "Big Buck Bunny Stream" "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov" "bunny_stream.rtp" 15) {
        $successfulTests++
    }
    Write-Host ""
}

Write-Host "🎉 Real RTSP tests completed!" -ForegroundColor Green
Write-Host ""

# Анализ результатов
Write-Host "=== Results Summary ===" -ForegroundColor Yellow

Write-Host ""
Write-Host "📈 Test Results:" -ForegroundColor Yellow
Write-Host "   Successful tests: $successfulTests/$totalTests" -ForegroundColor White

if ($successfulTests -gt 0) {
    Write-Host ""
    Write-Host "🔍 Next steps:" -ForegroundColor Cyan
    Write-Host "   1. Analyze RTP files: python ..\scripts\analyze_rtp_files.py *.rtp --compare" -ForegroundColor White
    Write-Host "   2. Convert to MP4: ffmpeg -i *.rtp -c copy output.mp4" -ForegroundColor White
    exit 0
} else {
    Write-Host "⚠️  No RTSP tests succeeded" -ForegroundColor Yellow
    exit 1
}