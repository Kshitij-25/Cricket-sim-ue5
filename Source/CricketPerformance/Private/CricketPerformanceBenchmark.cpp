#include "CricketPerformanceBenchmark.h"
#include "CricketAIMatchSimulator.h"
#include "CricketAIPlayerProfile.h"
#include "CricketAITypes.h"
#include "CricketTeamDataAsset.h"
#include "CricketScoringTypes.h"
#include "CricketMatchTypes.h"
#include "CricketReplayTypes.h"
#include "HAL/PlatformTime.h"

namespace
{
	FCricketAIPlayerProfile MakePlayer(const FString& Name, ECricketRole Role,
		float Bat, float Bowl, float Pace)
	{
		FCricketPlayer P;
		P.Name = Name; P.Role = Role; P.Batting = Bat; P.Bowling = Bowl;
		P.Fielding = 0.6f; P.PaceKmh = Pace;
		return FCricketAIPlayerProfile::Derive(P);
	}

	// A balanced XI (matches the AI test fixture): five who can bowl 20 overs.
	FCricketAITeam BuildTeam(const FString& Code, ECricketAIDifficulty Diff)
	{
		TArray<FCricketAIPlayerProfile> Profs = {
			MakePlayer(Code + TEXT("_Open1"), ECricketRole::BatterTop,    0.82f, 0.05f, 0.f),
			MakePlayer(Code + TEXT("_Open2"), ECricketRole::BatterTop,    0.78f, 0.05f, 0.f),
			MakePlayer(Code + TEXT("_No3"),   ECricketRole::BatterMiddle, 0.80f, 0.05f, 0.f),
			MakePlayer(Code + TEXT("_No4"),   ECricketRole::BatterMiddle, 0.74f, 0.35f, 118.f),
			MakePlayer(Code + TEXT("_AllR"),  ECricketRole::AllRounder,   0.66f, 0.62f, 128.f),
			MakePlayer(Code + TEXT("_Keep"),  ECricketRole::WicketKeeper, 0.62f, 0.0f,  0.f),
			MakePlayer(Code + TEXT("_No7"),   ECricketRole::BatterMiddle, 0.55f, 0.10f, 0.f),
			MakePlayer(Code + TEXT("_Spin"),  ECricketRole::SpinBowler,   0.28f, 0.74f, 0.f),
			MakePlayer(Code + TEXT("_Pace1"), ECricketRole::PaceBowler,   0.22f, 0.82f, 142.f),
			MakePlayer(Code + TEXT("_Pace2"), ECricketRole::PaceBowler,   0.20f, 0.78f, 138.f),
			MakePlayer(Code + TEXT("_Pace3"), ECricketRole::PaceBowler,   0.18f, 0.72f, 134.f),
		};
		FCricketSquad S; S.TeamName = Code; S.ShortCode = Code.Left(3).ToUpper();
		for (const FCricketAIPlayerProfile& P : Profs) { S.PlayerNames.Add(P.Name); }
		FCricketTeamStrategy Strat; Strat.Difficulty = Diff;
		return FCricketAITeam::FromProfiles(S, Profs, Strat);
	}

	int32 TotalBalls(const FCricketAIMatchTelemetry& M)
	{
		int32 Balls = 0;
		for (const FCricketAIInningsTelemetry& I : M.Innings) { Balls += I.LegalBalls + I.Extras; }
		return FMath::Max(Balls, M.TotalBalls);
	}
}

FCricketBenchmarkResult FCricketPerformanceBenchmark::RunAIvsAIMatch(int32 Seed)
{
	const FCricketAITeam A = BuildTeam(TEXT("Alpha"), ECricketAIDifficulty::Hard);
	const FCricketAITeam B = BuildTeam(TEXT("Bravo"), ECricketAIDifficulty::Hard);
	FCricketMatchRules Rules; // T20 defaults

	const double Start = FPlatformTime::Seconds();
	const FCricketAIMatchTelemetry M = FCricketAIMatchSimulator::Simulate(A, B, Rules, Seed);
	const double Elapsed = FPlatformTime::Seconds() - Start;

	FCricketBenchmarkResult R;
	R.Name = TEXT("AI vs AI (single T20)");
	R.Iterations = 1;
	R.TotalMs = Elapsed * 1000.0;
	R.MsPerIteration = R.TotalMs;
	R.ItemsProcessed = TotalBalls(M);
	R.ItemsPerSecond = Elapsed > 0.0 ? R.ItemsProcessed / Elapsed : 0.0;
	R.bPassed = M.bCompleted && R.ItemsProcessed > 0;
	R.Notes = FString::Printf(TEXT("%s | %lld balls in %.2f ms"),
		*M.ResultSummary, (long long)R.ItemsProcessed, R.TotalMs);
	return R;
}

FCricketBenchmarkResult FCricketPerformanceBenchmark::RunLongMatchStress(int32 NumMatches, int32 Seed)
{
	NumMatches = FMath::Max(NumMatches, 1);
	const FCricketAITeam A = BuildTeam(TEXT("Alpha"), ECricketAIDifficulty::Hard);
	const FCricketAITeam B = BuildTeam(TEXT("Bravo"), ECricketAIDifficulty::Simulation);
	FCricketMatchRules Rules;

	int64 Balls = 0;
	bool bAllCompleted = true;
	const double Start = FPlatformTime::Seconds();
	for (int32 i = 0; i < NumMatches; ++i)
	{
		const FCricketAIMatchTelemetry M = FCricketAIMatchSimulator::Simulate(A, B, Rules, Seed + i);
		Balls += TotalBalls(M);
		bAllCompleted &= M.bCompleted;
	}
	const double Elapsed = FPlatformTime::Seconds() - Start;

	FCricketBenchmarkResult R;
	R.Name = TEXT("Long-match stress (N x T20)");
	R.Iterations = NumMatches;
	R.TotalMs = Elapsed * 1000.0;
	R.MsPerIteration = R.TotalMs / NumMatches;
	R.ItemsProcessed = Balls;
	R.ItemsPerSecond = Elapsed > 0.0 ? Balls / Elapsed : 0.0;
	R.bPassed = bAllCompleted && Balls > 0;
	R.Notes = FString::Printf(TEXT("%d matches, %lld balls, %.2f ms/match"),
		NumMatches, (long long)Balls, R.MsPerIteration);
	return R;
}

FCricketBenchmarkResult FCricketPerformanceBenchmark::RunReplayStress(int32 NumFrames, int32 NumActors)
{
	NumFrames = FMath::Max(NumFrames, 2);
	NumActors = FMath::Clamp(NumActors, 0, 64);

	// Synthesize a dense clip: a ball on a representative arc, plus mostly-static
	// fielders that drift slightly — the redundancy the optimizer should reclaim.
	FCricketReplayClip Clip;
	Clip.MaxFrames = NumFrames + 1;
	const double Dt = 1.0 / 60.0;
	for (int32 i = 0; i < NumFrames; ++i)
	{
		const double T = i * Dt;
		FCricketReplayFrame F;
		F.Time = T;
		// Ball: 24 m downrange in ~1.2 s with a parabolic bounce-ish height.
		const double U = static_cast<double>(i) / NumFrames;
		F.Ball.PositionM = FVector(20.0 * U, 0.05 * FMath::Sin(U * 12.0), 2.2 - 1.9 * FMath::Abs(FMath::Sin(U * 6.28)));
		F.Ball.VelocityMS = FVector(38.0, 0.0, 0.0);
		F.Ball.bInFlight = true;
		for (int32 a = 0; a < NumActors; ++a)
		{
			FCricketActorSnapshot Snap;
			Snap.ActorId = a;
			// Fielders nudge < 1 cm/frame — almost all of these frames are redundant.
			Snap.LocationCm = FVector(1000.0 * a + 0.4 * i, 500.0 * a, 0.0);
			F.Actors.Add(Snap);
		}
		Clip.Frames.Add(MoveTemp(F));
	}
	// A couple of events the optimizer must preserve.
	FCricketReplayEvent Bounce; Bounce.Type = ECricketReplayEventType::Bounce;
	Bounce.Time = (NumFrames / 3) * Dt; Clip.Events.Add(Bounce);
	FCricketReplayEvent Impact; Impact.Type = ECricketReplayEventType::BatImpact;
	Impact.Time = (NumFrames / 2) * Dt; Clip.Events.Add(Impact);

	FCricketReplayOptimizerSettings Settings; // defaults
	FCricketReplayClip Out;

	const double Start = FPlatformTime::Seconds();
	const FCricketReplayOptimizationReport Rep = FCricketReplayOptimizer::Optimize(Clip, Settings, Out);
	const double Elapsed = FPlatformTime::Seconds() - Start;

	FCricketBenchmarkResult R;
	R.Name = TEXT("Replay optimization stress");
	R.Iterations = 1;
	R.TotalMs = Elapsed * 1000.0;
	R.MsPerIteration = R.TotalMs;
	R.ItemsProcessed = NumFrames;
	R.ItemsPerSecond = Elapsed > 0.0 ? NumFrames / Elapsed : 0.0;
	R.PeakBytes = Rep.OriginalBytes;
	// Pass if we saved space and stayed within a generous spatial tolerance (5 cm).
	R.bPassed = (Rep.CompressionRatio < 1.0f) && (Rep.MaxPositionErrorM < 0.05f);
	R.Notes = FString::Printf(
		TEXT("%d→%d frames, %.1f%% of original, saved %.2f MB, max err %.1f mm"),
		Rep.OriginalFrames, Rep.OptimizedFrames, Rep.CompressionRatio * 100.0f,
		Rep.SavedMB, Rep.MaxPositionErrorM * 1000.0f);
	return R;
}

TArray<FCricketBenchmarkResult> FCricketPerformanceBenchmark::RunAll()
{
	TArray<FCricketBenchmarkResult> Results;
	Results.Add(RunAIvsAIMatch());
	Results.Add(RunLongMatchStress());
	Results.Add(RunReplayStress());
	return Results;
}
