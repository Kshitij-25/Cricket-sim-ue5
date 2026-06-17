#include "CricketPitchDebugComponent.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketPhysicsSettings.h"
#include "CricketPhysicsConstants.h"
#include "CricketTrajectoryPredictor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

// ---- Console variables: cricket.Debug.Pitch.* (-1 = inherit settings) ------
namespace
{
	TAutoConsoleVariable<int32> CVarPitchEnable(TEXT("cricket.Debug.Pitch.Enable"), -1,
		TEXT("Master pitch debug overlay. -1=project settings, 0=off, 1=on"));
	TAutoConsoleVariable<int32> CVarPitchAngle(TEXT("cricket.Debug.Pitch.BounceAngle"), -1,
		TEXT("Draw incoming/outgoing bounce-angle arrows. -1=settings"));
	TAutoConsoleVariable<int32> CVarPitchSurface(TEXT("cricket.Debug.Pitch.SurfaceInfo"), -1,
		TEXT("Floating per-bounce surface/friction readout. -1=settings"));
	TAutoConsoleVariable<int32> CVarPitchTurnSeam(TEXT("cricket.Debug.Pitch.TurnSeam"), -1,
		TEXT("Draw turn + seam-deviation lateral kicks. -1=settings"));
	TAutoConsoleVariable<int32> CVarPitchPvA(TEXT("cricket.Debug.Pitch.PredictedVsActual"), -1,
		TEXT("Draw predicted vs actual bounce point + error. -1=settings"));

	bool Resolve(const TAutoConsoleVariable<int32>& Cvar, bool bSettingsDefault)
	{
		const int32 V = Cvar.GetValueOnGameThread();
		return V < 0 ? bSettingsDefault : (V != 0);
	}
}

UCricketPitchDebugComponent::UCricketPitchDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Run after the physics component (TG_PrePhysics) has advanced this frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UCricketPitchDebugComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Ball = Owner->FindComponentByClass<UCricketBallPhysicsComponent>();
		if (Ball)
		{
			Ball->OnBounce.AddDynamic(this, &UCricketPitchDebugComponent::HandleBounce);
		}
	}
}

void UCricketPitchDebugComponent::HandleBounce(FCricketBounceReport Report)
{
	if (!Ball)
	{
		return;
	}

	FBounceViz B;
	B.Report = Report;
	B.ContactCm = MetersToWorld(Ball->GetState().Position);

	// Pre-bounce velocity was captured at the end of the previous frame; the
	// post-bounce velocity is the freshly-resolved one. Both give clean dirs.
	B.IncomingDir = PrevVelocityMS.GetSafeNormal();
	B.OutgoingDir = Ball->GetState().Velocity.GetSafeNormal();

	// Pair the actual bounce with the most recent prediction (for the error line).
	B.bHasPrediction = bHasPredictedContact;
	B.PredictedContactCm = PredictedContactCm;

	Bounces.Add(B);
	if (Bounces.Num() > MaxBouncesShown)
	{
		Bounces.RemoveAt(0, Bounces.Num() - MaxBouncesShown, EAllowShrinking::No);
	}

	// Consume the prediction so the next ball gets a fresh one.
	bHasPredictedContact = false;
}

void UCricketPitchDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Ball || !GetWorld())
	{
		return;
	}

	const UCricketPhysicsSettings* S = GetDefault<UCricketPhysicsSettings>();
	if (!Resolve(CVarPitchEnable, S->bEnablePitchDebug))
	{
		PrevVelocityMS = Ball->GetState().Velocity;
		return;
	}

	if (Resolve(CVarPitchPvA, S->bDrawPitchPredictedVsActual))
	{
		UpdatePredictedBounce();
	}

	for (const FBounceViz& B : Bounces)
	{
		DrawBounce(B);
	}

	// Cache this frame's velocity as the pre-bounce velocity for the next bounce.
	PrevVelocityMS = Ball->GetState().Velocity;
}

void UCricketPitchDebugComponent::UpdatePredictedBounce()
{
	if (!Ball->IsBallInFlight())
	{
		return;
	}

	FCricketPredictionParams Params;
	Params.MaxTime = PredictionSeconds;
	Params.PitchPatch = Ball->PitchSurface;     // predict against the SAME surface
	Params.PitchPlaneZ = 0.0;                   // world pitch plane at Z=0 (m)
	Params.bResolveBounces = false;             // we only want the first bounce
	Params.MaxBounces = 1;

	const FCricketTrajectoryPrediction Pred = FCricketTrajectoryPredictor::Predict(
		Ball->GetState(), Ball->GetIntegrator(), Params);

	if (Pred.BouncePoints.Num() > 0)
	{
		PredictedContactCm = MetersToWorld(Pred.BouncePoints[0]);
		bHasPredictedContact = true;
		// Hollow yellow marker at the predicted landing spot.
		DrawDebugSphere(GetWorld(), PredictedContactCm, 7.f, 12, FColor::Yellow, false, -1.f, 0, 1.f);
	}
}

void UCricketPitchDebugComponent::DrawBounce(const FBounceViz& B) const
{
	const UCricketPhysicsSettings* S = GetDefault<UCricketPhysicsSettings>();
	UWorld* World = GetWorld();

	// Bounce point.
	DrawDebugSphere(World, B.ContactCm, 6.f, 12, FColor::Red, false, -1.f, 0, 1.5f);

	// Bounce angle: incoming (red, pointing INTO the spot) + outgoing (green).
	if (Resolve(CVarPitchAngle, S->bDrawPitchBounceAngle))
	{
		constexpr float Len = 45.f; // cm
		if (!B.IncomingDir.IsNearlyZero())
		{
			const FVector Start = B.ContactCm - B.IncomingDir * Len;
			DrawDebugDirectionalArrow(World, Start, B.ContactCm, 10.f, FColor::Red, false, -1.f, 0, 2.f);
		}
		if (!B.OutgoingDir.IsNearlyZero())
		{
			DrawDebugDirectionalArrow(World, B.ContactCm, B.ContactCm + B.OutgoingDir * Len,
				10.f, FColor::Green, false, -1.f, 0, 2.f);
		}
	}

	// Turn (spin) and seam-deviation lateral kicks. Both are signed cross-line
	// magnitudes; draw them along the world +Y (off side) axis from the spot.
	if (Resolve(CVarPitchTurnSeam, S->bDrawPitchTurnSeam))
	{
		const FVector Side = FVector(0, 1, 0);
		const FVector TurnEnd = B.ContactCm + Side * (B.Report.TurnMS * LateralArrowScale) + FVector(0, 0, 4);
		DrawDebugDirectionalArrow(World, B.ContactCm + FVector(0, 0, 4), TurnEnd, 8.f, FColor::Cyan, false, -1.f, 0, 2.f);
		const FVector SeamEnd = B.ContactCm + Side * (B.Report.SeamDeviationMS * LateralArrowScale) + FVector(0, 0, 8);
		DrawDebugDirectionalArrow(World, B.ContactCm + FVector(0, 0, 8), SeamEnd, 8.f, FColor::Orange, false, -1.f, 0, 2.f);
	}

	// Predicted vs actual error.
	if (B.bHasPrediction && Resolve(CVarPitchPvA, S->bDrawPitchPredictedVsActual))
	{
		DrawDebugSphere(World, B.PredictedContactCm, 7.f, 12, FColor::Yellow, false, -1.f, 0, 1.f);
		DrawDebugLine(World, B.PredictedContactCm, B.ContactCm, FColor::Purple, false, -1.f, 0, 1.5f);
	}

	// Floating per-bounce surface/physics readout.
	if (Resolve(CVarPitchSurface, S->bDrawPitchSurfaceInfo))
	{
		const FCricketBounceReport& R = B.Report;
		const FString Text = FString::Printf(
			TEXT("e %.2f | mu %.2f | %s grip %.2f\nbounce %.0f deg, h %.2f m | pace %.0f%%\nturn %+.2f m/s | seam %+.2f m/s"),
			R.RestitutionUsed, R.FrictionUsed, R.bGripped ? TEXT("GRIP") : TEXT("skid"),
			R.GripLevel, R.BounceAngleDeg, R.BounceHeightM, R.SpeedRetainedFrac * 100.0,
			R.TurnMS, R.SeamDeviationMS);
		DrawDebugString(World, B.ContactCm + FVector(0, 0, 20), Text, nullptr, FColor::White, 0.f);
	}
}
