#!/bin/bash

# Enable strict mode
set -euo pipefail

# Default arguments
CLEAN=false
SOURCE="all"

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    -c|--clean)
      CLEAN=true
      shift
      ;;
    -s|--source)
      SOURCE="$2"
      shift 2
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# Fix: Trim hidden whitespace/CR characters (common in WSL)
SOURCE=$(echo "$SOURCE" | tr -d '[:space:]')

# Setup paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")/build"
CLIENT_EXEC="$BUILD_DIR/test_rtsp_client"
RESULTS_DIR="test_results"

echo "=== Real RTSP Client Test Script (Linux) ==="
echo "Connecting to real RTSP video sources"
echo ""

# Check executable
if [[ ! -x "$CLIENT_EXEC" ]]; then
    # Fallback for Windows executable in WSL
    if [[ -f "${CLIENT_EXEC}.exe" ]]; then
        CLIENT_EXEC="${CLIENT_EXEC}.exe"
    else
        echo "❌ Error: test_rtsp_client executable not found at: $CLIENT_EXEC"
        echo "   Please build first: cmake --build build --target test_rtsp_client"
        exit 1
    fi
fi

# Clean results if requested
if [[ "$CLEAN" == "true" ]]; then
    if [[ -d "$RESULTS_DIR" ]]; then
        rm -rf "$RESULTS_DIR"
        echo "🧹 Cleaned previous results"
    fi
fi

mkdir -p "$RESULTS_DIR"
cd "$RESULTS_DIR" || exit

echo "🎥 Starting real RTSP stream tests..."
echo ""

# Test function
run_test() {
    local name="$1"
    local url="$2"
    local output_file="$3"
    local duration="$4"

    echo "=== Test: $name ==="
    echo "URL: $url"
    echo "Duration: $duration seconds"
    echo "Output: $output_file"

    # Run client
    # Note: Passing arguments explicitly (assuming CLI standard), which the PS1 script may have omitted
    "$CLIENT_EXEC" --url "$url" --output "$output_file" --duration "$duration"
    
    if [[ -f "$output_file" && -s "$output_file" ]]; then
        local size=$(stat -c%s "$output_file")
        echo "✅ $name test completed: $size bytes saved"
        echo ""
        return 0
    else
        echo "❌ $name test failed: no output file or empty"
        echo ""
        return 1
    fi
}

SUCCESS_COUNT=0
TOTAL_COUNT=0
TESTS_RAN=false

if [[ "$SOURCE" == "all" || "$SOURCE" == "wowza" ]]; then
    TESTS_RAN=true
    ((TOTAL_COUNT+=1))
    if run_test "Wowza Test Stream" "rtsp://716f898c7b71.entrypoint.cloud.wowza.com:1935/app-8F9K44lJ/304679fe_stream2" "wowza_stream.rtp" 30; then
        ((SUCCESS_COUNT+=1))
    fi
    sleep 2
fi

if [[ "$SOURCE" == "all" || "$SOURCE" == "ipvm" ]]; then
    TESTS_RAN=true
    ((TOTAL_COUNT+=1))
    if run_test "IPVM Public Camera" "rtsp://demo:demo@ipvmdemo.dyndns.org:5541/onvif-media/media.amp?profile=profile_1_h264&sessiontimeout=60&streamtype=unicast" "ipvm_camera.rtp" 20; then
        ((SUCCESS_COUNT+=1))
    fi
    sleep 2
fi

if [[ "$SOURCE" == "all" || "$SOURCE" == "bunny" ]]; then
    TESTS_RAN=true
    ((TOTAL_COUNT+=1))
    if run_test "Big Buck Bunny Stream" "rtsp://wowzaec2demo.streamlock.net/vod/mp4:BigBuckBunny_115k.mov" "bunny_stream.rtp" 15; then
        ((SUCCESS_COUNT+=1))
    fi
fi

echo "🎉 Real RTSP tests completed!"
echo ""
echo "=== Results Summary ==="

if [[ "$TESTS_RAN" == "false" ]]; then
    echo "⚠️  No tests were executed. Check your SOURCE parameter ('$SOURCE')."
    exit 0
fi

echo "📈 Test Results:"
echo "   Successful tests: $SUCCESS_COUNT/$TOTAL_COUNT"

if [[ $SUCCESS_COUNT -gt 0 ]]; then
    echo ""
    echo "🔍 Next steps:"
    echo "   1. Analyze RTP files: python ../scripts/analyze_rtp_files.py *.rtp --compare"
    echo "   2. Convert to MP4: ffmpeg -i *.rtp -c copy output.mp4"
    exit 0
else
    echo "⚠️  No RTSP tests succeeded"
    exit 1
fi