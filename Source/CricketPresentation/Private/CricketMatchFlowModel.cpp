#include "CricketMatchFlowModel.h"

FCricketBroadcastSequence FCricketMatchFlowModel::BuildMatchIntro(const FString& TeamA, const FString& TeamB, int32 OversPerInnings)
{
	FCricketBroadcastSequence Seq;
	Seq.Segment = ECricketBroadcastSegment::MatchIntro;
	Seq.Steps.Add(FCricketBroadcastStep(
		FString::Printf(TEXT("%s vs %s"), *TeamA.ToUpper(), *TeamB.ToUpper()),
		ECricketBroadcastCamera::MainBroadcast, 4.0f));
	Seq.Steps.Add(FCricketBroadcastStep(
		FString::Printf(TEXT("%d Overs • Live"), FMath::Max(1, OversPerInnings)),
		ECricketBroadcastCamera::MainBroadcast, 3.0f));
	Seq.Begin();
	return Seq;
}

FCricketBroadcastSequence FCricketMatchFlowModel::BuildTeamIntro(const FString& TeamName, const TArray<FString>& PlayerNames)
{
	FCricketBroadcastSequence Seq;
	Seq.Segment = ECricketBroadcastSegment::TeamIntro;
	Seq.Steps.Add(FCricketBroadcastStep(
		FString::Printf(TEXT("%s — Playing XI"), *TeamName.ToUpper()),
		ECricketBroadcastCamera::Batting, 3.0f));

	// Flash the order three names at a time so the bumper stays brief.
	FString Line;
	int32 OnLine = 0;
	for (int32 i = 0; i < PlayerNames.Num(); ++i)
	{
		Line += (OnLine == 0 ? TEXT("") : TEXT("  •  "));
		Line += FString::Printf(TEXT("%d %s"), i + 1, *PlayerNames[i]);
		if (++OnLine == 3 || i == PlayerNames.Num() - 1)
		{
			Seq.Steps.Add(FCricketBroadcastStep(Line, ECricketBroadcastCamera::Batting, 2.0f));
			Line.Reset();
			OnLine = 0;
		}
	}
	Seq.Begin();
	return Seq;
}

FCricketBroadcastSequence FCricketMatchFlowModel::BuildToss(const FString& TossWinner, bool bChoseToBat)
{
	FCricketBroadcastSequence Seq;
	Seq.Segment = ECricketBroadcastSegment::Toss;
	Seq.Steps.Add(FCricketBroadcastStep(TEXT("THE TOSS"), ECricketBroadcastCamera::MainBroadcast, 2.5f));
	Seq.Steps.Add(FCricketBroadcastStep(
		FString::Printf(TEXT("%s won the toss and chose to %s"), *TossWinner, bChoseToBat ? TEXT("bat") : TEXT("bowl")),
		ECricketBroadcastCamera::MainBroadcast, 3.5f));
	Seq.Begin();
	return Seq;
}

FCricketBroadcastSequence FCricketMatchFlowModel::BuildOverTransition(const FString& OverSummary, const FString& ChaseLine)
{
	FCricketBroadcastSequence Seq;
	Seq.Segment = ECricketBroadcastSegment::OverTransition;
	if (!OverSummary.IsEmpty())
	{
		Seq.Steps.Add(FCricketBroadcastStep(OverSummary, ECricketBroadcastCamera::MainBroadcast, 2.5f));
	}
	if (!ChaseLine.IsEmpty())
	{
		Seq.Steps.Add(FCricketBroadcastStep(ChaseLine, ECricketBroadcastCamera::MainBroadcast, 2.0f));
	}
	if (Seq.Steps.Num() == 0)
	{
		// Always show something so the bumper isn't empty.
		Seq.Steps.Add(FCricketBroadcastStep(TEXT("End of the over"), ECricketBroadcastCamera::MainBroadcast, 1.5f));
	}
	Seq.Begin();
	return Seq;
}

FCricketBroadcastSequence FCricketMatchFlowModel::BuildInningsTransition(const FString& BattingTeam, int32 Runs, int32 Wickets, int32 Target)
{
	FCricketBroadcastSequence Seq;
	Seq.Segment = ECricketBroadcastSegment::InningsTransition;
	Seq.Steps.Add(FCricketBroadcastStep(TEXT("INNINGS BREAK"), ECricketBroadcastCamera::MainBroadcast, 3.0f));
	Seq.Steps.Add(FCricketBroadcastStep(
		FString::Printf(TEXT("%s %d/%d"), *BattingTeam.ToUpper(), Runs, Wickets),
		ECricketBroadcastCamera::MainBroadcast, 3.0f));
	if (Target > 0)
	{
		Seq.Steps.Add(FCricketBroadcastStep(
			FString::Printf(TEXT("Target: %d"), Target),
			ECricketBroadcastCamera::MainBroadcast, 3.0f));
	}
	Seq.Begin();
	return Seq;
}

FCricketBroadcastSequence FCricketMatchFlowModel::BuildMatchResult(const FCricketMatchSnapshot& Final)
{
	FCricketBroadcastSequence Seq;
	Seq.Segment = ECricketBroadcastSegment::MatchResult;

	const FString Headline = Final.bTie ? TEXT("MATCH TIED")
		: (!Final.WinningTeam.IsEmpty() ? FString::Printf(TEXT("%s WIN"), *Final.WinningTeam.ToUpper()) : TEXT("MATCH COMPLETE"));

	Seq.Steps.Add(FCricketBroadcastStep(Headline, ECricketBroadcastCamera::MainBroadcast, 4.0f));
	if (!Final.ResultSummary.IsEmpty())
	{
		Seq.Steps.Add(FCricketBroadcastStep(Final.ResultSummary, ECricketBroadcastCamera::MainBroadcast, 4.0f));
	}
	Seq.Steps.Add(FCricketBroadcastStep(TEXT("Thanks for watching"), ECricketBroadcastCamera::MainBroadcast, 3.0f));
	Seq.Begin();
	return Seq;
}
