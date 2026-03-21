# PowerShell скрипт для тестирования RTSP/RTP видеостриминга с реальными источниками
# Использует реальные публичные RTSP URL для тестирования

param(
    [switch]$Clean = $false
)

Write-Host "=== RTSP/RTP Video Streaming Test Script ===" -ForegroundColor Green
Write-Host "Testing with real video sources" -ForegroundColor Green
Write-Host ""

# Проверка наличия исполняемого файла
$rtspClient = "build\rtsp_test_client.exe"
if (-not (Test-Path $rtspClient)) {
    Write-Host "❌ Error: rtsp_test_client.exe not found. Please build first:" -ForegroundColor Red
    Write-Host "   cmake --build build --target rtsp_test_client" -ForegroundColor Yellow
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

Write-Host "🎥 Starting RTSP stream tests..." -ForegroundColor Green
Write-Host ""

# Тест 1: Wowza Test Stream (официальный)
Write-Host "=== Test 1: Wowza Test Stream ===" -ForegroundColor Cyan
Write-Host "URL: rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2" -ForegroundColor White
Write-Host "Duration: 30 seconds" -ForegroundColor White
Write-Host "Output: wowza_stream.rtp" -ForegroundColor White
Write-Host ""

$process = Start-Process -FilePath $rtspClient -NoNewWindow -Wait
$process.WaitForExit()

# Проверка результата
$outputFile = "wowza_stream.rtp"
if (Test-Path $outputFile) {
    $size = (Get-Item $outputFile).Length
    Write-Host "✅ Wowza test completed: $size bytes saved" -ForegroundColor Green
} else {
    Write-Host "❌ Wowza test failed: no output file" -ForegroundColor Red
}

Write-Host ""
Start-Sleep -Seconds 2

# Тест 2: IPVM Public Camera (реальная камера)
Write-Host "=== Test 2: IPVM Public Camera ===" -ForegroundColor Cyan
Write-Host "URL: rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast" -ForegroundColor White
Write-Host "Duration: 20 seconds" -ForegroundColor White
Write-Host "Output: ipvm_camera.rtp" -ForegroundColor White
Write-Host ""

$process = Start-Process -FilePath $rtspClient -NoNewWindow -Wait
$process.WaitForExit()

# Проверка результата
$outputFile = "ipvm_camera.rtp"
if (Test-Path $outputFile) {
    $size = (Get-Item $outputFile).Length
    Write-Host "✅ IPVM test completed: $size bytes saved" -ForegroundColor Green
} else {
    Write-Host "❌ IPVM test failed: no output file" -ForegroundColor Red
}

Write-Host ""
Start-Sleep -Seconds 2

# Тест 3: Big Buck Bunny (простой файл)
Write-Host "=== Test 3: Big Buck Bunny Stream ===" -ForegroundColor Cyan
Write-Host "URL: rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov" -ForegroundColor White
Write-Host "Duration: 15 seconds" -ForegroundColor White
Write-Host "Output: bunny_stream.rtp" -ForegroundColor White
Write-Host ""

$process = Start-Process -FilePath $rtspClient -NoNewWindow -Wait
$process.WaitForExit()

# Проверка результата
$outputFile = "bunny_stream.rtp"
if (Test-Path $outputFile) {
    $size = (Get-Item $outputFile).Length
    Write-Host "✅ Bunny test completed: $size bytes saved" -ForegroundColor Green
} else {
    Write-Host "❌ Bunny test failed: no output file" -ForegroundColor Red
}

Write-Host ""
Write-Host "🎉 All RTSP stream tests completed!" -ForegroundColor Green
Write-Host ""

# Анализ результатов
Write-Host "=== Results Summary ===" -ForegroundColor Yellow
$totalFiles = 0
$successfulTests = 0

$files = @("wowza_stream.rtp", "ipvm_camera.rtp", "bunny_stream.rtp")
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
    Write-Host "🎥 All tests passed successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "⚠️  Some tests failed" -ForegroundColor Yellow
    exit 1
}
