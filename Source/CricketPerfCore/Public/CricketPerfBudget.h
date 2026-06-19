#pragma once

#include "CoreMinimal.h"
#include "CricketPerfCategory.h"
#include "CricketPerfBudget.generated.h"

/** How a measured cost compares to its allotted budget. */
UENUM(BlueprintType)
enum class ECricketBudgetStatus : uint8
{
	UnderBudget UMETA(DisplayName = "Under Budget"), // <= warn fraction of budget
	Warning     UMETA(DisplayName = "Warning"),      // between warn fraction and 100%
	OverBudget  UMETA(DisplayName = "Over Budget")   // > 100% of budget
};

/**
 * FCricketBudgetResult — the verdict for one category this frame: how many ms it
 * actually cost, its allotted budget, the fraction used and the resulting status.
 */
USTRUCT(BlueprintType)
struct CRICKETPERFCORE_API FCricketBudgetResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") ECricketPerfCategory Category = ECricketPerfCategory::Other;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float CostMs = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float BudgetMs = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float FractionUsed = 0.0f; // CostMs / BudgetMs
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") ECricketBudgetStatus Status = ECricketBudgetStatus::UnderBudget;
};

/**
 * FCricketSimulationBudget — the Simulation Budgeting System.
 *
 * Holds a per-category millisecond budget derived from a frame-time target (e.g.
 * 60 FPS → 16.67 ms total, split across physics/AI/animation/replay/...). Pure math:
 * given a frame's measured per-category costs it returns each category's status so
 * the manager can flag, log, or (later) shed load on the area that broke the frame.
 *
 * Budgets are authored as fractions of the frame target so the same split scales
 * cleanly between the 60 FPS floor and the 120 FPS preferred target.
 */
struct CRICKETPERFCORE_API FCricketSimulationBudget
{
	/** Fraction of the budget at/above which a category is flagged "Warning". */
	double WarnFraction = 0.85;

	/** The frame-time target this budget is sized for (ms). 16.67 = 60 FPS. */
	double FrameTargetMs = 1000.0 / 60.0;

	/** Per-category budget in ms (indexed by ECricketPerfCategory). */
	double BudgetMs[CricketPerfCategoryCount] = {};

	FCricketSimulationBudget() { SetFromFrameTarget(1000.0 / 60.0); }

	/**
	 * Re-derive every category budget from a frame-time target using the default
	 * cricket-sim split (physics-first: the integrator/contact gets the largest
	 * gameplay slice, then AI, animation, replay). Whole-frame measures (Frame,
	 * threads, GPU) are budgeted at the full target.
	 */
	void SetFromFrameTarget(double InFrameTargetMs);

	/** Set a single category's budget directly (ms). */
	void SetBudget(ECricketPerfCategory Category, double Ms)
	{
		BudgetMs[static_cast<int32>(Category)] = FMath::Max(Ms, 0.0);
	}

	double GetBudget(ECricketPerfCategory Category) const
	{
		return BudgetMs[static_cast<int32>(Category)];
	}

	/** Classify a single measured cost (ms) for a category. */
	FCricketBudgetResult Evaluate(ECricketPerfCategory Category, double CostMs) const;
};
