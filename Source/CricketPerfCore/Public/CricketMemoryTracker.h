#pragma once

#include "CoreMinimal.h"
#include "CricketPerfCategory.h"
#include "CricketMemoryTracker.generated.h"

/** A single category's tracked memory footprint, in bytes and MB. */
USTRUCT(BlueprintType)
struct CRICKETPERFCORE_API FCricketMemoryEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") ECricketPerfCategory Category = ECricketPerfCategory::Other;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int64 Bytes = 0;

	float MB() const { return static_cast<float>(static_cast<double>(Bytes) / (1024.0 * 1024.0)); }
};

/**
 * FCricketMemoryLedger — the Memory Tracking System core.
 *
 * A pure, per-category byte ledger that systems report their estimated footprint
 * into (replay buffers, AI scratch, prediction caches). It does NOT hook the global
 * allocator — it tracks the sim's own large, knowable buffers so the dashboard can
 * show "Replay memory: 4.2 MB" and the framework can warn before a long innings'
 * recording grows unbounded. Process-wide physical usage is sampled separately by
 * the manager via FPlatformMemory and kept as Total/Peak here for context.
 */
struct CRICKETPERFCORE_API FCricketMemoryLedger
{
	/** Set a category's footprint to an absolute byte count (replaces prior value). */
	void Set(ECricketPerfCategory Category, int64 Bytes)
	{
		Tracked[static_cast<int32>(Category)] = FMath::Max<int64>(Bytes, 0);
	}

	/** Add (or subtract, with a negative delta) to a category's footprint. */
	void Add(ECricketPerfCategory Category, int64 DeltaBytes)
	{
		int64& B = Tracked[static_cast<int32>(Category)];
		B = FMath::Max<int64>(B + DeltaBytes, 0);
	}

	int64 Get(ECricketPerfCategory Category) const { return Tracked[static_cast<int32>(Category)]; }

	/** Sum of all tracked categories (the sim's own buffers). */
	int64 TotalTracked() const
	{
		int64 Sum = 0;
		for (int64 B : Tracked) { Sum += B; }
		return Sum;
	}

	void Reset() { for (int64& B : Tracked) { B = 0; } }

	FCricketMemoryEntry Entry(ECricketPerfCategory Category) const
	{
		FCricketMemoryEntry E; E.Category = Category; E.Bytes = Get(Category); return E;
	}

	/** Process physical usage (bytes), sampled from the platform; for context only. */
	int64 ProcessUsedBytes = 0;
	int64 ProcessPeakBytes = 0;

private:
	int64 Tracked[CricketPerfCategoryCount] = {};
};
