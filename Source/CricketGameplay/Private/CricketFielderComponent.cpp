#include "CricketFielderComponent.h"
#include "CricketBall.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketFieldingPredictor.h"
#include "CricketPhysicsConstants.h"
#include "CricketPerfProfiler.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

namespace
{
	FCricketPredictionParams FielderPredictionParams()
	{
		FCricketPredictionParams P;
		P.MaxTime = 5.0;
		P.SampleInterval = 0.02;   // coarse enough for decisions, fine enough to be accurate
		P.PitchPlaneZ = 0.0;       // the field plane
		P.MaxBounces = 4;
		P.bResolveBounces = true;
		return P;
	}

	FORCEINLINE double HorizDistM(const FVector& A, const FVector& B)
	{
		return FVector(A.X - B.X, A.Y - B.Y, 0.0).Size();
	}
}

UCricketFielderComponent::UCricketFielderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the ball has advanced this frame, so we react to its current path.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UCricketFielderComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!bHomeSet)
	{
		if (const AActor* Owner = GetOwner()) { HomeWorldCm = Owner->GetActorLocation(); bHomeSet = true; }
	}
}

void UCricketFielderComponent::SetTargetBall(ACricketBall* InBall)
{
	TargetBall = InBall;
}

UCricketBallPhysicsComponent* UCricketFielderComponent::GetBallPhysics() const
{
	return TargetBall.IsValid() ? TargetBall->GetBallPhysics() : nullptr;
}

FVector UCricketFielderComponent::GetHandWorldCm() const
{
	const AActor* Owner = GetOwner();
	const FVector Loc = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	return Loc + FVector(0, 0, HandHeightM * MetersToUE);
}

FCricketInterceptQuery UCricketFielderComponent::MakeQuery() const
{
	FCricketInterceptQuery Q;
	const AActor* Owner = GetOwner();
	Q.FielderPosM = WorldToMeters(Owner ? Owner->GetActorLocation() : FVector::ZeroVector);
	Q.MaxSpeedMS = MaxSpeedMS;
	Q.ReactionTimeSec = ReactionTimeSec;
	Q.ReachRadiusM = ReachRadiusM;
	Q.CatchReachHeightM = CatchReachHeightM;
	Q.GroundFieldHeightM = GroundFieldHeightM;
	return Q;
}

FCricketInterceptResult UCricketFielderComponent::EvaluateIntercept(FCricketBallPrediction& OutPrediction) const
{
	OutPrediction = FCricketBallPrediction();
	UCricketBallPhysicsComponent* Ball = GetBallPhysics();
	if (!Ball || !Ball->IsBallInFlight())
	{
		return FCricketInterceptResult();
	}
	OutPrediction = FCricketFieldingPredictor::PredictBall(
		Ball->GetState(), Ball->GetIntegrator(), FielderPredictionParams());
	return FCricketFieldingPredictor::SolveIntercept(OutPrediction, MakeQuery());
}

void UCricketFielderComponent::SetActiveChaser(bool bActive)
{
	bIsActiveChaser = bActive;
	if (!bActive && !bHoldingBall && State != ECricketFielderState::ReturningToPosition && State != ECricketFielderState::Idle)
	{
		SetState(ECricketFielderState::ReturningToPosition);
	}
}

void UCricketFielderComponent::SetState(ECricketFielderState NewState)
{
	if (State == NewState) { return; }
	State = NewState;
	StateTime = 0.0;

	// On entering a collecting state, secure the ball: stop its physics and hold it.
	if (NewState == ECricketFielderState::Catching || NewState == ECricketFielderState::PickingUp)
	{
		if (UCricketBallPhysicsComponent* Ball = GetBallPhysics())
		{
			Ball->Freeze();
			bHoldingBall = true;
		}
	}
	OnStateChanged.Broadcast(NewState);
}

bool UCricketFielderComponent::MoveOwnerToward(const FVector& TargetCm, double SpeedMS, double DeltaSeconds)
{
	AActor* Owner = GetOwner();
	if (!Owner) { return true; }

	const FVector Cur = Owner->GetActorLocation();
	const FVector Flat(TargetCm.X - Cur.X, TargetCm.Y - Cur.Y, 0.0); // run on the ground (keep feet Z)
	const double Dist = Flat.Size();
	const double Step = SpeedMS * MetersToUE * DeltaSeconds;

	if (Dist <= Step || Dist < KINDA_SMALL_NUMBER)
	{
		Owner->SetActorLocation(FVector(TargetCm.X, TargetCm.Y, Cur.Z), false);
		return true;
	}
	Owner->SetActorLocation(Cur + (Flat / Dist) * Step, false);
	return false;
}

void UCricketFielderComponent::HoldBallAtHand()
{
	if (ACricketBall* Ball = TargetBall.Get())
	{
		Ball->SetActorLocation(GetHandWorldCm(), false);
	}
}

FVector UCricketFielderComponent::ResolveThrowTargetCm() const
{
	switch (ThrowPreference)
	{
	case ECricketThrowTarget::Stumps:
		if (!StumpsWorldCm.IsZero()) { return StumpsWorldCm; }
		break;
	case ECricketThrowTarget::Keeper:
		if (!KeeperWorldCm.IsZero()) { return KeeperWorldCm; }
		break;
	case ECricketThrowTarget::NearestFielder:
	{
		const FVector Me = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
		double Best = TNumericLimits<double>::Max();
		FVector BestPos = KeeperWorldCm;
		for (const FVector& T : Teammates)
		{
			const double D = FVector::DistSquared(Me, T);
			if (D > 1.0 && D < Best) { Best = D; BestPos = T; }
		}
		return BestPos;
	}
	}
	// Fallbacks: prefer the keeper, then the stumps.
	if (!KeeperWorldCm.IsZero()) { return KeeperWorldCm; }
	return StumpsWorldCm;
}

void UCricketFielderComponent::ExecuteThrow()
{
	UCricketBallPhysicsComponent* Ball = GetBallPhysics();
	if (!Ball) { return; }

	const FVector FromCm = GetHandWorldCm();
	const FVector TargetCm = ResolveThrowTargetCm();
	const FVector FromM = WorldToMeters(FromCm);
	const FVector TargetM = WorldToMeters(TargetCm);

	FCricketThrowSolution Sol = FCricketFieldingPredictor::SolveThrow(FromM, TargetM, ThrowSpeedMS, /*flat*/ true);
	if (!Sol.bFeasible)
	{
		// Out of range even flat: throw hard along the horizontal toward the target.
		const FVector Dir = (FVector(TargetM.X - FromM.X, TargetM.Y - FromM.Y, 0.0)).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
		Sol.LaunchVelocityMS = Dir * ThrowSpeedMS + FVector(0, 0, ThrowSpeedMS * 0.15);
		Sol.bFeasible = true;
	}

	bHoldingBall = false;
	Ball->Release(FromCm, Sol.LaunchVelocityMS, FVector::ZeroVector, FVector(0, 1, 0));

	// Cache the throw path for the debug overlay.
	ThrowPathCm.Reset();
	FCricketPredictionParams P;
	P.MaxTime = FMath::Max(Sol.FlightTimeSec * 1.2, 0.5);
	P.SampleInterval = 0.02;
	P.bResolveBounces = false;
	const FCricketBallPrediction Path = FCricketFieldingPredictor::PredictBall(Ball->GetState(), Ball->GetIntegrator(), P);
	for (const FCricketTrajectorySample& S : Path.Path)
	{
		ThrowPathCm.Add(MetersToWorld(S.Position));
	}

	OnThrew.Broadcast(TargetCm, Sol);
}

void UCricketFielderComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Profiled: fielding intercept/catch forecast (ball trajectory prediction).
	CRICKET_PERF_SCOPE(Prediction);

	const double Dt = FMath::Max((double)DeltaTime, 1e-5);
	StateTime += Dt;

	UCricketBallPhysicsComponent* Ball = GetBallPhysics();
	const bool bBallLive = Ball && Ball->IsBallInFlight();

	switch (State)
	{
	case ECricketFielderState::Idle:
		if (bIsActiveChaser && bBallLive) { SetState(ECricketFielderState::Tracking); }
		break;

	case ECricketFielderState::Tracking:
	{
		if (!bBallLive) { SetState(ECricketFielderState::ReturningToPosition); break; }
		Intercept = EvaluateIntercept(Prediction);
		if (Intercept.bCanIntercept)
		{
			MoveTargetCm = MetersToWorld(FVector(Intercept.PointM.X, Intercept.PointM.Y, 0.0));
			SetState(ECricketFielderState::MovingToIntercept);
		}
		else if (!bIsActiveChaser)
		{
			SetState(ECricketFielderState::ReturningToPosition);
		}
		break;
	}

	case ECricketFielderState::MovingToIntercept:
	{
		if (!bBallLive && !bHoldingBall) { SetState(ECricketFielderState::ReturningToPosition); break; }

		// Re-predict each tick so the chase adapts to bounces / drag.
		Intercept = EvaluateIntercept(Prediction);
		if (Intercept.bCanIntercept)
		{
			MoveTargetCm = MetersToWorld(FVector(Intercept.PointM.X, Intercept.PointM.Y, 0.0));
		}
		MoveOwnerToward(MoveTargetCm, MaxSpeedMS, Dt);

		// Are we on the ball yet?
		const FVector BallPosM = Ball->GetState().Position;
		const FVector BallPosCm = MetersToWorld(BallPosM);
		const FVector HandCm = GetHandWorldCm();
		const double Dist3D = WorldToMeters(BallPosCm - HandCm).Size();
		const double HorizDist = WorldToMeters(FVector(BallPosCm.X - HandCm.X, BallPosCm.Y - HandCm.Y, 0.0)).Size();

		if (BallPosM.Z > GroundFieldHeightM && Dist3D <= CatchRadiusM)
		{
			SetState(ECricketFielderState::Catching);
		}
		else if (BallPosM.Z <= GroundFieldHeightM && HorizDist <= PickupRadiusM)
		{
			SetState(ECricketFielderState::PickingUp);
		}
		break;
	}

	case ECricketFielderState::Catching:
		HoldBallAtHand();
		if (StateTime >= CatchHoldSec) { SetState(ECricketFielderState::Throwing); }
		break;

	case ECricketFielderState::PickingUp:
		HoldBallAtHand();
		if (StateTime >= GatherTimeSec) { SetState(ECricketFielderState::Throwing); }
		break;

	case ECricketFielderState::Throwing:
		// Windup: the ball stays in hand while the arm winds up. ExecuteThrow (the
		// ThrowRelease physics handoff) fires only once the windup elapses, not the
		// instant the state is entered — Throwing is a real timed phase, not a snap.
		HoldBallAtHand();
		if (StateTime >= ThrowWindupSec)
		{
			ExecuteThrow();
			SetState(ECricketFielderState::ReturningToPosition);
		}
		break;

	case ECricketFielderState::ReturningToPosition:
		if (MoveOwnerToward(HomeWorldCm, ReturnSpeedMS, Dt))
		{
			SetState(ECricketFielderState::Idle);
		}
		break;
	}
}
