#!/bin/bash
set -e
echo "🔧 Fixing include paths..."

# Fix includes in headers and modules to match new CMake include paths
find core -type f \( -name "*.hpp" -o -name "*.ixx" \) -exec sed -i 's|include "../rtp/|include "rtp/|g' {} +
find core -type f \( -name "*.hpp" -o -name "*.ixx" \) -exec sed -i 's|include "../media/|include "media/|g' {} +
find core -type f \( -name "*.hpp" -o -name "*.ixx" \) -exec sed -i 's|include "../network/|include "network/|g' {} +
find core -type f \( -name "*.hpp" -o -name "*.ixx" \) -exec sed -i 's|include "../jitter/|include "jitter/|g' {} +
find core -type f \( -name "*.hpp" -o -name "*.ixx" \) -exec sed -i 's|include "../common/|include "common/|g' {} +

echo "✅ Includes updated."