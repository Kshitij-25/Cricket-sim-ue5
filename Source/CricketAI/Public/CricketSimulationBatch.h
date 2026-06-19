#pragma once

#include "CoreMinimal.h"
#include "CricketAIMatchSimulator.h"   // FCricketAITeam, FCricketAIMatchTelemetry
#include "CricketBalanceConfig.h"
#include "CricketMatchAnalytics.h"
#include "CricketMatchTypes.h"         // FCricketMatchRules

/**
 * CricketSimulationBatch — the MATCH SIMULATION FRAMEWORK.
 *
 * Runs populations of AI-vs-AI matches (hundreds at a time), across pitch conditions
 * and difficulty tiers, and folds each population into FCricketAggregateMetrics. This
 * is the experiment harness behind every statistical report: give it teams, a rule
 * set and a balance config (or a sweep of them) and it returns graded metrics plus
 * the raw telemetry for drill-down.
 *
 * Deterministic: a cell is seeded from (SeedBase, match index), and the side batting
 * first alternates by match parity so first/second-innings stats are unbiased. Pure
 * and headless — no UWorld — so it runs inside automation tests and offline tooling.
 */

/** One cell of a sweep: a label, the graded metrics and the raw matches behind them. */
struct CRICKETAI_API FCricketBatchCell
{
	FString Label;
	FCricketBalanceConfig Balance;
	FCricketAggregateMetrics Metrics;
	TArray<FCricketAIMatchTelemetry> Matches;   // raw telemetry for drill-down
};

class CRICKETAI_API FCricketSimulationBatch
{
public:
	/**
	 * Build a balanced, believable Test/T20 XI from a code prefix: top & middle-order
	 * batters, an all-rounder, a keeper, a part-timer and four front-line bowlers (a
	 * spinner + three quicks) — at least five who can legally complete 20 overs. The
	 * same XI the AI tests use, promoted to reusable production code.
	 */
	static FCricketAITeam StandardTeam(const FString& Code, ECricketAIDifficulty Difficulty);

	/** Default men's T20 rules. */
	static FCricketMatchRules StandardRules() { return FCricketMatchRules(); }

	/**
	 * Run NumMatches matches between two teams under one balance config and aggregate.
	 * Alternates who bats first by match parity.
	 */
	static FCricketBatchCell RunCell(
		const FString& Label,
		const FCricketAITeam& TeamA,
		const FCricketAITeam& TeamB,
		const FCricketMatchRules& Rules,
		const FCricketBalanceConfig& Balance,
		int32 NumMatches,
		int32 SeedBase);

	/**
	 * Sweep the standard contest across a set of pitch conditions at one difficulty.
	 * Each condition gets NumMatchesPerCondition matches.
	 */
	static TArray<FCricketBatchCell> SweepConditions(
		ECricketAIDifficulty Difficulty,
		const TArray<FCricketBalanceConfig>& Conditions,
		int32 NumMatchesPerCondition,
		int32 SeedBase = 1000);

	/**
	 * Sweep the standard, neutral contest across the four difficulty tiers (both sides
	 * at the same tier). NumMatchesPerTier matches each.
	 */
	static TArray<FCricketBatchCell> SweepDifficulties(
		int32 NumMatchesPerTier,
		int32 SeedBase = 5000);
};
