// Headless automation tests for the Optimization & Profiling framework. They validate
// the pure cores directly — the rolling-stat math, the simulation budgeting, the
// memory ledger, the profiler accumulator, the replay optimizer — and then run the
// real benchmark/stress harness end-to-end (AI-vs-AI match + replay stress) and
// assert the throughput/quality verdicts. All pure; no UWorld.
//
// Run: UnrealEditor-Cmd CricketSim.uproject \
//   -ExecCmds="Automation RunTests CricketSim.Perf; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketPerfStats.h"
#include "CricketPerfBudget.h"
#include "CricketMemoryTracker.h"
#include "CricketPerfProfiler.h"
#include "CricketReplayOptimizer.h"
#include "CricketReplayTypes.h"
#include "CricketPerformanceBenchmark.h"

#if WITH_DEV_AUTOMATION_TESTS

// 1. ROLLING STAT — aggregates and the ring eviction behave.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPerfRollingStatTest,
	"CricketSim.Perf.RollingStat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPerfRollingStatTest::RunTest(const FString&)
{
	FCricketRollingStat Stat(5);
	TestTrue(TEXT("starts empty"), Stat.IsEmpty());

	for (int32 i = 1; i <= 5; ++i) { Stat.Push(static_cast<double>(i)); } // 1..5
	TestEqual(TEXT("count"), Stat.Num(), 5);
	TestEqual(TEXT("min"), Stat.Min(), 1.0);
	TestEqual(TEXT("max"), Stat.Max(), 5.0);
	TestEqual(TEXT("avg"), Stat.Average(), 3.0);
	TestEqual(TEXT("last"), Stat.Last(), 5.0);
	TestEqual(TEXT("p100 == max"), Stat.Percentile(1.0), 5.0);
	TestEqual(TEXT("p0 == min"), Stat.Percentile(0.0), 1.0);

	// Overflow the ring: 6,7 evict 1,2 → window is {3,4,5,6,7}.
	Stat.Push(6.0); Stat.Push(7.0);
	TestEqual(TEXT("capacity held"), Stat.Num(), 5);
	TestEqual(TEXT("min after evict"), Stat.Min(), 3.0);
	TestEqual(TEXT("max after evict"), Stat.Max(), 7.0);

	return true;
}

// 2. SIMULATION BUDGET — derives from frame target and classifies cost correctly.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPerfBudgetTest,
	"CricketSim.Perf.Budget", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPerfBudgetTest::RunTest(const FString&)
{
	FCricketSimulationBudget Budget;
	Budget.WarnFraction = 0.85;
	Budget.SetFromFrameTarget(1000.0 / 60.0); // 16.667 ms

	const double Frame = Budget.GetBudget(ECricketPerfCategory::Frame);
	TestTrue(TEXT("frame budget ~16.67ms"), FMath::IsNearlyEqual(Frame, 1000.0 / 60.0, 0.01));

	const double PhysBudget = Budget.GetBudget(ECricketPerfCategory::Physics);
	TestTrue(TEXT("physics budget > 0"), PhysBudget > 0.0);
	TestTrue(TEXT("physics budget < frame"), PhysBudget < Frame);

	// Under budget.
	const FCricketBudgetResult Under = Budget.Evaluate(ECricketPerfCategory::Physics, PhysBudget * 0.5);
	TestEqual(TEXT("under"), Under.Status, ECricketBudgetStatus::UnderBudget);

	// Warning (between warn fraction and 100%).
	const FCricketBudgetResult Warn = Budget.Evaluate(ECricketPerfCategory::Physics, PhysBudget * 0.90);
	TestEqual(TEXT("warning"), Warn.Status, ECricketBudgetStatus::Warning);

	// Over budget.
	const FCricketBudgetResult Over = Budget.Evaluate(ECricketPerfCategory::Physics, PhysBudget * 1.20);
	TestEqual(TEXT("over"), Over.Status, ECricketBudgetStatus::OverBudget);
	TestTrue(TEXT("fraction > 1"), Over.FractionUsed > 1.0f);

	// Scaling to 120 fps halves every budget.
	FCricketSimulationBudget Fast;
	Fast.SetFromFrameTarget(1000.0 / 120.0);
	TestTrue(TEXT("120fps physics ~ half of 60fps"),
		FMath::IsNearlyEqual(Fast.GetBudget(ECricketPerfCategory::Physics), PhysBudget * 0.5, 0.01));

	return true;
}

// 3. MEMORY LEDGER — set/add/total and per-category readout.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPerfMemoryTest,
	"CricketSim.Perf.Memory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPerfMemoryTest::RunTest(const FString&)
{
	FCricketMemoryLedger Ledger;
	Ledger.Set(ECricketPerfCategory::Replay, 4 * 1024 * 1024);
	Ledger.Add(ECricketPerfCategory::Replay, 1024 * 1024);
	Ledger.Set(ECricketPerfCategory::AI, 512 * 1024);

	TestEqual(TEXT("replay bytes"), Ledger.Get(ECricketPerfCategory::Replay), (int64)(5 * 1024 * 1024));
	TestEqual(TEXT("total"), Ledger.TotalTracked(), (int64)(5 * 1024 * 1024 + 512 * 1024));

	// Subtract cannot go negative.
	Ledger.Add(ECricketPerfCategory::AI, -10 * 1024 * 1024);
	TestEqual(TEXT("clamped to zero"), Ledger.Get(ECricketPerfCategory::AI), (int64)0);

	const FCricketMemoryEntry E = Ledger.Entry(ECricketPerfCategory::Replay);
	TestTrue(TEXT("MB conversion"), FMath::IsNearlyEqual(E.MB(), 5.0f, 0.001f));

	return true;
}

// 4. PROFILER — scope accumulation, call counts, and frame reset.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPerfProfilerTest,
	"CricketSim.Perf.Profiler", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPerfProfilerTest::RunTest(const FString&)
{
	FCricketProfiler& P = FCricketProfiler::Get();
	const bool bWasEnabled = P.IsEnabled();
	P.SetEnabled(true);
	P.ResetFrame();

	// Emit a few timed scopes plus an explicit add.
	for (int32 i = 0; i < 3; ++i)
	{
		CRICKET_PERF_SCOPE(AI);
		volatile double Busy = 0.0;
		for (int32 k = 0; k < 5000; ++k) { Busy += FMath::Sqrt((double)k); }
	}
	P.AddTime(ECricketPerfCategory::Physics, 0.002); // 2 ms

	const FCricketFrameTimings T = P.CaptureAndReset();
	TestEqual(TEXT("AI scopes counted"), T.GetCalls(ECricketPerfCategory::AI), 3);
	TestTrue(TEXT("AI time >= 0"), T.GetMs(ECricketPerfCategory::AI) >= 0.0);
	TestTrue(TEXT("physics time ~2ms"), FMath::IsNearlyEqual(T.GetMs(ECricketPerfCategory::Physics), 2.0, 0.001));

	// CaptureAndReset cleared the accumulators.
	const FCricketFrameTimings T2 = P.CaptureAndReset();
	TestEqual(TEXT("reset clears calls"), T2.GetCalls(ECricketPerfCategory::AI), 0);
	TestEqual(TEXT("reset clears time"), T2.GetMs(ECricketPerfCategory::Physics), 0.0);

	// When disabled, AddTime is ignored.
	P.SetEnabled(false);
	P.AddTime(ECricketPerfCategory::Physics, 0.01);
	const FCricketFrameTimings T3 = P.CaptureAndReset();
	TestEqual(TEXT("disabled ignores adds"), T3.GetMs(ECricketPerfCategory::Physics), 0.0);

	P.SetEnabled(bWasEnabled);
	return true;
}

namespace
{
	// Build a dense, mostly-redundant clip with a moving ball and static fielders.
	FCricketReplayClip MakeTestClip(int32 NumFrames, int32 NumActors)
	{
		FCricketReplayClip Clip;
		Clip.MaxFrames = NumFrames + 1;
		const double Dt = 1.0 / 60.0;
		for (int32 i = 0; i < NumFrames; ++i)
		{
			FCricketReplayFrame F;
			F.Time = i * Dt;
			const double U = static_cast<double>(i) / NumFrames;
			F.Ball.PositionM = FVector(18.0 * U, 0.0, 2.0 - 1.6 * FMath::Abs(FMath::Sin(U * 6.28)));
			F.Ball.bInFlight = true;
			for (int32 a = 0; a < NumActors; ++a)
			{
				FCricketActorSnapshot S; S.ActorId = a;
				S.LocationCm = FVector(1000.0 * a, 0.0, 0.0); // perfectly static
				F.Actors.Add(S);
			}
			Clip.Frames.Add(MoveTemp(F));
		}
		FCricketReplayEvent Ev; Ev.Type = ECricketReplayEventType::Bounce; Ev.Time = (NumFrames / 2) * Dt;
		Clip.Events.Add(Ev);
		return Clip;
	}
}

// 5. REPLAY OPTIMIZER — shrinks the clip, preserves events, bounds spatial error.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPerfReplayOptimizerTest,
	"CricketSim.Perf.ReplayOptimizer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPerfReplayOptimizerTest::RunTest(const FString&)
{
	const FCricketReplayClip In = MakeTestClip(3000, 11);
	const int64 OrigBytes = FCricketReplayOptimizer::EstimateClipBytes(In);
	TestTrue(TEXT("original has bytes"), OrigBytes > 0);

	FCricketReplayOptimizerSettings Settings; // defaults: 5mm quantize, 2cm motion
	FCricketReplayClip Out;
	const FCricketReplayOptimizationReport R = FCricketReplayOptimizer::Optimize(In, Settings, Out);

	TestEqual(TEXT("original frames"), R.OriginalFrames, 3000);
	TestTrue(TEXT("fewer frames after"), R.OptimizedFrames < R.OriginalFrames);
	TestTrue(TEXT("fewer frames present"), Out.Frames.Num() == R.OptimizedFrames);
	TestTrue(TEXT("first/last kept"), Out.Frames.Num() >= 2);
	TestTrue(TEXT("compressed"), R.CompressionRatio < 1.0f);
	TestTrue(TEXT("bounded error (<= 5cm)"), R.MaxPositionErrorM <= 0.05f);
	TestEqual(TEXT("events preserved"), Out.Events.Num(), In.Events.Num());

	// Endpoints are exact (not dropped, not shifted in time).
	TestEqual(TEXT("start time"), Out.Frames[0].Time, In.Frames[0].Time);
	TestEqual(TEXT("end time"), Out.Frames.Last().Time, In.Frames.Last().Time);

	// Disabling both lossy steps keeps every frame.
	FCricketReplayOptimizerSettings Lossless; Lossless.PositionQuantizeMm = 0; Lossless.MinMotionM = 0.0f;
	FCricketReplayClip Out2;
	const FCricketReplayOptimizationReport R2 = FCricketReplayOptimizer::Optimize(In, Lossless, Out2);
	TestEqual(TEXT("lossless keeps all frames"), R2.OptimizedFrames, R2.OriginalFrames);
	TestTrue(TEXT("lossless zero error"), R2.MaxPositionErrorM < KINDA_SMALL_NUMBER);

	return true;
}

// 6. BENCHMARK HARNESS — the real stress tests run and pass their verdicts.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPerfBenchmarkTest,
	"CricketSim.Perf.Benchmarks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPerfBenchmarkTest::RunTest(const FString&)
{
	// Replay stress: deterministic, must compress and stay within tolerance.
	const FCricketBenchmarkResult Replay = FCricketPerformanceBenchmark::RunReplayStress(6000, 11);
	AddInfo(Replay.Notes);
	TestTrue(TEXT("replay stress passed"), Replay.bPassed);
	TestTrue(TEXT("replay processed frames"), Replay.ItemsProcessed == 6000);

	// AI-vs-AI: a full T20 completes and bowls a believable number of balls.
	const FCricketBenchmarkResult Match = FCricketPerformanceBenchmark::RunAIvsAIMatch(1337);
	AddInfo(Match.Notes);
	TestTrue(TEXT("match completed"), Match.bPassed);
	TestTrue(TEXT("balls in T20 range"), Match.ItemsProcessed > 100 && Match.ItemsProcessed < 320);

	// Long-match stress: a handful of matches, all complete.
	const FCricketBenchmarkResult Long = FCricketPerformanceBenchmark::RunLongMatchStress(4, 100);
	AddInfo(Long.Notes);
	TestTrue(TEXT("long-match stress passed"), Long.bPassed);
	TestTrue(TEXT("aggregate balls"), Long.ItemsProcessed > 400);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
