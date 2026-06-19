#include "CricketTacticalEvaluator.h"

int32 FCricketTacticalEvaluator::SelectOption(FCricketDecisionTrace& Trace,
	const FCricketAIDifficultyParams& Difficulty, FRandomStream& Rng, double PressureMistakeScale)
{
	const int32 N = Trace.Options.Num();
	if (N == 0) { Trace.ChosenIndex = INDEX_NONE; return INDEX_NONE; }

	// Find the best-scoring option (the "correct" play).
	int32 BestIdx = 0;
	double BestScore = Trace.Options[0].Score;
	for (int32 i = 1; i < N; ++i)
	{
		if (Trace.Options[i].Score > BestScore) { BestScore = Trace.Options[i].Score; BestIdx = i; }
	}

	// Softmax probabilities at the difficulty's temperature (for display + sampling).
	const double Temp = FMath::Max(Difficulty.SelectionTemperature, 1e-3);
	double SumW = 0.0;
	TArray<double, TInlineAllocator<16>> W; W.SetNumZeroed(N);
	for (int32 i = 0; i < N; ++i)
	{
		// Subtract the max for numerical stability.
		W[i] = FMath::Exp((Trace.Options[i].Score - BestScore) / Temp);
		SumW += W[i];
	}
	for (int32 i = 0; i < N; ++i)
	{
		Trace.Options[i].Probability = (SumW > 0.0) ? (W[i] / SumW) : (1.0 / N);
		Trace.Options[i].bChosen = false;
	}

	int32 Chosen;
	const double MistakeChance = FMath::Clamp(Difficulty.MistakeRate * PressureMistakeScale, 0.0, 1.0);
	if (N > 1 && Rng.FRand() < MistakeChance)
	{
		// A gross misjudgement: pick uniformly among the NON-best options.
		int32 Pick = Rng.RandRange(0, N - 2);
		Chosen = (Pick >= BestIdx) ? Pick + 1 : Pick;
	}
	else if (Difficulty.SelectionTemperature <= 1e-3)
	{
		// Simulation: always the best play.
		Chosen = BestIdx;
	}
	else
	{
		// Sample from the softmax.
		const double R = Rng.FRand() * SumW;
		double Acc = 0.0; Chosen = N - 1;
		for (int32 i = 0; i < N; ++i)
		{
			Acc += W[i];
			if (R <= Acc) { Chosen = i; break; }
		}
	}

	Trace.Options[Chosen].bChosen = true;
	Trace.ChosenIndex = Chosen;
	return Chosen;
}

ECricketInningsApproach FCricketTacticalEvaluator::PhaseApproach(const FCricketMatchSituation& S,
	ECricketInningsApproach Powerplay, ECricketInningsApproach Middle, ECricketInningsApproach Death)
{
	switch (S.Phase)
	{
	case ECricketMatchPhase::Powerplay: return Powerplay;
	case ECricketMatchPhase::Death:     return Death;
	default:                            return Middle;
	}
}

double FCricketTacticalEvaluator::BattingAggressionTarget(const FCricketMatchSituation& S,
	ECricketInningsApproach PhaseApproach, double Awareness)
{
	// 1. Base intent from the team's chosen approach for this phase. Even a
	// "consolidating" T20 side ticks along near a run a ball, so the bases sit higher
	// than a Test-match reading would.
	double Base = 0.55;
	switch (PhaseApproach)
	{
	case ECricketInningsApproach::Anchor:      Base = 0.42; break;
	case ECricketInningsApproach::Consolidate: Base = 0.56; break;
	case ECricketInningsApproach::Accelerate:  Base = 0.74; break;
	case ECricketInningsApproach::AllOut:      Base = 0.93; break;
	case ECricketInningsApproach::Defend:      Base = 0.48; break;
	}

	// 2. Situational override in a chase: the required rate dictates everything.
	double Situational = Base;
	if (S.bChasing)
	{
		const double Gap = S.RequiredRunRate - S.CurrentRunRate;       // how far behind
		// Chase from the SAME phase shape the first innings would use (Base), then modulate
		// by how far ahead/behind the required rate the chase is. On-rate, the chaser bats
		// like a first-innings side in this phase, so it tracks the target instead of sitting
		// at a flat, over-conservative tempo and falling ~20 short. (Calibrated from the
		// validation batch: a flat chase baseline gave only ~12% chase success.)
		// Note: a STEEPER gap response was tried and made chasing WORSE — pushing harder
		// when behind just loses more wickets (chasing here is wicket-limited, not rate-
		// limited), so the response is deliberately gentle.
		Situational = FMath::Clamp(Base + Gap * 0.06, 0.20, 1.0);
		if (S.RequiredRunRate > 12.0) { Situational = FMath::Max(Situational, 0.9); }
		if (S.RunsRequired <= 0)       { Situational = 0.15; }          // already won-ish
	}
	else
	{
		// First innings: lean on the projected total vs par sense baked into Pressure,
		// but accelerate at the death.
		Situational = Base + (S.Phase == ECricketMatchPhase::Death ? 0.25 : 0.0);
	}

	// 3. Temper for wickets in hand — fewer wickets, more caution (saturating).
	const double WicketCaution = FMath::Clamp((4.0 - S.WicketsInHand) * 0.10, 0.0, 0.4);
	Situational = FMath::Clamp(Situational - WicketCaution, 0.1, 1.0);

	// 4. A low-awareness AI can't fully read the situation: blend toward a flat 0.5.
	const double A = FMath::Clamp(Awareness, 0.0, 1.0);
	return FMath::Clamp(FMath::Lerp(0.5, Situational, A), 0.0, 1.0);
}
