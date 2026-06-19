#include "CricketCharacterAnimComponent.h"
#include "CricketAnimationModel.h"
#include "CricketBattingComponent.h"
#include "CricketFielderComponent.h"
#include "CricketPhysicsConstants.h"
#include "CricketPerfProfiler.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

UCricketCharacterAnimComponent::UCricketCharacterAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After movement/gameplay has updated this frame, so we visualise the latest sim.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UCricketCharacterAnimComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		// Follow whichever cricket roles this character has.
		Batting = Owner->FindComponentByClass<UCricketBattingComponent>();
		Fielder = Owner->FindComponentByClass<UCricketFielderComponent>();

		if (UCricketBattingComponent* B = Batting.Get())
		{
			B->OnShotPlayed.AddDynamic(this, &UCricketCharacterAnimComponent::HandleShotPlayed);
		}
		if (UCricketFielderComponent* F = Fielder.Get())
		{
			F->OnStateChanged.AddDynamic(this, &UCricketCharacterAnimComponent::HandleFielderState);
			F->OnThrew.AddDynamic(this, &UCricketCharacterAnimComponent::HandleFielderThrew);
		}
		PrevLocationCm = Owner->GetActorLocation();
		PrevYawDeg = Owner->GetActorRotation().Yaw;
		bHasPrev = true;
	}
}

void UCricketCharacterAnimComponent::StartBowlingAction()
{
	BowlPlayer.Start(FCricketAnimationModel::MakeBowlingMontage(BowlingTimeline));
	BowlingState = ECricketBowlingAnimState::RunUp;
}

void UCricketCharacterAnimComponent::EmitNotify(ECricketAnimNotify Type)
{
	FCricketAnimNotifyLog Log;
	Log.Type = Type;
	Log.WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	RecentNotifies.Add(Log);
	if (RecentNotifies.Num() > 8) { RecentNotifies.RemoveAt(0); }
	OnAnimNotify.Broadcast(Type);
}

void UCricketCharacterAnimComponent::UpdateLocomotion(double Dt)
{
	AActor* Owner = GetOwner();
	if (!Owner || Dt <= 1e-6) { return; }

	const FVector Loc = Owner->GetActorLocation();
	const double YawDeg = Owner->GetActorRotation().Yaw;

	if (!bHasPrev) { PrevLocationCm = Loc; PrevYawDeg = YawDeg; bHasPrev = true; }

	const FVector DeltaCm(Loc.X - PrevLocationCm.X, Loc.Y - PrevLocationCm.Y, 0.0);
	const double SpeedMS = (DeltaCm.Size() * UEToMeters) / Dt;
	const double TurnRateDeg = FMath::FindDeltaAngleDegrees(PrevYawDeg, YawDeg) / Dt;
	const double AccelMS2 = (SpeedMS - PrevSpeedMS) / Dt;

	Locomotion = FCricketAnimationModel::ClassifyLocomotion(
		SpeedMS, TurnRateDeg, AccelMS2, Locomotion.State, LocomotionConfig);

	PrevLocationCm = Loc;
	PrevYawDeg = YawDeg;
	PrevSpeedMS = SpeedMS;
}

void UCricketCharacterAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Profiled: animation state machines + character pose update.
	CRICKET_PERF_SCOPE(Animation);

	const double Dt = FMath::Max((double)DeltaTime, 1e-5);

	// 1. Locomotion (derived from how the pawn is actually moving).
	UpdateLocomotion(Dt);

	// 2. Bowling action montage — advance and dispatch its notifies (the release).
	if (BowlPlayer.bPlaying)
	{
		TArray<ECricketAnimNotify> Fired;
		BowlPlayer.Advance(Dt, Fired);
		BowlingState = (ECricketBowlingAnimState)BowlPlayer.CurrentStateId();
		for (ECricketAnimNotify N : Fired) { EmitNotify(N); }
	}
	else if (BowlingState != ECricketBowlingAnimState::Idle && !BowlPlayer.bPlaying)
	{
		BowlingState = ECricketBowlingAnimState::Idle;
	}

	// 3. Batting state — FOLLOW the batting sim (physics is the source of truth).
	if (UCricketBattingComponent* B = Batting.Get())
	{
		BattingState = FCricketAnimationModel::MapBattingPhase(B->GetSwingPhase());
	}

	// 4. Fielding state — FOLLOW the fielder sim's state machine.
	if (UCricketFielderComponent* F = Fielder.Get())
	{
		switch (F->GetState())
		{
		case ECricketFielderState::Idle:                FieldingState = ECricketFieldingAnimState::Idle; break;
		case ECricketFielderState::Tracking:
		case ECricketFielderState::MovingToIntercept:   FieldingState = ECricketFieldingAnimState::Run; break;
		case ECricketFielderState::Catching:            FieldingState = ECricketFieldingAnimState::Catch; break;
		case ECricketFielderState::PickingUp:           FieldingState = ECricketFieldingAnimState::Pickup; break;
		case ECricketFielderState::Throwing:            FieldingState = ECricketFieldingAnimState::Throw; break;
		case ECricketFielderState::ReturningToPosition: FieldingState = ECricketFieldingAnimState::ReturnToPosition; break;
		}
	}
}

double UCricketCharacterAnimComponent::GetBatSpeedMS() const
{
	const UCricketBattingComponent* B = Batting.Get();
	return B ? B->GetCurrentBatSpeedMS() : 0.0;
}

FVector UCricketCharacterAnimComponent::GetBatFaceNormal() const
{
	const UCricketBattingComponent* B = Batting.Get();
	return B ? B->GetCurrentBatState().FaceNormal : FVector(-1, 0, 0);
}

void UCricketCharacterAnimComponent::HandleShotPlayed(FCricketBatImpactReport, FCricketTimingResult)
{
	// The swing met the ball: emit the bat-impact notify coincident with the
	// geometric contact the batting system just resolved (animation observes it).
	EmitNotify(ECricketAnimNotify::BatImpact);
}

void UCricketCharacterAnimComponent::HandleFielderState(ECricketFielderState NewState)
{
	if (NewState == ECricketFielderState::Catching)  { EmitNotify(ECricketAnimNotify::CatchAttempt); }
	if (NewState == ECricketFielderState::PickingUp)  { EmitNotify(ECricketAnimNotify::PickupContact); }
}

void UCricketCharacterAnimComponent::HandleFielderThrew(FVector, FCricketThrowSolution)
{
	EmitNotify(ECricketAnimNotify::ThrowRelease);
}
