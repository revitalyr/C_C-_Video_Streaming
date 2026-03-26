#!/bin/bash

# Video Stream Player with Network Simulation
echo "VIDEO STREAM PLAYER - NETWORK SIMULATION"
echo "========================================"
echo ""

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Check if visual_demo exists
if [ ! -f "$BUILD_DIR/visual_demo" ]; then
    echo "BUILD: Compiling video stream player..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake "$PROJECT_ROOT"
    cmake --build . --target visual_demo
    cd "$SCRIPT_DIR"
fi

echo "NETWORK CONDITIONS:"
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
        echo "EXEC: Perfect network conditions"
        "$BUILD_DIR/visual_demo"
        ;;
    2)
        echo "EXEC: Good network (1% loss, 20ms delay)"
        "$BUILD_DIR/visual_demo" --loss 1 --delay 20
        ;;
    3)
        echo "EXEC: Fair network (5% loss, 50ms delay, 10ms jitter)"
        "$BUILD_DIR/visual_demo" --loss 5 --delay 50 --jitter 10
        ;;
    4)
        echo "EXEC: Poor network (10% loss, 100ms delay, 30ms jitter)"
        "$BUILD_DIR/visual_demo" --loss 10 --delay 100 --jitter 30
        ;;
    5)
        echo "EXEC: Terrible network (20% loss, 200ms delay, 50ms jitter)"
        "$BUILD_DIR/visual_demo" --loss 20 --delay 200 --jitter 50
        ;;
    6)
        read -p "Packet loss % (0-100): " loss
        read -p "Network delay ms (0-1000): " delay
        read -p "Network jitter ms (0-200): " jitter
        echo "EXEC: Custom network conditions"
        echo "  Loss: $loss%, Delay: ${delay}ms, Jitter: ${jitter}ms"
        "$BUILD_DIR/visual_demo" --loss $loss --delay $delay --jitter $jitter
        ;;
    *)
        echo "ERROR: Invalid option selected"
        exit 1
        ;;
esac

echo ""
echo "SESSION COMPLETED"
