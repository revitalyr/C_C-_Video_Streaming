#!/bin/bash
set -e

echo "📦 Refactoring project structure..."

# Create new directories
mkdir -p apps
mkdir -p core
mkdir -p scripts

# Move Core components
echo " -> Moving core components..."
[ -d "rtp" ] && mv rtp core/
[ -d "jitter" ] && mv jitter core/
[ -d "network" ] && mv network core/
[ -d "media" ] && mv media core/
[ -d "common" ] && mv common core/
[ -d "pipeline" ] && mv pipeline core/

# Move High-Level Core Components
[ -d "sender" ] && mv sender core/
[ -d "receiver" ] && mv receiver core/
[ -d "viewer" ] && mv viewer core/

# Move Apps
echo " -> Moving applications..."
if [ -d "src" ]; then
    mv src/* apps/ 2>/dev/null || true
    rmdir src
fi

# Move visual_demo to apps for consistency
if [ -f "demo/visual_demo.cpp" ]; then
    mv demo/visual_demo.cpp apps/
fi

# Move Scripts
echo " -> Moving scripts..."
[ -f "demo.sh" ] && mv demo.sh scripts/
[ -f "demo.ps1" ] && mv demo.ps1 scripts/
[ -f "refactor_structure.sh" ] && cp refactor_structure.sh scripts/ && rm refactor_structure.sh

# Consolidate Core Components
echo " -> Consolidating core components..."
cd core

mkdir -p transport video session

# Transport (Network & Protocols)
[ -d "network" ] && mv network transport/
[ -d "rtp" ] && mv rtp transport/

# Video (Media & Jitter)
[ -d "media" ] && mv media video/
[ -d "jitter" ] && mv jitter video/

# Session (High-level Logic)
[ -d "sender" ] && mv sender session/
[ -d "receiver" ] && mv receiver session/
[ -d "viewer" ] && mv viewer session/

cd ..

echo "✅ Done! Run 'cmake --build build' to recompile."