#!/bin/bash

# Visual Video Streaming Demo Script
echo "🎬 === VISUAL VIDEO STREAMING DEMO ==="
echo ""

# Check if visual_demo exists
if [ ! -f "../build/visual_demo" ]; then
    echo "🔧 Building visual demo..."
    cd ..
    cmake --build build --target visual_demo
    cd demo
fi

echo "📡 Select network conditions:"
echo "1) Perfect network (0% loss, 0ms delay)"
echo "2) Good network (1% loss, 20ms delay)"
echo "3) Fair network (5% loss, 50ms delay, 10ms jitter)"
echo "4) Poor network (10% loss, 100ms delay, 30ms jitter)"
echo "5) Terrible network (20% loss, 200ms delay, 50ms jitter)"
echo "6) Custom conditions"
echo ""

read -p "Choose option (1-6): " choice

case $choice in
    1)
        echo "🌐 Running with perfect network..."
        ../build/visual_demo
        ;;
    2)
        echo "🌐 Running with good network..."
        ../build/visual_demo --loss 1 --delay 20
        ;;
    3)
        echo "🌐 Running with fair network..."
        ../build/visual_demo --loss 5 --delay 50 --jitter 10
        ;;
    4)
        echo "🌐 Running with poor network..."
        ../build/visual_demo --loss 10 --delay 100 --jitter 30
        ;;
    5)
        echo "🌐 Running with terrible network..."
        ../build/visual_demo --loss 20 --delay 200 --jitter 50
        ;;
    6)
        read -p "Packet loss % (0-100): " loss
        read -p "Network delay ms (0-1000): " delay
        read -p "Network jitter ms (0-200): " jitter
        echo "🌐 Running with custom conditions..."
        ../build/visual_demo --loss $loss --delay $delay --jitter $jitter
        ;;
    *)
        echo "❌ Invalid option"
        exit 1
        ;;
esac

echo ""
echo "🎬 Demo completed!"
