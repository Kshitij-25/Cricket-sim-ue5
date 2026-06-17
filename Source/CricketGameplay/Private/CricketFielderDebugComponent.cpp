#include "CricketFielderDebugComponent.h"
#include "CricketFielderComponent.h"
#include "CricketPhysicsConstants.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

namespace
{
	TAutoConsoleVariable<int32> CVarFieldingDebug(TEXT("cricket.Debug.Fielding"), 1,
		TEXT("Fielding decision/prediction debug visualization. 0=off, 1=on"));

	FColor KindColor(ECricketInterceptKind Kind)
	{
		switch (Kind)
		{
		case ECricketInterceptKind::Catch:       return FColor::Green;
		case ECricketInterceptKind::GroundField: return FColor::Cyan;
		default:                                  return FColor::Red;
		}
	}

	FColor StateColor(ECricketFielderState S)
	{
		switch (S)
		{
		case ECricketFielderState::Idle:                return FColor(120, 120, 120);
		case ECricketFielderState::Tracking:            return FColor::Yellow;
		case ECricketFielderState::MovingToIntercept:   return FColor::Orange;
		case ECricketFielderState::Catching:            return FColor::Green;
		case ECricketFielderState::PickingUp:           return FColor::Cyan;
		case ECricketFielderState::Throwing:            return FColor::Magenta;
		case ECricketFielderState::ReturningToPosition: return FColor(80, 160, 255);
		default:                                        return FColor::White;
		}
	}
}

UCricketFielderDebugComponent::UCricketFielderDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCricketFielderDebugComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Fielder = Owner->FindComponentByClass<UCricketFielderComponent>();
	}
}

void UCricketFielderDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Fielder || !GetWorld()) { return; }
	if (CVarFieldingDebug.GetValueOnGameThread() == 0) { return; }

	DrawPrediction();
	DrawInterceptAndState();
	DrawThrow();
}

void UCricketFielderDebugComponent::DrawPrediction() const
{
	// Only the active chaser draws the (shared) ball prediction, to avoid clutter.
	if (!Fielder->IsActiveChaser()) { return; }

	const FCricketBallPrediction& Pred = Fielder->GetPrediction();
	if (!Pred.bValid) { return; }

	for (int32 i = 1; i < Pred.Path.Num(); ++i)
	{
		DrawDebugLine(GetWorld(), MetersToWorld(Pred.Path[i - 1].Position), MetersToWorld(Pred.Path[i].Position),
			FColor(160, 160, 160), false, -1.f, 0, 1.2f);
	}
	if (Pred.bWillBounce)
	{
		const FVector LandCm = MetersToWorld(Pred.LandingPointM);
		DrawDebugSphere(GetWorld(), LandCm, 10.f, 12, FColor::Yellow, false, -1.f, 0, 2.f);
		DrawDebugString(GetWorld(), LandCm + FVector(0, 0, 20), TEXT("landing"), nullptr, FColor::Yellow, 0.f, false, 1.1f);
	}
	DrawDebugSphere(GetWorld(), MetersToWorld(Pred.ApexM), 8.f, 10, FColor(200, 120, 255), false, -1.f, 0, 1.5f);
}

void UCricketFielderDebugComponent::DrawInterceptAndState() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) { return; }
	const FVector OwnerCm = Owner->GetActorLocation();

	const FCricketInterceptResult& I = Fielder->GetIntercept();
	if (I.bCanIntercept)
	{
		const FVector PointCm = MetersToWorld(I.PointM);
		DrawDebugSphere(GetWorld(), PointCm, 9.f, 12, KindColor(I.Kind), false, -1.f, 0, 2.f);
		// Interception path: where the fielder is running.
		DrawDebugLine(GetWorld(), OwnerCm, Fielder->GetMoveTargetCm(), KindColor(I.Kind), false, -1.f, 0, 2.f);
	}

	// State label above the fielder.
	const UEnum* StateEnum = StaticEnum<ECricketFielderState>();
	const FString StateStr = StateEnum ? StateEnum->GetDisplayNameTextByValue((int64)Fielder->GetState()).ToString() : TEXT("?");
	DrawDebugString(GetWorld(), OwnerCm + FVector(0, 0, 220), StateStr, nullptr, StateColor(Fielder->GetState()), 0.f, true, 1.2f);

	// Marker for the fielder body (asset-free).
	DrawDebugCapsule(GetWorld(), OwnerCm + FVector(0, 0, 90), 90.f, 30.f, FQuat::Identity,
		Fielder->IsActiveChaser() ? FColor::White : FColor(90, 90, 90), false, -1.f, 0, 1.5f);

	// The active chaser writes the on-screen decision readout.
	if (Fielder->IsActiveChaser())
	{
		const UEnum* KindEnum = StaticEnum<ECricketInterceptKind>();
		const UEnum* DiffEnum = StaticEnum<ECricketCatchDifficulty>();
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(5001, 0.f, StateColor(Fielder->GetState()),
				FString::Printf(TEXT("Chaser: %s  %s"), *StateStr, Fielder->HasBall() ? TEXT("[has ball]") : TEXT("")));
			if (I.bCanIntercept)
			{
				GEngine->AddOnScreenDebugMessage(5002, 0.f, KindColor(I.Kind),
					FString::Printf(TEXT("Intercept: %s / %s  in %.2fs  dist %.1fm  slack %.2fs"),
						KindEnum ? *KindEnum->GetDisplayNameTextByValue((int64)I.Kind).ToString() : TEXT("?"),
						DiffEnum ? *DiffEnum->GetDisplayNameTextByValue((int64)I.Difficulty).ToString() : TEXT("?"),
						I.TimeSec, I.DistanceM, I.SlackSec));
			}
			else
			{
				GEngine->AddOnScreenDebugMessage(5002, 0.f, FColor::Red, TEXT("Intercept: unreachable (boundary)"));
			}
		}
	}
}

void UCricketFielderDebugComponent::DrawThrow() const
{
	const TArray<FVector>& Path = Fielder->GetThrowPathCm();
	for (int32 i = 1; i < Path.Num(); ++i)
	{
		DrawDebugLine(GetWorld(), Path[i - 1], Path[i], FColor::Magenta, false, -1.f, 0, 1.5f);
	}
}
