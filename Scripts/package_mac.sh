#!/usr/bin/env bash
# CricketSim — cook + package a standalone macOS build via RunUAT BuildCookRun.
#
# Usage:
#   Scripts/package_mac.sh [Config] [OutputDir]
#     Config    : Shipping (default) | Development | Test
#     OutputDir : staging dir (default: <project>/Build/Mac)
#
# PREREQUISITE (release blocker): the project must contain a cookable startup map.
# DefaultEngine.ini's GameDefaultMap (/Game/Maps/L_Nets) and at least one level
# with the gameplay actors placed must exist under Content/ before this succeeds.
# See Docs/KNOWN_ISSUES.md.
set -euo pipefail

UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$PROJECT_DIR/CricketSim.uproject"
RUNUAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"

CONFIG="${1:-Shipping}"
ARCHIVE_DIR="${2:-$PROJECT_DIR/Build/Mac}"

if [[ ! -x "$RUNUAT" ]]; then
  echo "ERROR: RunUAT.sh not found at: $RUNUAT" >&2
  echo "       Set UE_ROOT to your Unreal Engine 5.7 install." >&2
  exit 1
fi

echo "==> Packaging CricketSim | Mac | $CONFIG -> $ARCHIVE_DIR"
"$RUNUAT" BuildCookRun \
  -project="$PROJECT" \
  -platform=Mac \
  -clientconfig="$CONFIG" \
  -targetplatform=Mac \
  -target=CricketSim \
  -cook -stage -pak -package -build -archive \
  -archivedirectory="$ARCHIVE_DIR" \
  -nocompileeditor -utf8output -nop4

echo "==> Package complete. Artifact under: $ARCHIVE_DIR"
