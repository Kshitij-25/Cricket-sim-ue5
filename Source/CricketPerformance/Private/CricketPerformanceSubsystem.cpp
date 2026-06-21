#include "CricketPerformanceSubsystem.h"
#include "CricketPerformanceSettings.h"
#include "CricketPerfProfiler.h"
#include "CricketReplayOptimizer.h"
#include "CricketReplayComponent.h"   // CricketGameplay: live replay-clip memory
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"              // TActorIterator
#include "GameFramework/Actor.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "RenderingThread.h"         // GGameThreadTime, GRenderThreadTime
#include "RHI.h"                     // RHIGetGPUFrameCycles
#include "DynamicRHI.h"              // RHIGetGPUFrameCycles

DEFINE_LOG_CATEGORY_STATIC(LogCricketPerf, Log, All);

// ---- Console variables: cricket.Perf.* (-1 = inherit project settings) ----
namespace
{
	TAutoConsoleVariable<int32> CVarPerfEnable(TEXT("cricket.Perf.Enable"), 1,
		TEXT("Enable the performance profiler (scope timers + manager). 0=off, 1=on."));
	TAutoConsoleVariable<int32> CVarPerfDashboard(TEXT("cricket.Perf.Dashboard"), -1,
		TEXT("On-screen profiling dashboard. -1=project settings, 0=off, 1=on."));
	TAutoConsoleVariable<int32> CVarPerfLogOverruns(TEXT("cricket.Perf.LogOverruns"), -1,
		TEXT("Log budget overruns. -1=project settings, 0=off, 1=on."));

	constexpr double ReplayScanInterval = 0.5;  // s between live replay-memory scans
	constexpr double LogThrottleSeconds = 2.0;  // min gap between overrun logs per category
	constexpr int32 DashboardKeyBase = 0x5C1C7; // stable AddOnScreenDebugMessage key base

	FColor StatusColor(ECricketBudgetStatus S)
	{
		switch (S)
		{
		case ECricketBudgetStatus::OverBudget: return FColor(255, 80, 80);
		case ECricketBudgetStatus::Warning:    return FColor(255, 200, 60);
		default:                               return FColor(120, 230, 120);
		}
	}
}

void UCricketPerformanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UCricketPerformanceSettings* Settings = GetDefault<UCricketPerformanceSettings>();
	const int32 Window = Settings ? Settings->RollingWindow : 120;

	for (FCricketRollingStat& Stat : Timings) { Stat.Reset(Window); }

	Budget.WarnFraction = Settings ? Settings->BudgetWarnFraction : 0.85;
	Budget.SetFromFrameTarget(Settings ? Settings->FrameTargetMs() : (1000.0 / 60.0));

	// Start a clean profiler frame.
	FCricketProfiler::Get().ResetFrame();
}

void UCricketPerformanceSubsystem::Deinitialize()
{
	if (GEngine)
	{
		// Clear any dashboard lines we own.
		for (int32 i = 0; i < CricketPerfCategoryCount + 4; ++i)
		{
			GEngine->RemoveOnScreenDebugMessage(DashboardKeyBase + i);
		}
	}
	Super::Deinitialize();
}

bool UCricketPerformanceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCricketPerformanceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCricketPerformanceSubsystem, STATGROUP_Tickables);
}

bool UCricketPerformanceSubsystem::ProfilingEnabled() const
{
	return CVarPerfEnable.GetValueOnGameThread() != 0;
}

bool UCricketPerformanceSubsystem::DashboardEnabled() const
{
	if (bDashboardOverride >= 0) { return bDashboardOverride != 0; }
	const int32 V = CVarPerfDashboard.GetValueOnGameThread();
	if (V >= 0) { return V != 0; }
	const UCricketPerformanceSettings* Settings = GetDefault<UCricketPerformanceSettings>();
	return Settings && Settings->bShowDashboardByDefault;
}

void UCricketPerformanceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const bool bEnabled = ProfilingEnabled();
	FCricketProfiler::Get().SetEnabled(bEnabled);
	if (!bEnabled)
	{
		// Keep the profiler from accumulating stale time while disabled.
		FCricketProfiler::Get().ResetFrame();
		return;
	}

	SampleWholeFrame(DeltaTime);
	SampleScopes();

	TimeSinceReplayScan += DeltaTime;
	if (TimeSinceReplayScan >= ReplayScanInterval)
	{
		TimeSinceReplayScan = 0.0;
		SampleMemory();
	}

	EvaluateBudgets();

	if (DashboardEnabled())
	{
		DrawDashboard();
	}
}

void UCricketPerformanceSubsystem::SampleWholeFrame(float DeltaTime)
{
	// Frame wall time from the tick delta (s → ms).
	Timings[static_cast<int32>(ECricketPerfCategory::Frame)].Push(DeltaTime * 1000.0);

	// Engine thread/GPU measures are per-frame cycle counters.
	Timings[static_cast<int32>(ECricketPerfCategory::GameThread)].Push(
		FPlatformTime::ToMilliseconds(GGameThreadTime));
	Timings[static_cast<int32>(ECricketPerfCategory::RenderThread)].Push(
		FPlatformTime::ToMilliseconds(GRenderThreadTime));
	Timings[static_cast<int32>(ECricketPerfCategory::GPU)].Push(
		FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
}

void UCricketPerformanceSubsystem::SampleScopes()
{
	const FCricketFrameTimings Frame = FCricketProfiler::Get().CaptureAndReset();

	// Push the gameplay scope categories (the whole-frame ones were sampled separately).
	static const ECricketPerfCategory ScopeCats[] = {
		ECricketPerfCategory::Physics, ECricketPerfCategory::Prediction,
		ECricketPerfCategory::AI, ECricketPerfCategory::Animation,
		ECricketPerfCategory::Replay, ECricketPerfCategory::Other };

	for (ECricketPerfCategory C : ScopeCats)
	{
		const int32 Idx = static_cast<int32>(C);
		Timings[Idx].Push(Frame.GetMs(C));
		CallsLastFrame[Idx] = Frame.GetCalls(C);
	}
}

void UCricketPerformanceSubsystem::SampleMemory()
{
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	Memory.ProcessUsedBytes = static_cast<int64>(MemStats.UsedPhysical);
	Memory.ProcessPeakBytes = static_cast<int64>(MemStats.PeakUsedPhysical);

	// Sum live replay-clip memory across the world (the sim's largest dynamic buffer).
	int64 ReplayBytes = 0;
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (const UCricketReplayComponent* Replay = It->FindComponentByClass<UCricketReplayComponent>())
			{
				ReplayBytes += FCricketReplayOptimizer::EstimateClipBytes(Replay->GetClip());
			}
		}
	}
	Memory.Set(ECricketPerfCategory::Replay, ReplayBytes);
}

void UCricketPerformanceSubsystem::EvaluateBudgets()
{
	const bool bLog = [this]()
	{
		const int32 V = CVarPerfLogOverruns.GetValueOnGameThread();
		if (V >= 0) { return V != 0; }
		const UCricketPerformanceSettings* S = GetDefault<UCricketPerformanceSettings>();
		return S && S->bLogBudgetOverruns;
	}();

	for (int32 i = 0; i < CricketPerfCategoryCount; ++i)
	{
		const ECricketPerfCategory C = static_cast<ECricketPerfCategory>(i);
		LastBudget[i] = Budget.Evaluate(C, Timings[i].Last());

		SecondsSinceLog[i] += FApp::GetDeltaTime();
		if (bLog && LastBudget[i].Status == ECricketBudgetStatus::OverBudget
			&& SecondsSinceLog[i] >= LogThrottleSeconds)
		{
			SecondsSinceLog[i] = 0.0;
			UE_LOG(LogCricketPerf, Warning,
				TEXT("Budget overrun: %s used %.2f ms of %.2f ms (%.0f%%)"),
				LexToString(C), LastBudget[i].CostMs, LastBudget[i].BudgetMs,
				LastBudget[i].FractionUsed * 100.0f);
		}
	}
}

void UCricketPerformanceSubsystem::DrawDashboard()
{
	if (!GEngine) { return; }

	const FCricketRollingStat& Frame = Timings[static_cast<int32>(ECricketPerfCategory::Frame)];
	const double AvgMs = Frame.Average();
	const double FPS = AvgMs > KINDA_SMALL_NUMBER ? 1000.0 / AvgMs : 0.0;
	const UCricketPerformanceSettings* Settings = GetDefault<UCricketPerformanceSettings>();
	const int32 MinFPS = Settings ? Settings->MinTargetFPS : 60;
	const int32 PrefFPS = Settings ? Settings->PreferredTargetFPS : 120;

	const FColor FpsColor = FPS >= PrefFPS ? FColor(120, 230, 120)
		: (FPS >= MinFPS ? FColor(255, 200, 60) : FColor(255, 80, 80));

	int32 Key = DashboardKeyBase;
	const float Hold = 1.1f; // refreshed every frame; outlast one frame

	GEngine->AddOnScreenDebugMessage(Key++, Hold, FpsColor, TEXT("== CRICKET PERFORMANCE =="));
	GEngine->AddOnScreenDebugMessage(Key++, Hold, FpsColor, FString::Printf(
		TEXT("FPS %.0f  (frame %.2f ms, p95 %.2f)  target %d/%d"),
		FPS, AvgMs, Frame.Percentile(0.95), MinFPS, PrefFPS));

	// One line per gameplay category: avg / p95 vs budget, coloured by status.
	static const ECricketPerfCategory Rows[] = {
		ECricketPerfCategory::GameThread, ECricketPerfCategory::RenderThread, ECricketPerfCategory::GPU,
		ECricketPerfCategory::Physics, ECricketPerfCategory::Prediction, ECricketPerfCategory::AI,
		ECricketPerfCategory::Animation, ECricketPerfCategory::Replay };

	for (ECricketPerfCategory C : Rows)
	{
		const int32 Idx = static_cast<int32>(C);
		const FCricketRollingStat& S = Timings[Idx];
		const FCricketBudgetResult& B = LastBudget[Idx];
		const FString Line = FString::Printf(TEXT("%-12s %5.2f ms (p95 %5.2f) | budget %5.2f  %3.0f%%"),
			LexToString(C), S.Average(), S.Percentile(0.95), B.BudgetMs, B.FractionUsed * 100.0f);
		GEngine->AddOnScreenDebugMessage(Key++, Hold, StatusColor(B.Status), Line);
	}

	// Memory footprint line.
	const double ReplayMB = static_cast<double>(Memory.Get(ECricketPerfCategory::Replay)) / (1024.0 * 1024.0);
	const double ProcMB = static_cast<double>(Memory.ProcessUsedBytes) / (1024.0 * 1024.0);
	const float ReplayWarnMB = Settings ? Settings->ReplayMemoryWarnMB : 16.0f;
	const FColor MemColor = ReplayMB > ReplayWarnMB ? FColor(255, 200, 60) : FColor(160, 200, 255);
	GEngine->AddOnScreenDebugMessage(Key++, Hold, MemColor, FString::Printf(
		TEXT("Replay mem %.2f MB   |   process %.0f MB"), ReplayMB, ProcMB));
}

// ---- Query API ----------------------------------------------------------------

FCricketStatSnapshot UCricketPerformanceSubsystem::GetTiming(ECricketPerfCategory Category) const
{
	return Timings[static_cast<int32>(Category)].Snapshot();
}

FCricketBudgetResult UCricketPerformanceSubsystem::GetBudget(ECricketPerfCategory Category) const
{
	return LastBudget[static_cast<int32>(Category)];
}

float UCricketPerformanceSubsystem::GetAverageFPS() const
{
	const double AvgMs = Timings[static_cast<int32>(ECricketPerfCategory::Frame)].Average();
	return AvgMs > KINDA_SMALL_NUMBER ? static_cast<float>(1000.0 / AvgMs) : 0.0f;
}

FCricketPerformanceReport UCricketPerformanceSubsystem::BuildReport() const
{
	FCricketPerformanceReport Report;

	const FCricketRollingStat& Frame = Timings[static_cast<int32>(ECricketPerfCategory::Frame)];
	Report.FrameMsAvg = static_cast<float>(Frame.Average());
	Report.FrameMsP95 = static_cast<float>(Frame.Percentile(0.95));
	Report.AverageFPS = GetAverageFPS();

	const UCricketPerformanceSettings* Settings = GetDefault<UCricketPerformanceSettings>();
	const int32 MinFPS = Settings ? Settings->MinTargetFPS : 60;
	const int32 PrefFPS = Settings ? Settings->PreferredTargetFPS : 120;
	Report.bMeetingMinTarget = Report.AverageFPS >= MinFPS;
	Report.bMeetingPreferredTarget = Report.AverageFPS >= PrefFPS;

	for (int32 i = 0; i < CricketPerfCategoryCount; ++i)
	{
		FCricketPerfCategorySnapshot Snap;
		Snap.Category = static_cast<ECricketPerfCategory>(i);
		Snap.Timing = Timings[i].Snapshot();
		Snap.Budget = LastBudget[i];
		Snap.CallsLastFrame = CallsLastFrame[i];
		Report.Categories.Add(Snap);
	}

	for (int32 i = 0; i < CricketPerfCategoryCount; ++i)
	{
		Report.Memory.Add(Memory.Entry(static_cast<ECricketPerfCategory>(i)));
	}
	Report.ProcessMemoryMB = static_cast<float>(
		static_cast<double>(Memory.ProcessUsedBytes) / (1024.0 * 1024.0));

	return Report;
}
