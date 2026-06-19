#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CricketPerformanceSettings.generated.h"

/**
 * UCricketPerformanceSettings — project-wide configuration for the Optimization &
 * Profiling framework, under Project Settings ▸ Game ▸ "Cricket Performance"
 * (saved to DefaultGame.ini). Defines the FPS targets, the per-category simulation
 * budgets, the dashboard defaults and the replay-optimizer knobs. CVars
 * (cricket.Perf.*) override the live toggles; this is the editor/shipping baseline.
 *
 * Access anywhere via GetDefault<UCricketPerformanceSettings>().
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Cricket Performance"))
class CRICKETPERFORMANCE_API UCricketPerformanceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ---- Performance targets ---------------------------------------------
	/** Minimum acceptable frame rate (the budget floor). Sim must not drop below. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Targets", meta = (ClampMin = "15", ClampMax = "240"))
	int32 MinTargetFPS = 60;

	/** Preferred frame rate on capable hardware (Apple Silicon 120 Hz). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Targets", meta = (ClampMin = "30", ClampMax = "240"))
	int32 PreferredTargetFPS = 120;

	/** Budgets are sized for this target. true = preferred (120), false = min (60). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Targets")
	bool bBudgetForPreferredFPS = false;

	/** Fraction of a category budget at/above which it is flagged "Warning" (amber). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Targets", meta = (ClampMin = "0.5", ClampMax = "1.0"))
	float BudgetWarnFraction = 0.85f;

	// ---- Dashboard -------------------------------------------------------
	/** Show the on-screen profiling dashboard by default (cricket.Perf.Dashboard overrides). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Dashboard")
	bool bShowDashboardByDefault = false;

	/** Samples retained per metric for the rolling min/avg/max/p95 (≈ window size). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Dashboard", meta = (ClampMin = "30", ClampMax = "1024"))
	int32 RollingWindow = 120;

	/** Emit a warning to the log when a category goes over budget (rate-limited). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Dashboard")
	bool bLogBudgetOverruns = true;

	// ---- Replay optimization --------------------------------------------
	/** Quantize recorded positions to this resolution (mm). 0 disables quantization. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Replay Optimization", meta = (ClampMin = "0", ClampMax = "100"))
	int32 ReplayPositionQuantizeMm = 5;

	/**
	 * Adaptive sampling: drop a recorded frame when the ball moved less than this
	 * (metres) since the last kept frame AND no event occurred. 0 keeps every frame.
	 */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Replay Optimization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReplayMinMotionM = 0.02f;

	/** Warn when a single replay clip's tracked memory exceeds this (MB). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Replay Optimization", meta = (ClampMin = "1.0"))
	float ReplayMemoryWarnMB = 16.0f;

	/** The frame-time target (ms) the budgets should be sized for. */
	double FrameTargetMs() const
	{
		const int32 FPS = bBudgetForPreferredFPS ? PreferredTargetFPS : MinTargetFPS;
		return 1000.0 / FMath::Max(FPS, 1);
	}
};
