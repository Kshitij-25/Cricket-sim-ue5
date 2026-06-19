#pragma once

#include "CoreMinimal.h"
#include "CricketReplayOptimizer.h"
#include "CricketPerformanceBenchmark.generated.h"

/**
 * FCricketBenchmarkResult — the outcome of one automated benchmark: what it ran, how
 * long it took, the throughput, and a pass/fail verdict against a sanity threshold.
 * These are emitted by the headless benchmark suite and surfaced in the optimization
 * report so regressions are caught automatically rather than felt in the editor.
 */
USTRUCT(BlueprintType)
struct CRICKETPERFORMANCE_API FCricketBenchmarkResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int32 Iterations = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") double TotalMs = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") double MsPerIteration = 0.0;
	/** Domain items processed (balls bowled, frames recorded, ...). */
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int64 ItemsProcessed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") double ItemsPerSecond = 0.0;
	/** Peak tracked memory touched by the run (bytes), where meaningful. */
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int64 PeakBytes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") bool bPassed = true;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") FString Notes;
};

/**
 * FCricketPerformanceBenchmark — the automated benchmark + stress-test harness.
 *
 * It drives the REAL simulation (the deterministic CricketAI match simulator and the
 * replay optimizer) and measures wall-clock throughput, so the numbers reflect the
 * shipping code paths, not a mock. Everything here is headless and deterministic
 * given the seed, so it runs in CI as part of the CricketSim.Perf suite.
 */
class CRICKETPERFORMANCE_API FCricketPerformanceBenchmark
{
public:
	/** A full AI-vs-AI T20: drives brains → contest model → match engine. */
	static FCricketBenchmarkResult RunAIvsAIMatch(int32 Seed = 1337);

	/** Long-match stress: simulate NumMatches back-to-back and aggregate throughput. */
	static FCricketBenchmarkResult RunLongMatchStress(int32 NumMatches = 20, int32 Seed = 1337);

	/** Replay stress: build a large dense clip, run the optimizer, measure savings. */
	static FCricketBenchmarkResult RunReplayStress(int32 NumFrames = 12000, int32 NumActors = 13);

	/** Run the whole suite and return all results. */
	static TArray<FCricketBenchmarkResult> RunAll();
};
