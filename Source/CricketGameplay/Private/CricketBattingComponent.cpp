#include "CricketBattingComponent.h"
#include "CricketBall.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketSwingModel.h"
#include "CricketPhysicsConstants.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

UCricketBattingComponent::UCricketBattingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Tick AFTER the ball physics has advanced this frame, so the swept contact
	// test sees the ball's true path over the interval just completed.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UCricketBattingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetBall.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<ACricketBall> It(World); It; ++It)
			{
				TargetBall = *It;
				break;
			}
		}
	}
	ActiveProfile = FCricketSwingModel::BuildProfile(
		CurrentInput.ShotType, CurrentInput.Footwork, CurrentInput.bRightHanded);
}

void UCricketBattingComponent::SetTargetBall(ACricketBall* InBall)
{
	TargetBall = InBall;
}

UCricketBallPhysicsComponent* UCricketBattingComponent::GetTargetBallPhysics() const
{
	return TargetBall.IsValid() ? TargetBall->GetBallPhysics() : nullptr;
}

FVector UCricketBattingComponent::GetStanceOriginM() const
{
	const AActor* Owner = GetOwner();
	const FVector WorldCm = (Owner ? Owner->GetActorLocation() : FVector::ZeroVector) + StanceOffsetCm;
	return WorldToMeters(WorldCm);
}

void UCricketBattingComponent::TriggerSwing()
{
	ActiveProfile = FCricketSwingModel::BuildProfile(
		CurrentInput.ShotType, CurrentInput.Footwork, CurrentInput.bRightHanded);
	bSwingActive = true;
	bContactConsumed = false;
	SwingClock = 0.0;       // start of the downswing; the backlift is assumed set
	bHasPrevBall = false;
}

void UCricketBattingComponent::PlayShotNow(ECricketShotType Shot, ECricketFootwork Foot)
{
	CurrentInput.ShotType = Shot;
	CurrentInput.Footwork = Foot;
	TriggerSwing();
}

void UCricketBattingComponent::Defend()
{
	CurrentInput.ShotType = ECricketShotType::DefensiveBlock;
	if (CurrentInput.Footwork == ECricketFootwork::BackFoot)
	{
		// keep back-foot defence; otherwise default to a solid front-foot block
	}
	else
	{
		CurrentInput.Footwork = ECricketFootwork::FrontFoot;
	}
	TriggerSwing();
}

void UCricketBattingComponent::EndSwing()
{
	bSwingActive = false;
	CurrentPhase = ECricketSwingPhase::Idle;
	CurrentBatSpeedMS = 0.0;
	bHasPrevBall = false;
}

void UCricketBattingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Dt = FMath::Max((double)DeltaTime, 1e-5);
	const FVector StanceM = GetStanceOriginM();

	if (bSwingActive)
	{
		// Sample the bat at the start of this tick (for anim/debug & the contact
		// sweep's start), then attempt a contact across [SwingClock, SwingClock+Dt].
		CurrentBat = FCricketSwingModel::EvaluateBat(
			ActiveProfile, CurrentInput, StanceM, SwingClock, CurrentPhase, CurrentBatSpeedMS);

		UCricketBallPhysicsComponent* Ball = GetTargetBallPhysics();
		if (Ball && Ball->IsBallInFlight() && !bContactConsumed)
		{
			const FVector CurBallM = Ball->GetState().Position;
			if (!bHasPrevBall)
			{
				PrevBallPosM = CurBallM;
				bHasPrevBall = true;
			}

			FCricketContactSolution Sol;
			if (FCricketSwingModel::DetectContact(
					ActiveProfile, CurrentInput, StanceM, BatProfile,
					PrevBallPosM, CurBallM, SwingClock, Dt, ContactSubsteps, Sol))
			{
				// Hand the real swing state + contact point to the EXISTING solver.
				LastReport = Ball->ApplyBatImpact(
					Sol.BatAtContact, BatProfile, MetersToWorld(Sol.ContactPointM));
				LastTiming = Sol.Timing;
				LastContactCm = MetersToWorld(Sol.ContactPointM);
				CurrentBat = Sol.BatAtContact;
				bContactConsumed = true;
				bHasPlayed = true;
				OnShotPlayed.Broadcast(LastReport, LastTiming);
			}
			PrevBallPosM = CurBallM;
		}

		// Record the bat path for the debug overlay (sweet spot, world cm).
		SwingTrailCm.Add(MetersToWorld(CurrentBat.SweetSpotLocation));
		if (SwingTrailCm.Num() > 256) { SwingTrailCm.RemoveAt(0, SwingTrailCm.Num() - 256, EAllowShrinking::No); }

		// Advance the swing clock and end the stroke once the follow-through is done.
		SwingClock += Dt;
		const double SwingEnd = ActiveProfile.DownswingTimeSec + ActiveProfile.FollowThroughTimeSec + 0.10;
		if (SwingClock > SwingEnd)
		{
			EndSwing();
		}
	}
	else
	{
		// Idle guard pose: hold the bat at the backlift/ready position.
		CurrentBat = FCricketSwingModel::EvaluateBat(
			ActiveProfile, CurrentInput, StanceM, 0.0, CurrentPhase, CurrentBatSpeedMS);
		CurrentPhase = ECricketSwingPhase::Idle;
	}

	// Animation Integration Layer: drive the optional bat visual from the swing.
	if (USceneComponent* Visual = BatVisual.Get())
	{
		const FVector LocCm = MetersToWorld(CurrentBat.SweetSpotLocation);
		const FQuat Rot = FRotationMatrix::MakeFromXZ(CurrentBat.LongAxis, CurrentBat.FaceNormal).ToQuat();
		Visual->SetWorldLocationAndRotation(LocCm, Rot);
	}
}
