# PowerShell скрипт для тестирования настоящего RTSP клиента
# Подключается к реальным RTSP источникам и получает настоящие RTP пакеты

param(
    [switch]$Clean = $false
)

Write-Host "=== Real RTSP Client Test Script ===" -ForegroundColor Green
Write-Host "Connecting to real RTSP video sources" -ForegroundColor Green
Write-Host ""

# Проверка наличия исполняемого файла
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rtspClient = Join-Path $scriptDir "..\build\real_rtsp_client.exe"
$rtspClient = [System.IO.Path]::GetFullPath($rtspClient)

if (-not (Test-Path $rtspClient)) {
    Write-Host "❌ Error: real_rtsp_client.exe not found at: $rtspClient" -ForegroundColor Red
    Write-Host "   Please build first: cmake --build build --target real_rtsp_client" -ForegroundColor Yellow
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

# Тест 1: Wowza Test Stream (официальный)
Write-Host "=== Test 1: Real Wowza Test Stream ===" -ForegroundColor Cyan
Write-Host "URL: rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2" -ForegroundColor White
Write-Host "Duration: 30 seconds" -ForegroundColor White
Write-Host "Output: real_wowza_stream.rtp" -ForegroundColor White
Write-Host ""

$process = Start-Process -FilePath $rtspClient -NoNewWindow -Wait -PassThru
if ($null -ne $process) {
    $process.WaitForExit()
} else {
    Write-Host "❌ Failed to start real RTSP client" -ForegroundColor Red
}

# Проверка результата
$outputFile = "real_wowza_stream.rtp"
if (Test-Path $outputFile) {
    $size = (Get-Item $outputFile).Length
    Write-Host "✅ Wowza test completed: $size bytes saved" -ForegroundColor Green
} else {
    Write-Host "❌ Wowza test failed: no output file" -ForegroundColor Red
}

Write-Host ""
Start-Sleep -Seconds 2

# Тест 2: IPVM Public Camera (реальная камера)
Write-Host "=== Test 2: Real IPVM Public Camera ===" -ForegroundColor Cyan
Write-Host "URL: rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast" -ForegroundColor White
Write-Host "Duration: 20 seconds" -ForegroundColor White
Write-Host "Output: real_ipvm_camera.rtp" -ForegroundColor White
Write-Host ""

$process = Start-Process -FilePath $rtspClient -NoNewWindow -Wait -PassThru
if ($null -ne $process) {
    $process.WaitForExit()
} else {
    Write-Host "❌ Failed to start real RTSP client" -ForegroundColor Red
}

# Проверка результата
$outputFile = "real_ipvm_camera.rtp"
if (Test-Path $outputFile) {
    $size = (Get-Item $outputFile).Length
    Write-Host "✅ IPVM test completed: $size bytes saved" -ForegroundColor Green
} else {
    Write-Host "❌ IPVM test failed: no output file" -ForegroundColor Red
}

Write-Host ""
Start-Sleep -Seconds 2

# Тест 3: Big Buck Bunny (простой файл)
Write-Host "=== Test 3: Real Big Buck Bunny Stream ===" -ForegroundColor Cyan
Write-Host "URL: rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov" -ForegroundColor White
Write-Host "Duration: 15 seconds" -ForegroundColor White
Write-Host "Output: real_bunny_stream.rtp" -ForegroundColor White
Write-Host ""

$process = Start-Process -FilePath $rtspClient -NoNewWindow -Wait -PassThru
if ($null -ne $process) {
    $process.WaitForExit()
} else {
    Write-Host "❌ Failed to start real RTSP client" -ForegroundColor Red
}

# Проверка результата
$outputFile = "real_bunny_stream.rtp"
if (Test-Path $outputFile) {
    $size = (Get-Item $outputFile).Length
    Write-Host "✅ Bunny test completed: $size bytes saved" -ForegroundColor Green
} else {
    Write-Host "❌ Bunny test failed: no output file" -ForegroundColor Red
}

Write-Host ""
Write-Host "🎉 All real RTSP tests completed!" -ForegroundColor Green
Write-Host ""

# Анализ результатов
Write-Host "=== Results Summary ===" -ForegroundColor Yellow
$totalFiles = 0
$successfulTests = 0

$files = @("real_wowza_stream.rtp", "real_ipvm_camera.rtp", "real_bunny_stream.rtp")
foreach ($file in $files) {
    if (Test-Path $file) {
        $totalFiles++
        $successfulTests++
        
        $size = (Get-Item $file).Length
        Write-Host "📊 $file`: $size bytes" -ForegroundColor White
    }
}

Write-Host ""
Write-Host "📈 Test Results:" -ForegroundColor Yellow
Write-Host "   Total files created: $totalFiles" -ForegroundColor White
Write-Host "   Successful tests: $successfulTests/3" -ForegroundColor White
$successRate = [math]::Round(($successfulTests * 100) / 3, 2)
Write-Host "   Success rate: $successRate%" -ForegroundColor White

if ($successfulTests -eq 3) {
    Write-Host "🎥 All real RTSP tests passed successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "🔍 Next steps:" -ForegroundColor Cyan
    Write-Host "   1. Analyze RTP files with: python ..\scripts\analyze_rtp_files.py *.rtp --compare" -ForegroundColor White
    Write-Host "   2. Convert to MP4 with: ffmpeg -i real_*.rtp -c copy output.mp4" -ForegroundColor White
    Write-Host "   3. Check packet quality and loss rates" -ForegroundColor White
    exit 0
} else {
    Write-Host "⚠️  Some real RTSP tests failed" -ForegroundColor Yellow
    Write-Host "   This could be due to network issues or unavailable sources" -ForegroundColor White
    exit 1
}
