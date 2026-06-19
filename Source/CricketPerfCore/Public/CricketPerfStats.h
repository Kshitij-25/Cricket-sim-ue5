#pragma once

#include "CoreMinimal.h"
#include "CricketPerfStats.generated.h"

/**
 * FCricketStatSnapshot — an immutable, Blueprint-friendly readout of one metric's
 * rolling window: last value plus min/avg/max/p95 over the window, in milliseconds
 * (or MB, for memory metrics). This is what the dashboard and the HUD consume; the
 * heavy ring buffer (FCricketRollingStat) stays in C++.
 */
USTRUCT(BlueprintType)
struct CRICKETPERFCORE_API FCricketStatSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float Last = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float Min = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float Avg = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float Max = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float P95 = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int32 Samples = 0;
};

/**
 * FCricketRollingStat — a fixed-capacity ring of recent samples with O(1) push and
 * cheap aggregate queries. Used for every profiled metric so the dashboard shows a
 * stable smoothed value (avg) AND the spikes that actually break frame budgets (max,
 * p95) rather than a jittery instantaneous number.
 *
 * Pure C++: no UObject, no allocation after Reset(capacity). Headlessly testable.
 */
struct CRICKETPERFCORE_API FCricketRollingStat
{
	explicit FCricketRollingStat(int32 InCapacity = 120) { Reset(InCapacity); }

	/** Re-size and clear the window. */
	void Reset(int32 InCapacity);

	/** Clear samples but keep capacity. */
	void Clear();

	/** Push one sample (ms or MB). Evicts the oldest when full. */
	void Push(double Value);

	int32 Num() const { return Count; }
	int32 Capacity() const { return Samples.Num(); }
	bool IsEmpty() const { return Count == 0; }

	double Last() const { return LastValue; }
	double Min() const;
	double Max() const;
	double Average() const;
	/** P-th percentile (0..1) over the window, e.g. Percentile(0.95). */
	double Percentile(double P) const;

	FCricketStatSnapshot Snapshot() const;

private:
	TArray<double> Samples; // ring storage
	int32 Head = 0;         // next write index
	int32 Count = 0;        // valid samples
	double LastValue = 0.0;
};
