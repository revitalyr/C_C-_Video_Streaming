#!/bin/bash
set -e

# Locate RtpPacket definition
HEADER_FILE=$(find core -name "rtp_packet.hpp" | head -n 1)
SOURCE_FILE="core/session/sender/video_sender.cpp"

echo "🔍 Inspecting RtpPacket interface..."

if [ -f "$HEADER_FILE" ]; then
    # Determine the correct method to access packet data
    if grep -q "get_buffer()" "$HEADER_FILE"; then
        METHOD="get_buffer()"
    elif grep -q "data()" "$HEADER_FILE"; then
        METHOD="data()"
    elif grep -q "buffer()" "$HEADER_FILE"; then
        METHOD="buffer()"
    else
        echo "⚠️ Could not auto-detect method. Assuming get_buffer()."
        METHOD="get_buffer()"
    fi
    
    echo "✅ Detected accessor: packet.$METHOD"
    
    # Apply fix to source file (replace potential incorrect calls)
    sed -i "s/packet.get_data()/packet.${METHOD}/g" "$SOURCE_FILE"
    sed -i "s/packet.data()/packet.${METHOD}/g" "$SOURCE_FILE"
    sed -i "s/packet.get_buffer()/packet.${METHOD}/g" "$SOURCE_FILE"
fi