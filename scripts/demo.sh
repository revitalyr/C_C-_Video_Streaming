#!/bin/bash

# One-Command Video Streaming Demo
# This script demonstrates real-time video streaming with metrics

set -e

echo "🎬 === VIDEO STREAMING DEMO ==="
echo ""

# Ensure we are running from the project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.." || exit 1

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "📦 Build directory not found. Building project..."
    mkdir -p build
    cd build
    cmake .. -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
    cmake --build .
    cd ..
    echo "✅ Build completed!"
    echo ""
fi

# Check if binaries exist
if [ ! -f "build/sender" ] || [ ! -f "build/viewer" ]; then
    echo "❌ Binaries not found. Please build the project first:"
    echo "   mkdir build && cd build"
    echo "   cmake .. -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake"
    echo "   cmake --build ."
    exit 1
fi

# Parse command line arguments
LOSS=0
DELAY=0
JITTER=0
MODE="basic"

while [[ $# -gt 0 ]]; do
    case $1 in
        --loss)
            LOSS="$2"
            shift 2
            ;;
        --delay)
            DELAY="$2"
            shift 2
            ;;
        --jitter)
            JITTER="$2"
            shift 2
            ;;
        --mode)
            MODE="$2"
            shift 2
            ;;
        --help)
            echo "Video Streaming Demo - One Command Testing"
            echo ""
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --loss <percent>     Packet loss rate (0-100)"
            echo "  --delay <ms>         Network delay (0-1000ms)"
            echo "  --jitter <ms>        Network jitter (0-200ms)"
            echo "  --mode <type>        Demo type: basic, ffplay, visual"
            echo ""
            echo "Examples:"
            echo "  $0                                    # Perfect network"
            echo "  $0 --loss 5                          # 5% packet loss"
            echo "  $0 --loss 10 --delay 100 --jitter 30 # Poor network"
            echo "  $0 --mode ffplay --loss 5            # FFplay with 5% loss"
            echo "  $0 --mode visual                     # Visual ASCII demo"
            echo ""
            echo "Demo Types:"
            echo "  basic    - UDP streaming with console metrics"
            echo "  ffplay   - Pipe to ffplay for real video"
            echo "  visual   - ASCII art visualization"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "🔧 Demo Configuration:"
echo "  📉 Packet Loss: $LOSS%"
echo "  ⏱️  Delay: $DELAY ms"
echo "  🔄 Jitter: $JITTER ms"
echo "  🎬 Mode: $MODE"
echo ""

# Function to cleanup background processes
cleanup() {
    echo ""
    echo "🛑 Stopping demo..."
    
    # Kill all child processes
    pkill -P $$ 2>/dev/null || true
    
    # Kill specific demo processes
    pkill -f "network_sender" 2>/dev/null || true
    pkill -f "viewer" 2>/dev/null || true
    pkill -f "ffplay" 2>/dev/null || true
    pkill -f "visual_demo" 2>/dev/null || true
    
    # Wait a moment for processes to die
    sleep 1
    
    echo "✅ Demo stopped."
    exit 0
}

# Set up signal handlers
trap cleanup SIGINT SIGTERM

case $MODE in
    "basic")
        echo "🎬 Starting Basic UDP Streaming Demo..."
        echo "📡 Sender: ./build/network_sender --loss $LOSS --delay $DELAY --jitter $JITTER"
        echo "📺 Viewer: ./build/viewer"
        echo ""
        echo "🔥 Press Ctrl+C to stop both sender and viewer"
        echo ""
        
        # Start viewer in background
        ./build/viewer &
        VIEWER_PID=$!
        
        # Give viewer time to start
        sleep 1
        
        # Start sender in foreground
        ./build/network_sender --loss $LOSS --delay $DELAY --jitter $JITTER
        
        # This will block until sender is stopped
        wait $VIEWER_PID
        ;;
        
    "ffplay")
        echo "🎬 Starting FFplay Demo..."
        echo "📡 Sender: ./build/network_sender --loss $LOSS --delay $DELAY --jitter $JITTER"
        echo "📺 Viewer: ./build/ffplay_viewer | ffplay -f h264 -"
        echo ""
        echo "🔥 Press Ctrl+C to stop streaming"
        echo ""
        
        # Check if ffplay is available
        if ! command -v ffplay &> /dev/null; then
            echo "❌ ffplay not found. Please install FFmpeg:"
            echo "   Ubuntu/Debian: sudo apt-get install ffmpeg"
            echo "   macOS: brew install ffmpeg"
            echo "   Windows: Download from https://ffmpeg.org/"
            exit 1
        fi
        
        # Start ffplay viewer and pipe to ffplay
        ./build/ffplay_viewer | ffplay -f h264 -window_title "Video Stream Demo" - &
        
        # Give ffplay time to start
        sleep 2
        
        # Start sender
        ./build/network_sender --loss $LOSS --delay $DELAY --jitter $JITTER
        
        wait
        ;;
        
    "visual")
        echo "🎬 Starting Visual ASCII Demo..."
        echo "📡 Running: ./build/visual_demo --loss $LOSS --delay $DELAY --jitter $JITTER"
        echo ""
        echo "🔥 Press Ctrl+C to stop demo"
        echo ""
        
        # Check if visual_demo exists
        if [ ! -f "build/visual_demo" ]; then
            echo "❌ Visual demo not found. Building..."
            cd build
            cmake --build . --target visual_demo
            cd ..
        fi
        
        # Run visual demo
        ./build/visual_demo --loss $LOSS --delay $DELAY --jitter $JITTER
        ;;
        
    *)
        echo "❌ Unknown mode: $MODE"
        echo "Available modes: basic, ffplay, visual"
        exit 1
        ;;
esac
