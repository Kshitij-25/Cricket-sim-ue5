#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CricketPerfCategory.h"
#include "CricketPerfStats.h"
#include "CricketPerfBudget.h"
#include "CricketMemoryTracker.h"
#include "CricketPerformanceSubsystem.generated.h"

/**
 * FCricketPerfCategorySnapshot — one category's full readout for the HUD/dashboard:
 * its rolling timing stats and its budget verdict this frame.
 */
USTRUCT(BlueprintType)
struct CRICKETPERFORMANCE_API FCricketPerfCategorySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") ECricketPerfCategory Category = ECricketPerfCategory::Other;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") FCricketStatSnapshot Timing;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") FCricketBudgetResult Budget;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int32 CallsLastFrame = 0;
};

/**
 * FCricketPerformanceReport — a complete, Blueprint-friendly snapshot of the frame:
 * FPS, every category's timing + budget, and the memory breakdown. This is the
 * single struct the HUD widget (or an automated benchmark) reads.
 */
USTRUCT(BlueprintType)
struct CRICKETPERFORMANCE_API FCricketPerformanceReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float AverageFPS = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float FrameMsAvg = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float FrameMsP95 = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") TArray<FCricketPerfCategorySnapshot> Categories;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") TArray<FCricketMemoryEntry> Memory;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float ProcessMemoryMB = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") bool bMeetingMinTarget = true;     // >= 60 FPS
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") bool bMeetingPreferredTarget = false; // >= 120 FPS
};

/**
 * UCricketPerformanceSubsystem — the PERFORMANCE MANAGER.
 *
 * A world subsystem (auto-created, no level placement) that is the hub of the
 * profiling & optimization framework. Each frame it:
 *   - samples the whole-frame measures (frame time, game/render-thread, GPU) from
 *     the engine, and the gameplay scope times (physics/AI/animation/replay/...) the
 *     instrumented systems accumulated into FCricketProfiler;
 *   - pushes them through per-metric rolling windows (min/avg/max/p95);
 *   - tracks the sim's large buffers (notably live replay-clip memory) in the
 *     Memory Tracking System and samples process physical usage;
 *   - evaluates every category against the Simulation Budgeting System and flags /
 *     logs overruns (the amber/red on the dashboard);
 *   - draws the on-screen Profiling Dashboard (cvar cricket.Perf.Dashboard).
 *
 * It holds no gameplay state and never writes back into the simulation — pure
 * observation. The HUD and the automated benchmarks read BuildReport().
 */
UCLASS()
class CRICKETPERFORMANCE_API UCricketPerformanceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Subsystem lifecycle ---
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// --- Tickable ---
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	// --- Public query API (HUD / benchmarks) ---
	UFUNCTION(BlueprintCallable, Category = "Cricket|Perf")
	FCricketStatSnapshot GetTiming(ECricketPerfCategory Category) const;

	UFUNCTION(BlueprintCallable, Category = "Cricket|Perf")
	FCricketBudgetResult GetBudget(ECricketPerfCategory Category) const;

	UFUNCTION(BlueprintCallable, Category = "Cricket|Perf")
	float GetAverageFPS() const;

	UFUNCTION(BlueprintCallable, Category = "Cricket|Perf")
	FCricketPerformanceReport BuildReport() const;

	/** Toggle the on-screen dashboard at runtime (also via cricket.Perf.Dashboard). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Perf")
	void SetDashboardVisible(bool bVisible) { bDashboardOverride = bVisible ? 1 : 0; }

	/** Report a category's tracked memory footprint (bytes). For instrumented systems. */
	void ReportMemory(ECricketPerfCategory Category, int64 Bytes) { Memory.Set(Category, Bytes); }

private:
	void SampleWholeFrame(float DeltaTime);
	void SampleScopes();
	void SampleMemory();
	void EvaluateBudgets();
	void DrawDashboard();

	bool DashboardEnabled() const;
	bool ProfilingEnabled() const;

	// Per-category rolling timing windows (ms).
	FCricketRollingStat Timings[CricketPerfCategoryCount];
	// Per-category scope call counts last frame.
	int32 CallsLastFrame[CricketPerfCategoryCount] = {};
	// Latest budget verdicts.
	FCricketBudgetResult LastBudget[CricketPerfCategoryCount];

	FCricketSimulationBudget Budget;
	FCricketMemoryLedger Memory;

	// Rate-limited overrun logging (seconds since last log, per category).
	double SecondsSinceLog[CricketPerfCategoryCount] = {};

	// -1 = follow settings/cvar; 0/1 = explicit runtime override.
	int32 bDashboardOverride = -1;

	double TimeSinceReplayScan = 0.0;
};
