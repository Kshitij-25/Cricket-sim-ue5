#include "CricketSimulationBatch.h"
#include "CricketTeamDataAsset.h"   // FCricketPlayer, ECricketRole

namespace
{
	FCricketAIPlayerProfile BatchPlayer(const FString& Name, ECricketRole Role, float Bat, float Bowl, float Pace)
	{
		FCricketPlayer P; P.Name = Name; P.Role = Role; P.Batting = Bat; P.Bowling = Bowl; P.Fielding = 0.6f; P.PaceKmh = Pace;
		return FCricketAIPlayerProfile::Derive(P);
	}
}

FCricketAITeam FCricketSimulationBatch::StandardTeam(const FString& Code, ECricketAIDifficulty Difficulty)
{
	TArray<FCricketAIPlayerProfile> Profs = {
		BatchPlayer(Code + TEXT("_Open1"), ECricketRole::BatterTop,    0.82f, 0.05f, 0.f),
		BatchPlayer(Code + TEXT("_Open2"), ECricketRole::BatterTop,    0.78f, 0.05f, 0.f),
		BatchPlayer(Code + TEXT("_No3"),   ECricketRole::BatterMiddle, 0.80f, 0.05f, 0.f),
		BatchPlayer(Code + TEXT("_No4"),   ECricketRole::BatterMiddle, 0.74f, 0.35f, 118.f), // part-timer
		BatchPlayer(Code + TEXT("_AllR"),  ECricketRole::AllRounder,   0.66f, 0.62f, 128.f),
		BatchPlayer(Code + TEXT("_Keep"),  ECricketRole::WicketKeeper, 0.62f, 0.0f,  0.f),
		BatchPlayer(Code + TEXT("_No7"),   ECricketRole::BatterMiddle, 0.55f, 0.10f, 0.f),
		BatchPlayer(Code + TEXT("_Spin"),  ECricketRole::SpinBowler,   0.28f, 0.74f, 0.f),
		BatchPlayer(Code + TEXT("_Pace1"), ECricketRole::PaceBowler,   0.22f, 0.82f, 142.f),
		BatchPlayer(Code + TEXT("_Pace2"), ECricketRole::PaceBowler,   0.20f, 0.78f, 138.f),
		BatchPlayer(Code + TEXT("_Pace3"), ECricketRole::PaceBowler,   0.18f, 0.72f, 134.f),
	};
	FCricketSquad S; S.TeamName = Code; S.ShortCode = Code.Left(3).ToUpper();
	for (const FCricketAIPlayerProfile& P : Profs) { S.PlayerNames.Add(P.Name); }
	FCricketTeamStrategy Strat; Strat.Difficulty = Difficulty;
	return FCricketAITeam::FromProfiles(S, Profs, Strat);
}

FCricketBatchCell FCricketSimulationBatch::RunCell(
	const FString& Label,
	const FCricketAITeam& TeamA,
	const FCricketAITeam& TeamB,
	const FCricketMatchRules& Rules,
	const FCricketBalanceConfig& Balance,
	int32 NumMatches,
	int32 SeedBase)
{
	FCricketBatchCell Cell;
	Cell.Label = Label;
	Cell.Balance = Balance;
	Cell.Matches.Reserve(NumMatches);

	for (int32 i = 0; i < NumMatches; ++i)
	{
		// Distinct, well-spread seed per match; alternate who bats first to debias.
		const int32 Seed = SeedBase + i * 7919;          // 7919 is prime -> good spread
		const bool bABatsFirst = (i % 2 == 0);
		Cell.Matches.Add(FCricketAIMatchSimulator::Simulate(TeamA, TeamB, Rules, Seed, bABatsFirst, Balance));
	}

	Cell.Metrics = FCricketAggregateMetrics::FromMatches(Cell.Matches);
	return Cell;
}

TArray<FCricketBatchCell> FCricketSimulationBatch::SweepConditions(
	ECricketAIDifficulty Difficulty,
	const TArray<FCricketBalanceConfig>& Conditions,
	int32 NumMatchesPerCondition,
	int32 SeedBase)
{
	const FCricketAITeam A = StandardTeam(TEXT("Alpha"), Difficulty);
	const FCricketAITeam B = StandardTeam(TEXT("Bravo"), Difficulty);
	const FCricketMatchRules Rules = StandardRules();

	TArray<FCricketBatchCell> Cells;
	int32 Cond = 0;
	for (const FCricketBalanceConfig& Cfg : Conditions)
	{
		// Each condition gets its own seed band so populations don't overlap.
		Cells.Add(RunCell(Cfg.PresetName, A, B, Rules, Cfg, NumMatchesPerCondition, SeedBase + Cond * 100000));
		++Cond;
	}
	return Cells;
}

TArray<FCricketBatchCell> FCricketSimulationBatch::SweepDifficulties(int32 NumMatchesPerTier, int32 SeedBase)
{
	const ECricketAIDifficulty Tiers[] = {
		ECricketAIDifficulty::Easy, ECricketAIDifficulty::Medium,
		ECricketAIDifficulty::Hard, ECricketAIDifficulty::Simulation
	};
	const TCHAR* Names[] = { TEXT("Easy"), TEXT("Medium"), TEXT("Hard"), TEXT("Simulation") };

	const FCricketMatchRules Rules = StandardRules();
	const FCricketBalanceConfig Neutral = FCricketBalanceConfig::Neutral();

	TArray<FCricketBatchCell> Cells;
	for (int32 t = 0; t < 4; ++t)
	{
		const FCricketAITeam A = StandardTeam(TEXT("Alpha"), Tiers[t]);
		const FCricketAITeam B = StandardTeam(TEXT("Bravo"), Tiers[t]);
		Cells.Add(RunCell(Names[t], A, B, Rules, Neutral, NumMatchesPerTier, SeedBase + t * 100000));
	}
	return Cells;
}
