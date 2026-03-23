#!/bin/bash
set -e

echo "🔧 Fixing video_sender.hpp..."

# Replace VideoFrame with Frame
find core/session/sender -name "video_sender.hpp" -exec sed -i 's/VideoFrame/Frame/g' {} +

# Add include for frame.hpp to the header
find core/session/sender -name "video_sender.hpp" -exec sed -i '1i #include "media/frame.hpp"' {} +

echo "✅ Fix applied."