#!/bin/bash
set -e
# Ensure we run from project root
cd "$(dirname "$0")/.."

echo "📂 Organizing apps into categories..."

mkdir -p apps/basic
mkdir -p apps/rtsp
mkdir -p apps/vis
mkdir -p apps/exp

# Basic UDP Streaming
[ -f apps/sender.cpp ] && mv apps/sender.cpp apps/basic/
[ -f apps/viewer.cpp ] && mv apps/viewer.cpp apps/basic/
[ -f apps/network_sender.cpp ] && mv apps/network_sender.cpp apps/basic/

# RTSP
[ -f apps/simple_rtsp_server.cpp ] && mv apps/simple_rtsp_server.cpp apps/rtsp/
[ -f apps/viewer_client.cpp ] && mv apps/viewer_client.cpp apps/rtsp/
[ -f apps/test_rtsp_client.cpp ] && mv apps/test_rtsp_client.cpp apps/rtsp/

# Visualization
[ -f apps/sdl_viewer.cpp ] && mv apps/sdl_viewer.cpp apps/vis/
[ -f apps/ffplay_viewer.cpp ] && mv apps/ffplay_viewer.cpp apps/vis/
[ -f apps/visual_demo.cpp ] && mv apps/visual_demo.cpp apps/vis/

# Experimental
[ -f apps/streaming_server.cpp ] && mv apps/streaming_server.cpp apps/exp/
[ -f apps/metrics_adaptive.cpp ] && mv apps/metrics_adaptive.cpp apps/exp/

echo "✅ Apps organized! Don't forget to re-run CMake."