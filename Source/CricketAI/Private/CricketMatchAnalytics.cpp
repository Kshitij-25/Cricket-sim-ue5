#include "CricketMatchAnalytics.h"
#include "CricketBatterBrain.h"   // ECricketBatterAction
#include "CricketBowlingTypes.h"  // ECricketLength
#include "CricketMatchTypes.h"    // ECricketDismissal

// ---------------------------------------------------------------------------
// Benchmarks — modern men's T20 reference bands.
//   [PassLo,PassHi] = on-target core; [WarnLo,WarnHi] = believable envelope.
// Bands are deliberately a touch wide: real T20 varies by ground, era and pitch,
// and the simulator should sit inside the spread, not on a single point.
// ---------------------------------------------------------------------------

FCricketT20Benchmarks FCricketT20Benchmarks::Default()
{
	FCricketT20Benchmarks B;
	B.FirstInningsRuns   = { 140.0, 150.0, 182.0, 200.0 };
	B.SecondInningsRuns  = { 120.0, 135.0, 172.0, 190.0 };
	B.RunRate            = {  7.2,   7.7,   9.1,   9.7 };
	B.WicketsPerInnings  = {  4.3,   5.0,   7.0,   7.8 };
	B.BoundaryPct        = { 12.0,  14.0,  19.5,  23.0 };
	B.DotPct             = { 31.0,  35.0,  42.0,  47.0 };
	B.SixPct             = {  3.0,   4.0,   7.5,   9.5 };
	B.TeamStrikeRate     = {120.0, 128.0, 151.0, 162.0 };
	B.EconomyRate        = {  7.2,   7.7,   9.1,   9.7 };
	B.BowlingAverage     = { 19.0,  22.0,  31.0,  36.0 };
	B.PowerplayRunRate   = {  6.3,   7.0,   8.9,   9.8 };
	B.DeathRunRate       = {  8.2,   9.2,  11.8,  13.0 };
	B.ChaseSuccessPct    = { 36.0,  44.0,  58.0,  64.0 };
	return B;
}

const TCHAR* FCricketMetricRow::VerdictGlyph(ECricketMetricVerdict V)
{
	switch (V)
	{
	case ECricketMetricVerdict::Pass: return TEXT("PASS");
	case ECricketMetricVerdict::Warn: return TEXT("WARN");
	default:                          return TEXT("FAIL");
	}
}

// ---------------------------------------------------------------------------
// Aggregation
// ---------------------------------------------------------------------------

namespace
{
	double Mean(const TArray<double>& X)
	{
		if (X.Num() == 0) { return 0.0; }
		double S = 0.0; for (double V : X) { S += V; }
		return S / X.Num();
	}
	double StdDev(const TArray<double>& X)
	{
		if (X.Num() < 2) { return 0.0; }
		const double M = Mean(X);
		double S = 0.0; for (double V : X) { S += (V - M) * (V - M); }
		return FMath::Sqrt(S / (X.Num() - 1));
	}
	void Normalise(TArray<double>& Counts)
	{
		double S = 0.0; for (double V : Counts) { S += V; }
		if (S > 0.0) { for (double& V : Counts) { V /= S; } }
	}
}

FCricketAggregateMetrics FCricketAggregateMetrics::FromMatches(const TArray<FCricketAIMatchTelemetry>& Matches)
{
	FCricketAggregateMetrics M;
	M.ActionMix.Init(0.0, 5);
	M.LengthMix.Init(0.0, 7);
	M.DismissalMix.Init(0.0, 6);

	int64 TotRuns = 0, TotBalls = 0, TotWkts = 0, TotFours = 0, TotSixes = 0, TotDots = 0;
	int64 PPRuns = 0, PPBalls = 0, DRuns = 0, DBalls = 0;
	int32 ChaseWins = 0, Ties = 0, TwoInnings = 0;

	TArray<double> FirstInnRuns, SecondInnRuns, InningsRunRates;

	for (const FCricketAIMatchTelemetry& Mt : Matches)
	{
		M.NumMatches++;
		if (Mt.bCompleted) { M.NumCompleted++; }
		if (Mt.bTie) { Ties++; }
		if (Mt.Innings.Num() == 2)
		{
			TwoInnings++;
			if (Mt.bChaseSucceeded) { ChaseWins++; }
		}

		for (int32 i = 0; i < Mt.Innings.Num(); ++i)
		{
			const FCricketAIInningsTelemetry& In = Mt.Innings[i];
			M.NumInnings++;
			TotRuns += In.Runs; TotBalls += In.LegalBalls; TotWkts += In.Wickets;
			TotFours += In.Fours; TotSixes += In.Sixes; TotDots += In.Dots;
			PPRuns += In.PowerplayRuns; PPBalls += In.PowerplayBalls;
			DRuns  += In.DeathRuns;     DBalls  += In.DeathBalls;

			if (In.LegalBalls > 0) { InningsRunRates.Add(In.RunRate()); }
			if (i == 0) { FirstInnRuns.Add(In.Runs); } else if (i == 1) { SecondInnRuns.Add(In.Runs); }

			for (int32 a = 0; a < In.ActionCounts.Num()    && a < 5; ++a) { M.ActionMix[a]    += In.ActionCounts[a]; }
			for (int32 l = 0; l < In.BowlLengthCounts.Num() && l < 7; ++l) { M.LengthMix[l]    += In.BowlLengthCounts[l]; }
			for (int32 d = 0; d < In.Dismissals.Num()       && d < 6; ++d) { M.DismissalMix[d] += In.Dismissals[d]; }
		}
	}

	M.AvgFirstInningsRuns  = Mean(FirstInnRuns);
	M.StdFirstInningsRuns  = StdDev(FirstInnRuns);
	M.AvgSecondInningsRuns = Mean(SecondInnRuns);
	M.AvgRunRate           = (TotBalls > 0) ? (6.0 * TotRuns / TotBalls) : 0.0;
	M.StdRunRate           = StdDev(InningsRunRates);
	M.AvgWicketsPerInnings = (M.NumInnings > 0) ? ((double)TotWkts / M.NumInnings) : 0.0;
	M.BoundaryFraction     = (TotBalls > 0) ? ((double)(TotFours + TotSixes) / TotBalls) : 0.0;
	M.DotFraction          = (TotBalls > 0) ? ((double)TotDots / TotBalls) : 0.0;
	M.SixFraction          = (TotBalls > 0) ? ((double)TotSixes / TotBalls) : 0.0;
	M.TeamStrikeRate       = (TotBalls > 0) ? (100.0 * TotRuns / TotBalls) : 0.0;
	M.EconomyRate          = M.AvgRunRate;
	M.BowlingAverage       = (TotWkts > 0) ? ((double)TotRuns / TotWkts) : 0.0;
	M.PowerplayRunRate     = (PPBalls > 0) ? (6.0 * PPRuns / PPBalls) : 0.0;
	M.DeathRunRate         = (DBalls > 0)  ? (6.0 * DRuns / DBalls)   : 0.0;
	M.ChaseSuccessFraction = (TwoInnings > 0) ? ((double)ChaseWins / TwoInnings) : 0.0;
	M.TieFraction          = (M.NumMatches > 0) ? ((double)Ties / M.NumMatches) : 0.0;

	Normalise(M.ActionMix);
	Normalise(M.LengthMix);
	Normalise(M.DismissalMix);
	return M;
}

TArray<FCricketMetricRow> FCricketAggregateMetrics::Grade(const FCricketT20Benchmarks& B) const
{
	auto Row = [](const TCHAR* Name, const TCHAR* Unit, double Value, const FCricketBenchmarkRange& Bench, const TCHAR* Src)
	{
		FCricketMetricRow R; R.Name = Name; R.Unit = Unit; R.Value = Value; R.Bench = Bench; R.Source = Src; return R;
	};

	TArray<FCricketMetricRow> Rows;
	Rows.Add(Row(TEXT("Avg 1st-innings total"), TEXT("runs"), AvgFirstInningsRuns, B.FirstInningsRuns,  TEXT("Men's T20I 1st-inn avg ~160-170")));
	Rows.Add(Row(TEXT("Avg 2nd-innings total"), TEXT("runs"), AvgSecondInningsRuns, B.SecondInningsRuns, TEXT("2nd-inn lower; chases end early")));
	Rows.Add(Row(TEXT("Run rate"),              TEXT("rpo"),  AvgRunRate,           B.RunRate,           TEXT("Top T20 overall RR ~8.0-8.5")));
	Rows.Add(Row(TEXT("Wickets / innings"),     TEXT("wkts"), AvgWicketsPerInnings, B.WicketsPerInnings, TEXT("~6 wkts/innings typical")));
	Rows.Add(Row(TEXT("Boundary %"),            TEXT("%"),    BoundaryFraction*100, B.BoundaryPct,       TEXT("~15-19% of balls are 4/6")));
	Rows.Add(Row(TEXT("Dot-ball %"),            TEXT("%"),    DotFraction*100,      B.DotPct,            TEXT("~36-40% dot balls")));
	Rows.Add(Row(TEXT("Six %"),                 TEXT("%"),    SixFraction*100,      B.SixPct,            TEXT("~5-7% of balls are six")));
	Rows.Add(Row(TEXT("Team strike rate"),      TEXT("/100"), TeamStrikeRate,       B.TeamStrikeRate,    TEXT("Runs per 100 balls ~133-142")));
	Rows.Add(Row(TEXT("Economy rate"),          TEXT("rpo"),  EconomyRate,          B.EconomyRate,       TEXT("Mirror of run rate")));
	Rows.Add(Row(TEXT("Bowling average"),       TEXT("r/wk"), BowlingAverage,       B.BowlingAverage,    TEXT("Runs per wicket ~25-28")));
	Rows.Add(Row(TEXT("Powerplay run rate"),    TEXT("rpo"),  PowerplayRunRate,     B.PowerplayRunRate,  TEXT("PP (ov 1-6) ~7.5-8.5")));
	Rows.Add(Row(TEXT("Death run rate"),        TEXT("rpo"),  DeathRunRate,         B.DeathRunRate,      TEXT("Death (ov 16-20) ~10-11")));
	Rows.Add(Row(TEXT("Chase success %"),       TEXT("%"),    ChaseSuccessFraction*100, B.ChaseSuccessPct, TEXT("~50% (slight chase edge)")));
	return Rows;
}

void FCricketAggregateMetrics::CountVerdicts(const FCricketT20Benchmarks& B, int32& OutPass, int32& OutWarn, int32& OutFail) const
{
	OutPass = OutWarn = OutFail = 0;
	for (const FCricketMetricRow& R : Grade(B))
	{
		switch (R.Verdict())
		{
		case ECricketMetricVerdict::Pass: OutPass++; break;
		case ECricketMetricVerdict::Warn: OutWarn++; break;
		default:                          OutFail++; break;
		}
	}
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

namespace
{
	const TCHAR* ActionName(int32 I)
	{
		static const TCHAR* N[] = { TEXT("Leave"), TEXT("Defend"), TEXT("Rotate"), TEXT("Attack"), TEXT("BoundaryHit") };
		return (I >= 0 && I < 5) ? N[I] : TEXT("?");
	}
	const TCHAR* LengthName(int32 I)
	{
		static const TCHAR* N[] = { TEXT("FullToss"), TEXT("Yorker"), TEXT("Full"), TEXT("GoodLength"), TEXT("BackOfLength"), TEXT("Short"), TEXT("Bouncer") };
		return (I >= 0 && I < 7) ? N[I] : TEXT("?");
	}
	const TCHAR* DismissalName(int32 I)
	{
		static const TCHAR* N[] = { TEXT("NotOut"), TEXT("Bowled"), TEXT("Caught"), TEXT("LBW"), TEXT("RunOut"), TEXT("Stumped") };
		return (I >= 0 && I < 6) ? N[I] : TEXT("?");
	}
	/** A small ASCII bar for the distribution charts (each block ~2.5%). */
	FString Bar(double Frac)
	{
		const int32 Blocks = FMath::Clamp(FMath::RoundToInt(Frac * 40.0), 0, 40);
		return FString::ChrN(Blocks, TEXT('#'));
	}
}

FString FCricketAggregateMetrics::ToMarkdown(const FString& Title, const FCricketT20Benchmarks& B) const
{
	int32 Pass = 0, Warn = 0, Fail = 0;
	CountVerdicts(B, Pass, Warn, Fail);

	FString S;
	S += FString::Printf(TEXT("## %s\n\n"), *Title);
	S += FString::Printf(TEXT("- Matches: **%d** (completed %d) | Innings sampled: **%d**\n"), NumMatches, NumCompleted, NumInnings);
	S += FString::Printf(TEXT("- Verdicts: **%d PASS / %d WARN / %d FAIL** (of %d graded metrics)\n\n"), Pass, Warn, Fail, Pass + Warn + Fail);

	// Graded metric table
	S += TEXT("| Metric | Value | On-target band | Believable band | Verdict | Real-world reference |\n");
	S += TEXT("|---|---:|:---:|:---:|:---:|---|\n");
	for (const FCricketMetricRow& R : Grade(B))
	{
		S += FString::Printf(TEXT("| %s | %.1f %s | %.1f–%.1f | %.1f–%.1f | **%s** | %s |\n"),
			*R.Name, R.Value, *R.Unit,
			R.Bench.PassLo, R.Bench.PassHi, R.Bench.WarnLo, R.Bench.WarnHi,
			FCricketMetricRow::VerdictGlyph(R.Verdict()), *R.Source);
	}

	S += FString::Printf(TEXT("\n_Spread: 1st-innings σ = %.1f runs, run-rate σ = %.2f rpo._\n\n"), StdFirstInningsRuns, StdRunRate);

	// Shot distribution report
	S += TEXT("### Shot distribution (batter intent)\n\n```\n");
	for (int32 i = 0; i < ActionMix.Num(); ++i)
	{
		S += FString::Printf(TEXT("%-12s %5.1f%%  %s\n"), ActionName(i), ActionMix[i] * 100.0, *Bar(ActionMix[i]));
	}
	S += TEXT("```\n\n");

	// Pitch map (bowling length distribution)
	S += TEXT("### Pitch map (bowling length distribution)\n\n```\n");
	for (int32 i = 0; i < LengthMix.Num(); ++i)
	{
		S += FString::Printf(TEXT("%-13s %5.1f%%  %s\n"), LengthName(i), LengthMix[i] * 100.0, *Bar(LengthMix[i]));
	}
	S += TEXT("```\n\n");

	// Bowling / dismissal report
	S += TEXT("### Dismissal breakdown\n\n```\n");
	for (int32 i = 1; i < DismissalMix.Num(); ++i)   // skip NotOut at index 0
	{
		S += FString::Printf(TEXT("%-9s %5.1f%%  %s\n"), DismissalName(i), DismissalMix[i] * 100.0, *Bar(DismissalMix[i]));
	}
	S += TEXT("```\n");

	return S;
}

FString FCricketAggregateMetrics::CsvHeader()
{
	return TEXT("Label,Matches,Avg1stInn,Std1stInn,Avg2ndInn,RunRate,WktsPerInn,BoundaryPct,DotPct,SixPct,TeamSR,Economy,BowlAvg,PowerplayRR,DeathRR,ChaseWinPct,TiePct");
}

FString FCricketAggregateMetrics::ToCsvRow(const FString& Label) const
{
	return FString::Printf(TEXT("%s,%d,%.1f,%.1f,%.1f,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%.2f,%.1f,%.2f,%.2f,%.1f,%.1f"),
		*Label, NumMatches, AvgFirstInningsRuns, StdFirstInningsRuns, AvgSecondInningsRuns,
		AvgRunRate, AvgWicketsPerInnings, BoundaryFraction*100, DotFraction*100, SixFraction*100,
		TeamStrikeRate, EconomyRate, BowlingAverage, PowerplayRunRate, DeathRunRate,
		ChaseSuccessFraction*100, TieFraction*100);
}
