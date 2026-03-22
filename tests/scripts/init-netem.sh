#!/usr/bin/env bash
# init-netem.sh — Host network preparation for correct test operation
# Run on host (where Docker is installed) with root/sudo privileges.

BRIDGE="lossy0"

# Check if interface exists (created by Docker Compose)
if ! ip link show "$BRIDGE" > /dev/null 2>&1; then
    echo "Warning: Interface $BRIDGE not found."
    echo "This is expected on Docker Desktop (Windows/Mac). Host-side optimizations will be skipped."
    echo "Tests will continue using container-level emulation."
    exit 0
fi

echo "Configuring $BRIDGE for accurate network emulation..."

# Disable offloading (TSO, GSO, GRO).
# If not done, network card/driver might coalesce packets into large segments,
# and tc netem will delay them in "batches" rather than individually, distorting tests.
ethtool -K "$BRIDGE" tso off gso off gro off > /dev/null 2>&1 || echo "Warning: Could not disable offloading (ethtool missing?)"

# Clear possible old rules on the bridge itself (test rules are applied inside container)
tc qdisc del dev "$BRIDGE" root 2>/dev/null || true

echo "Network $BRIDGE is ready."
