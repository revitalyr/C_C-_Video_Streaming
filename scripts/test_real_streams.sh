#!/bin/bash

# Скрипт для тестирования RTSP/RTP видеостриминга с реальными источниками
# Использует реальные публичные RTSP URL для тестирования

set -e  # Выход при ошибке

echo "=== RTSP/RTP Video Streaming Test Script ==="
echo "Testing with real video sources"
echo ""

# Проверка наличия исполняемых файлов
if [ ! -f "build/rtsp_test_client.exe" ]; then
    echo "❌ Error: rtsp_test_client.exe not found. Please build first:"
    echo "   cmake --build build --target rtsp_test_client"
    exit 1
fi

# Создание директории для результатов
mkdir -p test_results
cd test_results

echo "🎥 Starting RTSP stream tests..."
echo ""

# Тест 1: Wowza Test Stream (официальный)
echo "=== Test 1: Wowza Test Stream ==="
echo "URL: rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2"
echo "Duration: 30 seconds"
echo "Output: wowza_stream.rtp"
echo ""

../build/rtsp_test_client.exe

# Проверка результата
if [ -f "wowza_stream.rtp" ]; then
    size=$(stat -f%s "wowza_stream.rtp" | cut -d' ' ' -f1)
    echo "✅ Wowza test completed: $size bytes saved"
else
    echo "❌ Wowza test failed: no output file"
fi

echo ""
sleep 2

# Тест 2: IPVM Public Camera (реальная камера)
echo "=== Test 2: IPVM Public Camera ==="
echo "URL: rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast"
echo "Duration: 20 seconds"
echo "Output: ipvm_camera.rtp"
echo ""

../build/rtsp_test_client.exe

# Проверка результата
if [ -f "ipvm_camera.rtp" ]; then
    size=$(stat -f%s "ipvm_camera.rtp" | cut -d' ' ' -f1)
    echo "✅ IPVM test completed: $size bytes saved"
else
    echo "❌ IPVM test failed: no output file"
fi

echo ""
sleep 2

# Тест 3: Big Buck Bunny (простой файл)
echo "=== Test 3: Big Buck Bunny Stream ==="
echo "URL: rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov"
echo "Duration: 15 seconds"
echo "Output: bunny_stream.rtp"
echo ""

../build/rtsp_test_client.exe

# Проверка результата
if [ -f "bunny_stream.rtp" ]; then
    size=$(stat -f%s "bunny_stream.rtp" | cut -d' ' ' -f1)
    echo "✅ Bunny test completed: $size bytes saved"
else
    echo "❌ Bunny test failed: no output file"
fi

echo ""
echo "🎉 All RTSP stream tests completed!"
echo ""

# Анализ результатов
echo "=== Results Summary ==="
total_files=0
successful_tests=0

for file in wowza_stream.rtp ipvm_camera.rtp bunny_stream.rtp; do
    if [ -f "$file" ]; then
        total_files=$((total_files + 1))
        successful_tests=$((successful_tests + 1))
        
        size=$(stat -f%s "$file" | cut -d' ' ' -f1)
        echo "📊 $file: $size bytes"
    fi
done

echo ""
echo "📈 Test Results:"
echo "   Total files created: $total_files"
echo "   Successful tests: $successful_tests/3"
echo "   Success rate: $((successful_tests * 100 / 3))%"

if [ $successful_tests -eq 3 ]; then
    echo "🎥 All tests passed successfully!"
    exit 0
else
    echo "⚠️  Some tests failed"
    exit 1
fi
