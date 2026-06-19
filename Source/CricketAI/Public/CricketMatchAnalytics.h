#pragma once

#include "CoreMinimal.h"
#include "CricketAIMatchSimulator.h"   // FCricketAIMatchTelemetry

/**
 * CricketMatchAnalytics — the ANALYTICS ARCHITECTURE and STATISTICAL VALIDATION core.
 *
 * It turns a population of simulated matches (FCricketAIMatchTelemetry) into the full
 * suite of cricket metrics the brief asks for — innings totals, run/strike/economy
 * rates, bowling averages, wicket / boundary / dot frequencies, phase splits and the
 * chase outcome — and then GRADES each one against published men's T20 benchmarks,
 * emitting Pass / Warn / Fail verdicts plus ready-to-write Markdown and CSV reports.
 *
 * Pure, headless and deterministic: the same telemetry always yields the same report,
 * so the automation tests can assert the simulation produces believable cricket and
 * the reports become reproducible deliverables.
 */

// ---------------------------------------------------------------------------
// Benchmark grading
// ---------------------------------------------------------------------------

/** Verdict of one metric against its real-world benchmark band. */
enum class ECricketMetricVerdict : uint8 { Pass, Warn, Fail };

/**
 * A real-world benchmark band: [WarnLo, WarnHi] is the believable envelope, with the
 * tighter [PassLo, PassHi] the on-target core. A value inside the core Passes, inside
 * the envelope Warns, outside Fails.
 */
struct CRICKETAI_API FCricketBenchmarkRange
{
	double WarnLo = 0.0, PassLo = 0.0, PassHi = 0.0, WarnHi = 0.0;

	ECricketMetricVerdict Classify(double V) const
	{
		if (V >= PassLo && V <= PassHi) { return ECricketMetricVerdict::Pass; }
		if (V >= WarnLo && V <= WarnHi) { return ECricketMetricVerdict::Warn; }
		return ECricketMetricVerdict::Fail;
	}
};

/** One graded row of the validation table. */
struct CRICKETAI_API FCricketMetricRow
{
	FString Name;
	FString Unit;
	double Value = 0.0;
	FCricketBenchmarkRange Bench;
	FString Source;   // where the real-world band comes from

	ECricketMetricVerdict Verdict() const { return Bench.Classify(Value); }
	static const TCHAR* VerdictGlyph(ECricketMetricVerdict V);
};

/**
 * FCricketT20Benchmarks — the canonical men's T20 reference bands (modern men's T20I /
 * top franchise cricket, ~2018-2024). Centralised so the targets are data, not magic
 * numbers scattered through the tests. Sources are recorded per band in the report.
 */
struct CRICKETAI_API FCricketT20Benchmarks
{
	FCricketBenchmarkRange FirstInningsRuns;
	FCricketBenchmarkRange SecondInningsRuns;
	FCricketBenchmarkRange RunRate;             // runs/over
	FCricketBenchmarkRange WicketsPerInnings;
	FCricketBenchmarkRange BoundaryPct;         // % of legal balls that go for 4 or 6
	FCricketBenchmarkRange DotPct;              // % of legal balls that are dots
	FCricketBenchmarkRange SixPct;              // % of legal balls that go for 6
	FCricketBenchmarkRange TeamStrikeRate;      // runs per 100 balls
	FCricketBenchmarkRange EconomyRate;         // runs/over conceded
	FCricketBenchmarkRange BowlingAverage;      // runs per wicket
	FCricketBenchmarkRange PowerplayRunRate;
	FCricketBenchmarkRange DeathRunRate;
	FCricketBenchmarkRange ChaseSuccessPct;     // % of matches won by the chasing side

	static FCricketT20Benchmarks Default();
};

// ---------------------------------------------------------------------------
// Aggregate metrics
// ---------------------------------------------------------------------------

/**
 * FCricketAggregateMetrics — the full statistical picture of a population of matches.
 * Every field is a mean over the sample unless named otherwise.
 */
struct CRICKETAI_API FCricketAggregateMetrics
{
	int32 NumMatches = 0;
	int32 NumInnings = 0;
	int32 NumCompleted = 0;

	// Headline scoring
	double AvgFirstInningsRuns = 0.0,  StdFirstInningsRuns = 0.0;
	double AvgSecondInningsRuns = 0.0;
	double AvgRunRate = 0.0,           StdRunRate = 0.0;
	double AvgWicketsPerInnings = 0.0;

	// Frequencies (fractions in [0,1] unless *Pct)
	double BoundaryFraction = 0.0;
	double DotFraction = 0.0;
	double SixFraction = 0.0;
	double TeamStrikeRate = 0.0;       // runs / 100 balls
	double EconomyRate = 0.0;          // == run rate, from the bowling side
	double BowlingAverage = 0.0;       // runs / wicket

	// Phase
	double PowerplayRunRate = 0.0;
	double DeathRunRate = 0.0;

	// Outcome
	double ChaseSuccessFraction = 0.0;
	double TieFraction = 0.0;

	// Distributions (fractions that sum to ~1)
	TArray<double> ActionMix;     // [Leave, Defend, Rotate, Attack, BoundaryHit]
	TArray<double> LengthMix;     // [FullToss..Bouncer] (7)
	TArray<double> DismissalMix;  // [NotOut, Bowled, Caught, LBW, RunOut, Stumped]

	/** Build the full picture from a population of completed matches. */
	static FCricketAggregateMetrics FromMatches(const TArray<FCricketAIMatchTelemetry>& Matches);

	/** Grade every headline metric against the benchmarks. */
	TArray<FCricketMetricRow> Grade(const FCricketT20Benchmarks& Bench) const;

	/** Count verdicts across the graded rows. */
	void CountVerdicts(const FCricketT20Benchmarks& Bench, int32& OutPass, int32& OutWarn, int32& OutFail) const;

	/** A human-readable Markdown validation report (table + distributions). */
	FString ToMarkdown(const FString& Title, const FCricketT20Benchmarks& Bench) const;

	/** A single CSV row of the headline metrics (with a header helper). */
	static FString CsvHeader();
	FString ToCsvRow(const FString& Label) const;
};
