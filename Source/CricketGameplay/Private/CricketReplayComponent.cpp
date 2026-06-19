#include "CricketReplayComponent.h"
#include "CricketBall.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketCharacterAnimComponent.h"
#include "CricketFielderComponent.h"
#include "CricketCameraModel.h"
#include "CricketPitchInteraction.h"
#include "CricketBatTypes.h"
#include "CricketPhysicsConstants.h"
#include "CricketPerfProfiler.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

namespace
{
	TAutoConsoleVariable<int32> CVarReplayDebug(TEXT("cricket.Debug.Replay"), 0,
		TEXT("Replay physics-visualization overlays. 0=off, 1=on"));
}

UCricketReplayComponent::UCricketReplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // after the ball has moved this frame
}

void UCricketReplayComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UCricketBallPhysicsComponent* BP = BallPhysics())
	{
		BP->OnBounce.AddDynamic(this, &UCricketReplayComponent::HandleBounce);
		BP->OnBatImpact.AddDynamic(this, &UCricketReplayComponent::HandleBatImpact);
	}
}

void UCricketReplayComponent::SetBall(ACricketBall* InBall)
{
	Ball = InBall;
	if (UCricketBallPhysicsComponent* BP = BallPhysics())
	{
		BP->OnBounce.AddDynamic(this, &UCricketReplayComponent::HandleBounce);
		BP->OnBatImpact.AddDynamic(this, &UCricketReplayComponent::HandleBatImpact);
	}
}

UCricketBallPhysicsComponent* UCricketReplayComponent::BallPhysics() const
{
	return Ball.IsValid() ? Ball->GetBallPhysics() : nullptr;
}

void UCricketReplayComponent::RegisterActor(AActor* Actor, int32 Id)
{
	if (!Actor) { return; }
	FCricketRecordedActor R; R.Actor = Actor; R.Id = Id;
	Actors.Add(R);
}

void UCricketReplayComponent::MarkEvent(ECricketReplayEventType Type, const FVector& WorldCm)
{
	FCricketReplayEvent E; E.Type = Type; E.Time = RecordClock; E.LocationM = WorldToMeters(WorldCm);
	Clip.AddEvent(E);
}

void UCricketReplayComponent::HandleBounce(FCricketBounceReport)
{
	if (bRecording) { if (UCricketBallPhysicsComponent* BP = BallPhysics()) { MarkEvent(ECricketReplayEventType::Bounce, MetersToWorld(BP->GetState().Position)); } }
}

void UCricketReplayComponent::HandleBatImpact(FCricketBatImpactReport)
{
	if (bRecording) { if (UCricketBallPhysicsComponent* BP = BallPhysics()) { MarkEvent(ECricketReplayEventType::BatImpact, BP->GetLastBatContactCm()); } }
}

void UCricketReplayComponent::CaptureFrame()
{
	// Profiled: per-frame replay recording cost (snapshot build + ring append).
	CRICKET_PERF_SCOPE(Replay);

	FCricketReplayFrame F;
	F.Time = RecordClock;

	if (UCricketBallPhysicsComponent* BP = BallPhysics())
	{
		const FCricketBallState& St = BP->GetState();
		F.Ball.PositionM = St.Position;
		F.Ball.VelocityMS = St.Velocity;
		F.Ball.AngularVelocityRadS = St.AngularVelocity;
		F.Ball.SeamNormal = St.SeamNormal;
		F.Ball.bInFlight = BP->IsBallInFlight();
	}

	for (const FCricketRecordedActor& R : Actors)
	{
		AActor* A = R.Actor.Get();
		if (!A) { continue; }
		FCricketActorSnapshot S;
		S.ActorId = R.Id;
		S.LocationCm = A->GetActorLocation();
		S.Rotation = A->GetActorRotation();
		if (UCricketCharacterAnimComponent* Anim = A->FindComponentByClass<UCricketCharacterAnimComponent>())
		{
			S.AnimStateId = (uint8)Anim->GetLocomotionState();
		}
		else if (UCricketFielderComponent* Fld = A->FindComponentByClass<UCricketFielderComponent>())
		{
			S.AnimStateId = (uint8)Fld->GetState();
		}
		F.Actors.Add(S);
	}
	Clip.AddFrame(F);
}

void UCricketReplayComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const double Dt = FMath::Max((double)DeltaTime, 1e-5);

	if (bReplaying)
	{
		Player.Advance(Dt);
		const FCricketReplayFrame S = Clip.SampleAtTime(Player.CursorTime);
		ReplayBallCm = MetersToWorld(S.Ball.PositionM);

		if (ACricketBall* B = Ball.Get())
		{
			if (UCricketBallPhysicsComponent* BP = BallPhysics()) { BP->Freeze(); } // no re-sim during playback
			B->SetActorLocation(ReplayBallCm, false);
		}
		for (const FCricketRecordedActor& R : Actors)
		{
			AActor* A = R.Actor.Get();
			if (!A) { continue; }
			const FCricketActorSnapshot* Snap = S.Actors.FindByPredicate(
				[&](const FCricketActorSnapshot& X) { return X.ActorId == R.Id; });
			if (Snap) { A->SetActorLocationAndRotation(Snap->LocationCm, Snap->Rotation, false); }
		}
		DrawPhysicsOverlays();
		return;
	}

	// --- Recording ---
	UCricketBallPhysicsComponent* BP = BallPhysics();
	const bool bLive = BP && BP->IsBallInFlight();
	if (bAutoRecordLiveBall)
	{
		if (bLive && !bPrevBallLive) // a new delivery began
		{
			Clip.Reset();
			RecordClock = 0.0;
			SampleAccumulator = 1e9; // capture immediately
			bRecording = true;
			MarkEvent(ECricketReplayEventType::Release, MetersToWorld(BP->GetState().Position));
		}
		else if (!bLive && bPrevBallLive)
		{
			bRecording = false; // keep the clip for replay
		}
	}
	bPrevBallLive = bLive;

	if (bRecording)
	{
		RecordClock += Dt;
		SampleAccumulator += Dt;
		const double Interval = 1.0 / FMath::Max(SampleHz, 1.0);
		if (SampleAccumulator >= Interval) { SampleAccumulator = 0.0; CaptureFrame(); }
	}

	DrawPhysicsOverlays();
}

void UCricketReplayComponent::StartReplay()
{
	if (Clip.NumFrames() < 2) { return; }
	Player.Start(Clip);
	bReplaying = true;
	bRecording = false;
}

void UCricketReplayComponent::StopReplay()
{
	bReplaying = false;
}

void UCricketReplayComponent::TogglePause() { if (bReplaying) { Player.TogglePause(); } }
void UCricketReplayComponent::StepFrames(int32 N) { if (bReplaying) { Player.StepFrames(Clip, N); } }
void UCricketReplayComponent::AdjustRate(double Delta) { Player.SetRate(Player.Rate + Delta); }
void UCricketReplayComponent::SetRate(double Rate) { Player.SetRate(Rate); }

int32 UCricketReplayComponent::BounceFrameIndex() const
{
	for (const FCricketReplayEvent& E : Clip.Events)
	{
		if (E.Type == ECricketReplayEventType::Bounce) { return Clip.FrameIndexAtTime(E.Time); }
	}
	return INDEX_NONE;
}

void UCricketReplayComponent::DrawPhysicsOverlays() const
{
	if (!GetWorld() || CVarReplayDebug.GetValueOnGameThread() == 0) { return; }
	if (Clip.NumFrames() < 2) { return; }

	// Ball path.
	TArray<FVector> PathM; Clip.GetBallPath(PathM);
	for (int32 i = 1; i < PathM.Num(); ++i)
	{
		DrawDebugLine(GetWorld(), MetersToWorld(PathM[i - 1]), MetersToWorld(PathM[i]), FColor::Emerald, false, -1.f, 0, 2.f);
	}

	// Bounce points.
	TArray<FVector> Bounces; Clip.GetEventLocations(ECricketReplayEventType::Bounce, Bounces);
	for (const FVector& B : Bounces) { DrawDebugSphere(GetWorld(), MetersToWorld(B), 8.f, 12, FColor::Yellow, false, -1.f, 0, 2.f); }

	// Impact locations.
	TArray<FVector> Impacts; Clip.GetEventLocations(ECricketReplayEventType::BatImpact, Impacts);
	for (const FVector& I : Impacts) { DrawDebugSphere(GetWorld(), MetersToWorld(I), 7.f, 12, FColor::Red, false, -1.f, 0, 2.5f); }

	// Catch / throw markers.
	TArray<FVector> Catches; Clip.GetEventLocations(ECricketReplayEventType::Catch, Catches);
	for (const FVector& C : Catches) { DrawDebugSphere(GetWorld(), MetersToWorld(C), 9.f, 12, FColor::Green, false, -1.f, 0, 2.f); }

	// Measured swing / spin deviation (the realism, made visible).
	const int32 BounceIdx = BounceFrameIndex();
	if (BounceIdx != INDEX_NONE && GEngine)
	{
		const double Swing = FCricketCameraModel::SwingDeviationM(PathM, BounceIdx);
		const double Spin = FCricketCameraModel::SpinDeviationM(PathM, BounceIdx);
		GEngine->AddOnScreenDebugMessage(8500, 0.f, FColor::Emerald,
			FString::Printf(TEXT("Replay: swing %.1f cm  |  off-pitch (spin/seam) %.1f cm  |  %d frames"),
				Swing * 100.0, Spin * 100.0, Clip.NumFrames()));
	}
}
