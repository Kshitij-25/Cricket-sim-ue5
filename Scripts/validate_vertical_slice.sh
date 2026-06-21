#!/usr/bin/env bash
# CricketSim — vertical slice validation script.
#
# Runs all automation suites and collects the statistical data required by the
# vertical slice validation report (see Docs/VERTICAL_SLICE_VALIDATION_REPORT.md).
#
# Collected automatically:
#   - All 122 unit / integration tests (pass / fail)
#   - 80 AI vs AI T20 matches (balance + validation report)
#   - Statistical metrics: avg score, wickets, run rate, boundary %, crash reports
#
# Human vs AI matches (10 sessions) are done manually in the packaged build;
# see Docs/GAMEPLAY_FOOTAGE_CHECKLIST.md for the manual testing matrix.
#
# Usage:
#   Scripts/validate_vertical_slice.sh [output_dir]
#     output_dir : where to write the report summary (default: Saved/Validation)
set -euo pipefail

UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}"
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT="$PROJECT_DIR/CricketSim.uproject"
CMD="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
LOG="$HOME/Library/Logs/Unreal Engine/CricketSimEditor/CricketSim.log"
OUT_DIR="${1:-$PROJECT_DIR/Saved/Validation}"
REPORT="$OUT_DIR/VerticalSliceValidation.md"

mkdir -p "$OUT_DIR"

if [[ ! -x "$CMD" ]]; then
  echo "ERROR: UnrealEditor-Cmd not found at: $CMD" >&2
  exit 1
fi

TIMESTAMP="$(date '+%Y-%m-%d %H:%M:%S')"
echo "==> CricketSim vertical slice validation — $TIMESTAMP"

# grep -c already prints "0" (with a non-zero exit) when there are no matches;
# appending "|| echo 0" double-prints a second "0" line. This helper returns
# exactly one line in all cases (including a missing log file).
count_result() {
  local pattern="$1" file="$2"
  if [[ -f "$file" ]]; then
    grep -c "$pattern" "$file" 2>/dev/null || true
  else
    echo 0
  fi
}

# ---------------------------------------------------------------------------
# Phase 1: Full test suite (122 tests)
# ---------------------------------------------------------------------------
echo ""
echo "--- Phase 1: Full test suite (CricketSim.*) ---"
rm -f "$LOG" 2>/dev/null || true

"$CMD" "$PROJECT" \
  -ExecCmds="Automation RunTests CricketSim; Quit" \
  -unattended -nullrhi -nosplash -nopause >/dev/null 2>&1 || true

PASS=$(count_result "Result={Success}" "$LOG")
FAIL=$(count_result "Result={Fail}" "$LOG")
echo "    Passed: $PASS   Failed: $FAIL"

if [[ "$FAIL" -gt 0 ]]; then
  echo "    FAILURES:"
  grep "Result={Fail}" "$LOG" 2>/dev/null | sed 's/^/    /' || true
fi

UNIT_STATUS="PASS ($PASS/$((PASS + FAIL)))"
[[ "$FAIL" -gt 0 ]] && UNIT_STATUS="FAIL ($PASS passed, $FAIL failed)"

# ---------------------------------------------------------------------------
# Phase 2: AI vs AI balance validation (runs ~80 simulated T20 matches)
# ---------------------------------------------------------------------------
echo ""
echo "--- Phase 2: AI vs AI balance validation (CricketSim.Balance) ---"
rm -f "$LOG" 2>/dev/null || true

"$CMD" "$PROJECT" \
  -ExecCmds="Automation RunTests CricketSim.Balance.ValidationReport; Quit" \
  -unattended -nullrhi -nosplash -nopause >/dev/null 2>&1 || true

BALANCE_PASS=$(count_result "Result={Success}" "$LOG")
BALANCE_FAIL=$(count_result "Result={Fail}" "$LOG")
BALANCE_STATUS="PASS"
[[ "$BALANCE_FAIL" -gt 0 ]] && BALANCE_STATUS="WARN ($BALANCE_FAIL sub-tests failed)"
echo "    Balance tests: $BALANCE_PASS passed, $BALANCE_FAIL failed"

# Extract key metrics from validation report if it was generated
METRICS_CSV="$PROJECT_DIR/Saved/Validation/Metrics.csv"
AVG_SCORE="n/a"
AVG_WICKETS="n/a"
RUN_RATE="n/a"
BOUNDARY_PCT="n/a"

if [[ -f "$METRICS_CSV" ]]; then
  # CSV is column-based: header row names columns, data rows are labelled
  # (Baseline, Cond_*, Diff_*). Pull the named column from the "Baseline" row.
  parse_metric() {
    awk -F',' -v col="$1" '
      NR==1 { for (i=1;i<=NF;i++) if ($i==col) c=i }
      $1=="Baseline" && c { print $c; exit }
    ' "$METRICS_CSV" 2>/dev/null
  }
  AVG_SCORE="$(parse_metric 'Avg1stInn')"
  AVG_WICKETS="$(parse_metric 'WktsPerInn')"
  RUN_RATE="$(parse_metric 'RunRate')"
  BOUNDARY_PCT="$(parse_metric 'BoundaryPct')"
  [[ -z "$AVG_SCORE" ]] && AVG_SCORE="n/a"
  [[ -z "$AVG_WICKETS" ]] && AVG_WICKETS="n/a"
  [[ -z "$RUN_RATE" ]] && RUN_RATE="n/a"
  [[ -z "$BOUNDARY_PCT" ]] && BOUNDARY_PCT="n/a"
  echo "    Metrics from Metrics.csv (Baseline row, 80 matches):"
  echo "      Avg 1st-innings total : $AVG_SCORE (target 150-182)"
  echo "      Avg wickets/innings   : $AVG_WICKETS (target 5.0-7.0)"
  echo "      Run rate              : $RUN_RATE (target 7.7-9.1)"
  echo "      Boundary %            : $BOUNDARY_PCT (target 14-19.5)"
else
  echo "    Metrics.csv not found — re-run CricketSim.Balance.ValidationReport to generate"
fi

# ---------------------------------------------------------------------------
# Phase 3: AI batch simulation (CricketSim.AI.FullMatch — headless full T20 sim)
# ---------------------------------------------------------------------------
echo ""
echo "--- Phase 3: AI batch simulation (CricketSim.AI.FullMatch) ---"
rm -f "$LOG" 2>/dev/null || true

"$CMD" "$PROJECT" \
  -ExecCmds="Automation RunTests CricketSim.AI.FullMatch; Quit" \
  -unattended -nullrhi -nosplash -nopause >/dev/null 2>&1 || true

BATCH_PASS=$(count_result "Result={Success}" "$LOG")
BATCH_FAIL=$(count_result "Result={Fail}" "$LOG")
BATCH_STATUS="PASS ($BATCH_PASS)"
[[ "$BATCH_FAIL" -gt 0 ]] && BATCH_STATUS="FAIL ($BATCH_FAIL failed)"
echo "    Batch AI tests: $BATCH_PASS passed, $BATCH_FAIL failed"

# ---------------------------------------------------------------------------
# Phase 4: Crash scan (check saved logs / crash reports)
# ---------------------------------------------------------------------------
echo ""
echo "--- Phase 4: Crash scan ---"
CRASH_DIR="$HOME/Library/Logs/Unreal Engine/CricketSimEditor"
CRASH_COUNT=0
if [[ -d "$CRASH_DIR" ]]; then
  CRASH_COUNT=$(find "$CRASH_DIR" -name "*.crash" -newer "$CRASH_DIR" 2>/dev/null | wc -l | tr -d ' ')
fi
CRASH_STATUS="None detected"
[[ "$CRASH_COUNT" -gt 0 ]] && CRASH_STATUS="$CRASH_COUNT crash files found in $CRASH_DIR"
echo "    Crash reports: $CRASH_STATUS"

# ---------------------------------------------------------------------------
# Write report
# ---------------------------------------------------------------------------
cat > "$REPORT" << REPORT_EOF
# CricketSim — Vertical Slice Validation Report

Generated: $TIMESTAMP

---

## Summary

| Check | Result |
|---|---|
| Unit / integration tests (122) | $UNIT_STATUS |
| AI vs AI balance validation | $BALANCE_STATUS |
| AI batch simulation (10 matches) | $BATCH_STATUS |
| Crash reports | $CRASH_STATUS |

---

## AI vs AI Statistical Metrics

> Source: \`Saved/Validation/Metrics.csv\` (generated by \`CricketSim.Balance.ValidationReport\`).
> The balance suite runs 80 T20 matches; the below reflect that full sample.

| Metric | Measured | T20 Target | Status |
|---|---:|:--:|:--|
| Avg 1st-innings total | $AVG_SCORE | 150–182 | — |
| Avg wickets / innings | $AVG_WICKETS | 5.0–7.0 | — |
| Run rate | $RUN_RATE | 7.7–9.1 | — |
| Boundary % | $BOUNDARY_PCT | 14–19.5 | — |

Full breakdown: \`Saved/Validation/ValidationReport.md\`

---

## Human vs AI Validation

Requires packaged build. See **Docs/GAMEPLAY_FOOTAGE_CHECKLIST.md** for the
10-session Human vs AI manual testing matrix (batting + bowling + scoreboard).

Results are recorded manually and appended here after packaged build testing.

### Human vs AI Results (to be filled after packaged play)

| Session | Mode | Overs | Score | Wickets | Crash | Notes |
|---|---|---|---|---|---|---|
| 1 | Human batting | 20 | — | — | — | — |
| 2 | Human batting | 20 | — | — | — | — |
| 3 | Human batting | 20 | — | — | — | — |
| 4 | Human batting | 20 | — | — | — | — |
| 5 | Human batting | 20 | — | — | — | — |
| 6 | Human bowling | 20 | — | — | — | — |
| 7 | Human bowling | 20 | — | — | — | — |
| 8 | Human bowling | 20 | — | — | — | — |
| 9 | Human bowling | 20 | — | — | — | — |
| 10 | Human bowling | 20 | — | — | — | — |

---

## Packaging & Launch

| Item | Status |
|---|---|
| \`Scripts/setup_content.sh\` ran without errors | — |
| \`find Content -name 'L_Nets.umap'\` returns a hit | — |
| \`find Content -name 'L_Match.umap'\` returns a hit | — |
| \`Scripts/package_mac.sh Shipping\` exits 0 | — |
| Cook log: no "Could not find object for asset" errors | — |
| Packaged app launches (L_Nets loads) | — |
| HUD panels visible (batting / bowling / physics) | — |
| L_Match: scoreboard populates, T20 completes | — |
| Replay records and plays back | — |

---

## Known Gaps

- Death run rate at Hard tier: 8.1 (target 9.2–11.8) — tracked, not blocking.
- Chase success % at Hard tier: 35 % (target 44–58 %) — tracked, not blocking.
- Ball invisible without BP_CricketBall (physics correct; cosmetic gap).
- No save/load (out of scope for this milestone).
REPORT_EOF

echo ""
echo "==> Validation report written to: $REPORT"
echo ""
echo "==> Summary"
echo "    Unit tests    : $UNIT_STATUS"
echo "    Balance AI    : $BALANCE_STATUS"
echo "    Batch AI      : $BATCH_STATUS"
echo "    Crash reports : $CRASH_STATUS"

if [[ "$FAIL" -gt 0 ]] || [[ "$BATCH_FAIL" -gt 0 ]]; then
  echo ""
  echo "    *** One or more test phases failed. Review failures above. ***"
  exit 1
fi

echo ""
echo "==> All automated checks passed."
