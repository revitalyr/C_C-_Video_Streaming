#!/usr/bin/env bash
# tests/scripts/quick_local_test.sh — Bash version of quick_local_test.ps1
# Runs local RTSP server and tests the client

set -u

# Define paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
RESULTS_DIR="$PROJECT_ROOT/tests/test_results"

# Binary files
SERVER_BIN="$BUILD_DIR/local_rtsp_server"
CLIENT_BIN="$BUILD_DIR/test_rtsp_client"

# Default parameters
KILL_ONLY=false
CLEAN=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --kill)
            KILL_ONLY=true
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --help)
            echo "Usage: $0 [--kill] [--clean]"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Process cleanup function
cleanup_processes() {
    echo "🔄 Stopping processes..."
    if pgrep -f "local_rtsp_server" > /dev/null; then
        pkill -f "local_rtsp_server" && echo "   Stopped local_rtsp_server"
    fi
    if pgrep -f "test_rtsp_client" > /dev/null; then
        pkill -f "test_rtsp_client" && echo "   Stopped test_rtsp_client"
    fi
}

if [ "$KILL_ONLY" = true ]; then
    cleanup_processes
    exit 0
fi

echo "=== Quick Local RTSP Test (Bash/WSL) ==="

# Clean results
if [ "$CLEAN" = true ]; then
    echo "🧹 Cleaning results..."
    rm -rf "$RESULTS_DIR"
fi

# Check for binaries
if [ ! -f "$SERVER_BIN" ]; then
    echo "❌ Error: $SERVER_BIN not found."
    echo "   Please build first: cmake --build build"
    exit 1
fi

if [ ! -f "$CLIENT_BIN" ]; then
    echo "❌ Error: $CLIENT_BIN not found."
    echo "   Please build first: cmake --build build --target test_rtsp_client"
    exit 1
fi

mkdir -p "$RESULTS_DIR"

# 1. Start server
echo "1️⃣ Starting local RTSP server..."
"$SERVER_BIN" > "$RESULTS_DIR/server.log" 2>&1 &
SERVER_PID=$!
echo "   Server PID: $SERVER_PID"

# Wait for startup (check port 8554)
echo "   Waiting for server..."
sleep 2
if (echo > /dev/tcp/127.0.0.1/8554) >/dev/null 2>&1; then
    echo "   ✅ RTSP server is running on port 8554"
else
    echo "   ⚠️ Warning: Could not verify port 8554, but process is running. Continuing..."
fi

# 2. Start client
echo "2️⃣ Testing RTSP client..."
cd "$RESULTS_DIR" || exit 1

# Start client (URL, OutputFile, Duration)
if "$CLIENT_BIN" "rtsp://127.0.0.1:8554/live" "local_stream.h264" 10; then
    echo "   ✅ Client finished successfully"
else
    echo "   ❌ Client failed"
    cleanup_processes
    exit 1
fi

# 3. Analyze
echo "3️⃣ Analyzing results..."
if [ -f "local_stream.h264" ] && [ -s "local_stream.h264" ]; then
    SIZE=$(stat -c%s "local_stream.h264")
    echo "   📊 H.264 file size: $SIZE bytes"
    echo "🎉 Quick local test completed successfully!"
else
    echo "   ❌ Output file is empty or missing"
fi

cleanup_processes