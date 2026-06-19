#include "CricketFieldingAICoordinator.h"
#include "CricketFielderComponent.h"
#include "CricketPerfProfiler.h"

UCricketFieldingAICoordinator::UCricketFieldingAICoordinator()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCricketFieldingAICoordinator::SetFielders(const TArray<UCricketFielderComponent*>& InFielders)
{
	Fielders.Reset();
	for (UCricketFielderComponent* F : InFielders) { if (F) { Fielders.Add(F); } }
}

void UCricketFieldingAICoordinator::AddFielder(UCricketFielderComponent* InFielder)
{
	if (InFielder) { Fielders.Add(InFielder); }
}

void UCricketFieldingAICoordinator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Profiled: fielding AI coordination (captain/fielding brain dispatch).
	CRICKET_PERF_SCOPE(AI);

	Coordinate();
}

void UCricketFieldingAICoordinator::Coordinate()
{
	if (Fielders.Num() == 0) { return; }

	// Gather each fielder's physics-true intercept of the live ball.
	TArray<FCricketInterceptResult> Intercepts;
	TArray<int32> ValidIndex;                       // map dense intercept list -> fielder slot
	Intercepts.Reserve(Fielders.Num());
	for (int32 i = 0; i < Fielders.Num(); ++i)
	{
		UCricketFielderComponent* F = Fielders[i].Get();
		if (!F) { continue; }
		FCricketBallPrediction Pred;
		Intercepts.Add(F->EvaluateIntercept(Pred));
		ValidIndex.Add(i);
	}
	if (Intercepts.Num() == 0) { return; }

	const FCricketAIDifficultyParams D = FCricketAIDifficultyParams::Resolve(Difficulty);
	FRandomStream Rng(Seed * 2654435761u + GFrameCounter);
	Plan = FCricketFieldingBrain::SelectChaser(Intercepts, D, Rng);

	// Designate the chaser; everyone else holds station.
	const int32 ChosenSlot = Intercepts.IsValidIndex(Plan.ChaserIndex) ? ValidIndex[Plan.ChaserIndex] : INDEX_NONE;
	for (int32 i = 0; i < Fielders.Num(); ++i)
	{
		if (UCricketFielderComponent* F = Fielders[i].Get())
		{
			F->SetActiveChaser(i == ChosenSlot);
		}
	}
	// Remap the plan indices back to fielder slots for the debug overlay.
	if (Intercepts.IsValidIndex(Plan.ChaserIndex)) { Plan.ChaserIndex = ChosenSlot; }
	if (Intercepts.IsValidIndex(Plan.BackupIndex)) { Plan.BackupIndex = ValidIndex[Plan.BackupIndex]; }
}
