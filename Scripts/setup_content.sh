#!/usr/bin/env bash
# CricketSim — run the Python content-setup script inside the Unreal Editor.
#
# Creates L_Nets, L_Match, and the recommended data assets automatically.
# Must be run AFTER `Scripts/build.sh CricketSimEditor Development` succeeds.
#
# Usage:
#   Scripts/setup_content.sh
#   UE_ROOT=/custom/path Scripts/setup_content.sh
#
# The editor opens headlessly, executes the Python, saves all assets, and quits.
# Results are logged to ~/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log
set -euo pipefail

UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$PROJECT_DIR/CricketSim.uproject"
EDITOR="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor"
SCRIPT="$PROJECT_DIR/Scripts/setup_content.py"
LOG="$HOME/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log"

if [[ ! -x "$EDITOR" ]]; then
  echo "ERROR: UnrealEditor not found at: $EDITOR" >&2
  echo "       Set UE_ROOT to your Unreal Engine 5.7 install." >&2
  exit 1
fi

if [[ ! -f "$SCRIPT" ]]; then
  echo "ERROR: setup script not found: $SCRIPT" >&2
  exit 1
fi

echo "==> Running content setup via UnrealEditor Python scripting"
echo "    Project : $PROJECT"
echo "    Script  : $SCRIPT"
echo "    Log     : $LOG"

# Remove stale log so we can grep from a clean baseline.
rm -f "$LOG" 2>/dev/null || true

# -ExecutePythonScript runs the script after the editor has fully loaded all modules,
# then -unattended/-nosplash prevent any modal dialogs, and -nullrhi skips GPU init.
"$EDITOR" "$PROJECT" \
  -ExecutePythonScript="$SCRIPT" \
  -unattended -nosplash -nopause -nullrhi \
  -log 2>&1 | grep -E '\[setup_content\]|ERROR|Warning' || true

echo ""
echo "==> Checking log for success..."

if [[ ! -f "$LOG" ]]; then
  echo "WARNING: log not found at expected path; check ~/Library/Logs/Unreal Engine/ manually."
else
  # Report what the script logged
  grep "\[setup_content\]" "$LOG" 2>/dev/null | tail -30 || true

  if grep -q "content setup complete" "$LOG" 2>/dev/null; then
    echo ""
    echo "==> Content setup complete."
    echo "    Verify with: find Content -name '*.umap' && find Content -name '*.uasset'"
  else
    echo ""
    echo "WARNING: completion marker not found in log. Check $LOG for details."
    grep -i "error\|fail\|fatal" "$LOG" 2>/dev/null | tail -20 || true
    exit 1
  fi
fi

echo ""
echo "==> Next: Scripts/package_mac.sh Shipping"
