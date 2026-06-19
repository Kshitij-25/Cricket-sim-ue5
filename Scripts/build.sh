#!/usr/bin/env bash
# CricketSim — compile a target. Does NOT cook/package (see package_mac.sh).
#
# Usage:
#   Scripts/build.sh [Target] [Config]
#     Target : CricketSimEditor (default) | CricketSim
#     Config : Development (default) | DebugGame | Shipping | Test
#
# Examples:
#   Scripts/build.sh                         # editor, Development (day-to-day)
#   Scripts/build.sh CricketSim Shipping     # game executable, Shipping (release codepath)
#
# Override the engine location with UE_ROOT if it is not the default below.
set -euo pipefail

UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$PROJECT_DIR/CricketSim.uproject"

TARGET="${1:-CricketSimEditor}"
CONFIG="${2:-Development}"
BUILD_SH="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"

if [[ ! -x "$BUILD_SH" ]]; then
  echo "ERROR: UE Build.sh not found at: $BUILD_SH" >&2
  echo "       Set UE_ROOT to your Unreal Engine 5.7 install." >&2
  exit 1
fi

echo "==> Building $TARGET | Mac | $CONFIG"
"$BUILD_SH" "$TARGET" Mac "$CONFIG" -project="$PROJECT" "${@:3}"
echo "==> Build OK: $TARGET ($CONFIG)"
