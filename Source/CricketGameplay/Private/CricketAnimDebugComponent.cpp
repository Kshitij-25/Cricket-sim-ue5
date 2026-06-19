#include "CricketAnimDebugComponent.h"
#include "CricketCharacterAnimComponent.h"
#include "CricketBattingComponent.h"
#include "CricketPhysicsConstants.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

namespace
{
	TAutoConsoleVariable<int32> CVarAnimDebug(TEXT("cricket.Debug.Anim"), 0,
		TEXT("Animation state/notify debug visualization. 0=off, 1=on"));

	const TCHAR* NotifyName(ECricketAnimNotify N)
	{
		switch (N)
		{
		case ECricketAnimNotify::BallRelease:   return TEXT("BallRelease");
		case ECricketAnimNotify::BatImpact:     return TEXT("BatImpact");
		case ECricketAnimNotify::CatchAttempt:  return TEXT("CatchAttempt");
		case ECricketAnimNotify::ThrowRelease:  return TEXT("ThrowRelease");
		case ECricketAnimNotify::PickupContact: return TEXT("PickupContact");
		default:                                return TEXT("FootPlant");
		}
	}
}

UCricketAnimDebugComponent::UCricketAnimDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCricketAnimDebugComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Anim = Owner->FindComponentByClass<UCricketCharacterAnimComponent>();
	}
}

void UCricketAnimDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Anim || !GetWorld()) { return; }
	if (CVarAnimDebug.GetValueOnGameThread() == 0) { return; }

	DrawStateLabel();
	DrawReadout();
	DrawBatPath();
}

void UCricketAnimDebugComponent::DrawStateLabel() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) { return; }

	// The dominant action state above the character (state transitions read live).
	FString Label;
	if (Anim->IsBowlingActionPlaying())
	{
		Label = StaticEnum<ECricketBowlingAnimState>()->GetDisplayNameTextByValue((int64)Anim->GetBowlingState()).ToString();
	}
	else if (Anim->GetBattingState() != ECricketBattingAnimState::Guard)
	{
		Label = StaticEnum<ECricketBattingAnimState>()->GetDisplayNameTextByValue((int64)Anim->GetBattingState()).ToString();
	}
	else if (Anim->GetFieldingState() != ECricketFieldingAnimState::Idle)
	{
		Label = StaticEnum<ECricketFieldingAnimState>()->GetDisplayNameTextByValue((int64)Anim->GetFieldingState()).ToString();
	}
	else
	{
		Label = StaticEnum<ECricketLocomotionState>()->GetDisplayNameTextByValue((int64)Anim->GetLocomotionState()).ToString();
	}
	DrawDebugString(GetWorld(), Owner->GetActorLocation() + FVector(0, 0, 240), Label, nullptr, FColor::Cyan, 0.f, true, 1.3f);
}

void UCricketAnimDebugComponent::DrawReadout() const
{
	if (!GEngine) { return; }
	auto Line = [&](int32 K, const FColor& C, const FString& T) { GEngine->AddOnScreenDebugMessage(MessageKeyBase + K, 0.f, C, T); };

	const FCricketLocomotionSample L = Anim->GetLocomotion();
	const FString LocoStr = StaticEnum<ECricketLocomotionState>()->GetDisplayNameTextByValue((int64)L.State).ToString();
	Line(0, FColor::White, FString::Printf(TEXT("Loco: %s  speed %.2f m/s  gait %.2f"), *LocoStr, L.SpeedMS, L.GaitBlend));

	if (Anim->IsBowlingActionPlaying())
	{
		const FString BowlStr = StaticEnum<ECricketBowlingAnimState>()->GetDisplayNameTextByValue((int64)Anim->GetBowlingState()).ToString();
		Line(1, FColor::Orange, FString::Printf(TEXT("Bowling: %s  t=%.2fs  release@%.2fs  wrist(%.1f,%.1f,%.1f)"),
			*BowlStr, Anim->GetBowlActionTimeSec(), Anim->GetReleaseTimeSec(),
			Anim->GetWristAxis().X, Anim->GetWristAxis().Y, Anim->GetWristAxis().Z));
	}
	if (Anim->GetBattingState() != ECricketBattingAnimState::Guard)
	{
		const FString BatStr = StaticEnum<ECricketBattingAnimState>()->GetDisplayNameTextByValue((int64)Anim->GetBattingState()).ToString();
		Line(2, FColor::Green, FString::Printf(TEXT("Batting: %s  bat speed %.1f m/s"), *BatStr, Anim->GetBatSpeedMS()));
	}

	// Notify timing: the recent notifies and when they fired; the newest flashes.
	const TArray<FCricketAnimNotifyLog>& Logs = Anim->GetRecentNotifies();
	const float Now = GetWorld()->GetTimeSeconds();
	FString NotifyLine = TEXT("Notifies: ");
	for (int32 i = FMath::Max(0, Logs.Num() - 5); i < Logs.Num(); ++i)
	{
		NotifyLine += FString::Printf(TEXT("[%s @%.2f] "), NotifyName(Logs[i].Type), Logs[i].WorldTime);
	}
	const bool bFlash = Logs.Num() > 0 && (Now - Logs.Last().WorldTime) < 0.3f;
	Line(3, bFlash ? FColor::Yellow : FColor(150, 150, 150), NotifyLine);
}

void UCricketAnimDebugComponent::DrawBatPath() const
{
	const UCricketBattingComponent* B = Anim->GetBatting();
	if (!B) { return; }
	const TArray<FVector>& Trail = B->GetSwingTrailCm();
	for (int32 i = 1; i < Trail.Num(); ++i)
	{
		DrawDebugLine(GetWorld(), Trail[i - 1], Trail[i], FColor::Emerald, false, -1.f, 0, 2.f);
	}
}
