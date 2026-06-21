#include "CricketAnimDebugComponent.h"
#include "CricketCharacterAnimComponent.h"
#include "CricketBattingComponent.h"
#include "CricketFielderComponent.h"
#include "CricketAnimationModel.h"
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
	DrawFieldingReadout();
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

		// The contact window: when the swing CAN register a hit, made an explicit,
		// inspectable construct rather than an implicit consequence of geometry.
		if (const UCricketBattingComponent* B = Anim->GetBatting())
		{
			const bool bOpen = B->IsContactWindowOpen();
			Line(5, bOpen ? FColor::Yellow : FColor(120, 120, 120),
				FString::Printf(TEXT("Contact window: %s  [%.3f .. %.3f]s  clock=%.3fs"),
					bOpen ? TEXT("OPEN") : TEXT("closed"),
					B->GetContactWindowOpenSec(), B->GetContactWindowCloseSec(), B->GetSwingClockSec()));
		}
	}

	// Notify timing: the recent notifies and when they fired; the newest flashes.
	// Physics-handoff notifies (BallRelease/BatImpact/CatchAttempt/PickupContact/
	// ThrowRelease) are starred so they read as distinct from cosmetic ones
	// (FootPlant) at a glance.
	const TArray<FCricketAnimNotifyLog>& Logs = Anim->GetRecentNotifies();
	const float Now = GetWorld()->GetTimeSeconds();
	FString NotifyLine = TEXT("Notifies: ");
	bool bLastWasHandoff = false;
	for (int32 i = FMath::Max(0, Logs.Num() - 5); i < Logs.Num(); ++i)
	{
		const bool bHandoff = FCricketAnimationModel::IsPhysicsHandoffNotify(Logs[i].Type);
		NotifyLine += FString::Printf(TEXT("[%s%s @%.2f] "), bHandoff ? TEXT("*") : TEXT(""), NotifyName(Logs[i].Type), Logs[i].WorldTime);
		if (i == Logs.Num() - 1) { bLastWasHandoff = bHandoff; }
	}
	const bool bFlash = Logs.Num() > 0 && (Now - Logs.Last().WorldTime) < 0.3f;
	const FColor NotifyColor = bFlash ? (bLastWasHandoff ? FColor::Yellow : FColor::Cyan) : FColor(150, 150, 150);
	Line(3, NotifyColor, NotifyLine);
}

void UCricketAnimDebugComponent::DrawFieldingReadout() const
{
	const UCricketFielderComponent* F = Anim->GetFielder();
	if (!F || !GEngine) { return; }
	if (Anim->GetFieldingState() == ECricketFieldingAnimState::Idle) { return; }

	auto Line = [&](int32 K, const FColor& C, const FString& T) { GEngine->AddOnScreenDebugMessage(MessageKeyBase + K, 0.f, C, T); };

	const FString FieldStr = StaticEnum<ECricketFieldingAnimState>()->GetDisplayNameTextByValue((int64)Anim->GetFieldingState()).ToString();
	Line(4, FColor::Magenta, FString::Printf(TEXT("Fielding: %s  state-time %.2fs  holding=%s"),
		*FieldStr, F->GetStateTimeSec(), F->HasBall() ? TEXT("yes") : TEXT("no")));

	// The throw windup: an explicit window between entering Throwing and the
	// ThrowRelease handoff (ball actually leaving the hand), mirroring the
	// run-up -> release pattern bowling already has.
	if (F->GetState() == ECricketFielderState::Throwing)
	{
		Line(6, FColor::Orange, FString::Printf(TEXT("Throw windup: %.2fs / %.2fs"), F->GetStateTimeSec(), F->ThrowWindupSec));
	}
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
