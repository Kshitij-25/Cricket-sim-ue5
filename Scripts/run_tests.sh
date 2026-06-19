#!/usr/bin/env bash
# CricketSim — run headless automation tests and report pass/fail.
#
# Usage:
#   Scripts/run_tests.sh [Suite]
#     Suite : a CricketSim automation filter (default: CricketSim = everything)
#             e.g. CricketSim.Physics+CricketSim.Bat   CricketSim.Match
#
# Results are parsed from UE's log (NOT stdout). Exit code is non-zero on any
# test failure, so this is CI-friendly.
set -euo pipefail

UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$PROJECT_DIR/CricketSim.uproject"
CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
LOG="$HOME/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log"

SUITE="${1:-CricketSim}"

if [[ ! -x "$CMD" ]]; then
  echo "ERROR: UnrealEditor-Cmd not found at: $CMD" >&2
  exit 1
fi

rm -f "$LOG" 2>/dev/null || true
echo "==> Running automation tests: $SUITE"
"$CMD" "$PROJECT" -ExecCmds="Automation RunTests $SUITE; Quit" \
  -unattended -nullrhi -nosplash -nopause >/dev/null 2>&1 || true

if [[ ! -f "$LOG" ]]; then
  echo "ERROR: test log not produced at: $LOG" >&2
  exit 1
fi

PASS=$(grep -c "Result={Success}" "$LOG" || true)
FAIL=$(grep -c "Result={Fail}" "$LOG" || true)
echo "==> Passed: $PASS   Failed: $FAIL"
if [[ "$FAIL" -gt 0 ]]; then
  echo "--- Failures ---"
  grep "Result={Fail}" "$LOG" || true
  exit 1
fi
echo "==> All tests passed."
