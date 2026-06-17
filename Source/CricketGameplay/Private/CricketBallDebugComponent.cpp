#include "CricketBallDebugComponent.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketPhysicsSettings.h"
#include "CricketPhysicsConstants.h"
#include "CricketTrajectoryPredictor.h"
#include "CricketAerodynamics.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

// ---- Console variables: cricket.Debug.* (-1 = inherit project settings) ----
namespace
{
	TAutoConsoleVariable<int32> CVarEnable(TEXT("cricket.Debug.Enable"), -1,
		TEXT("Master ball-physics debug overlay. -1=use project settings, 0=off, 1=on"));
	TAutoConsoleVariable<int32> CVarVelocity(TEXT("cricket.Debug.Velocity"), -1, TEXT("Draw velocity vector. -1=settings"));
	TAutoConsoleVariable<int32> CVarSpin(TEXT("cricket.Debug.SpinAxis"), -1, TEXT("Draw spin axis + RPM. -1=settings"));
	TAutoConsoleVariable<int32> CVarSeam(TEXT("cricket.Debug.Seam"), -1, TEXT("Draw seam plane/normal. -1=settings"));
	TAutoConsoleVariable<int32> CVarForces(TEXT("cricket.Debug.Forces"), -1, TEXT("Draw drag/Magnus/swing forces. -1=settings"));
	TAutoConsoleVariable<int32> CVarActual(TEXT("cricket.Debug.ActualTrajectory"), -1, TEXT("Draw actual path trail. -1=settings"));
	TAutoConsoleVariable<int32> CVarPredicted(TEXT("cricket.Debug.PredictedTrajectory"), -1, TEXT("Draw predicted path. -1=settings"));
	TAutoConsoleVariable<int32> CVarBounce(TEXT("cricket.Debug.BouncePoints"), -1, TEXT("Draw bounce points. -1=settings"));
	TAutoConsoleVariable<int32> CVarReadout(TEXT("cricket.Debug.Readout"), -1, TEXT("On-screen telemetry readout. -1=settings"));

	bool Resolve(const TAutoConsoleVariable<int32>& Cvar, bool bSettingsDefault)
	{
		const int32 V = Cvar.GetValueOnGameThread();
		return V < 0 ? bSettingsDefault : (V != 0);
	}
}

UCricketBallDebugComponent::UCricketBallDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Run after the physics component (TG_PrePhysics) has advanced this frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UCricketBallDebugComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Ball = Owner->FindComponentByClass<UCricketBallPhysicsComponent>();
		if (Ball)
		{
			Ball->OnBounce.AddDynamic(this, &UCricketBallDebugComponent::HandleBounce);
		}
	}
}

void UCricketBallDebugComponent::HandleBounce(FCricketBounceReport /*Report*/)
{
	if (AActor* Owner = GetOwner())
	{
		ActualBounces.Add(Owner->GetActorLocation());
	}
}

void UCricketBallDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Ball || !GetWorld())
	{
		return;
	}

	const UCricketPhysicsSettings* S = GetDefault<UCricketPhysicsSettings>();
	const bool bMaster = Resolve(CVarEnable, S->bEnableDebugByDefault);
	if (!bMaster)
	{
		return;
	}

	// Maintain the actual-path trail (only while in flight).
	if (Ball->IsBallInFlight())
	{
		Trail.Add(GetOwner()->GetActorLocation());
		if (Trail.Num() > MaxTrailPoints)
		{
			Trail.RemoveAt(0, Trail.Num() - MaxTrailPoints, EAllowShrinking::No);
		}
	}

	if (Resolve(CVarVelocity, S->bDrawVelocity))           { DrawVelocity(); }
	if (Resolve(CVarSpin, S->bDrawSpinAxis))               { DrawSpin(); }
	if (Resolve(CVarSeam, S->bDrawSeam))                   { DrawSeam(); }
	if (Resolve(CVarForces, S->bDrawForces))               { DrawForces(); }
	if (Resolve(CVarActual, S->bDrawActualTrajectory))     { DrawActualTrajectory(); }
	if (Resolve(CVarPredicted, S->bDrawPredictedTrajectory)){ DrawPredictedTrajectory(); }
	if (Resolve(CVarBounce, S->bDrawBouncePoints))         { DrawBouncePoints(); }
	if (Resolve(CVarReadout, S->bShowReadout))             { DrawReadout(); }
}

void UCricketBallDebugComponent::DrawVelocity() const
{
	const UCricketPhysicsSettings* S = GetDefault<UCricketPhysicsSettings>();
	const FVector Origin = GetOwner()->GetActorLocation();
	const FVector V = Ball->GetState().Velocity; // m/s, world axes
	if (V.IsNearlyZero()) { return; }
	const FVector End = Origin + V * S->VelocityArrowScale; // cm per (m/s)
	DrawDebugDirectionalArrow(GetWorld(), Origin, End, 12.f, FColor::Cyan, false, -1.f, 0, 1.5f);
}

void UCricketBallDebugComponent::DrawSpin() const
{
	const FVector Origin = GetOwner()->GetActorLocation();
	const FVector W = Ball->GetState().AngularVelocity;
	if (W.IsNearlyZero()) { return; }
	const FVector Axis = W.GetSafeNormal();
	// Spin axis as a double-ended line; length fixed for legibility.
	DrawDebugLine(GetWorld(), Origin - Axis * 25.f, Origin + Axis * 25.f, FColor::Magenta, false, -1.f, 0, 2.f);
	const FString Label = FString::Printf(TEXT("%.0f rpm"), Ball->GetSpinRPM());
	DrawDebugString(GetWorld(), Origin + Axis * 28.f, Label, nullptr, FColor::Magenta, 0.f);
}

void UCricketBallDebugComponent::DrawSeam() const
{
	const FVector Origin = GetOwner()->GetActorLocation();
	const FVector N = Ball->GetState().SeamNormal.GetSafeNormal();
	if (N.IsNearlyZero()) { return; }

	// Build a basis spanning the seam plane (perpendicular to the seam normal).
	FVector U = FVector::CrossProduct(N, FVector::UpVector);
	if (U.IsNearlyZero()) { U = FVector::CrossProduct(N, FVector::ForwardVector); }
	U = U.GetSafeNormal();
	const FVector Vv = FVector::CrossProduct(N, U).GetSafeNormal();

	const float R = 14.f; // cm, exaggerated for visibility
	DrawDebugCircle(GetWorld(), Origin, R, 32, FColor::Yellow, false, -1.f, 0, 1.2f, U, Vv, false);
	// Seam normal stub.
	DrawDebugLine(GetWorld(), Origin, Origin + N * 18.f, FColor::Orange, false, -1.f, 0, 1.5f);
}

void UCricketBallDebugComponent::DrawForces() const
{
	const UCricketPhysicsSettings* S = GetDefault<UCricketPhysicsSettings>();
	const FVector Origin = GetOwner()->GetActorLocation();
	const FCricketAeroResult& A = Ball->GetLastAero();
	const float K = S->ForceArrowScale * 20.f; // cm per Newton (forces are ~0.1–2 N)

	auto DrawForce = [&](const FVector& F, const FColor& C)
	{
		if (F.IsNearlyZero()) { return; }
		DrawDebugDirectionalArrow(GetWorld(), Origin, Origin + F * K, 10.f, C, false, -1.f, 0, 1.5f);
	};
	DrawForce(A.DragForce, FColor::Red);      // drag (opposes motion)
	DrawForce(A.MagnusForce, FColor::Green);  // Magnus (carry/dip/drift)
	DrawForce(A.SwingForce, FColor::Blue);    // swing (seam side force)
}

void UCricketBallDebugComponent::DrawActualTrajectory() const
{
	for (int32 i = 1; i < Trail.Num(); ++i)
	{
		DrawDebugLine(GetWorld(), Trail[i - 1], Trail[i], FColor::White, false, -1.f, 0, 2.f);
	}
}

void UCricketBallDebugComponent::DrawPredictedTrajectory() const
{
	if (!Ball->IsBallInFlight()) { return; }

	FCricketPredictionParams Params;
	Params.MaxTime = PredictionSeconds;
	Params.PitchPatch = PredictionPitch;
	Params.PitchPlaneZ = 0.0; // world pitch plane assumed at Z=0 (m)
	Params.bResolveBounces = true;

	const FCricketTrajectoryPrediction Pred = FCricketTrajectoryPredictor::Predict(
		Ball->GetState(), Ball->GetIntegrator(), Params);

	for (int32 i = 1; i < Pred.Samples.Num(); ++i)
	{
		DrawDebugLine(GetWorld(),
			MetersToWorld(Pred.Samples[i - 1].Position),
			MetersToWorld(Pred.Samples[i].Position),
			FColor::Emerald, false, -1.f, 0, 1.2f);
	}
	// Predicted bounce markers (hollow, to distinguish from actual).
	for (const FVector& B : Pred.BouncePoints)
	{
		DrawDebugSphere(GetWorld(), MetersToWorld(B), 6.f, 10, FColor::Yellow, false, -1.f, 0, 1.f);
	}
}

void UCricketBallDebugComponent::DrawBouncePoints() const
{
	for (const FVector& B : ActualBounces)
	{
		DrawDebugSphere(GetWorld(), B, 7.f, 12, FColor::Red, false, -1.f, 0, 1.5f);
	}
}

void UCricketBallDebugComponent::DrawReadout() const
{
	if (!GEngine) { return; }

	const FCricketBallState& St = Ball->GetState();
	const FCricketAeroResult& A = Ball->GetLastAero();

	const FString Regime = A.ReverseRegime > 0.5 ? TEXT("REVERSE") : TEXT("conventional");
	auto Line = [&](int32 Key, const FColor& C, const FString& Text)
	{
		GEngine->AddOnScreenDebugMessage(Key, 0.f, C, Text);
	};

	Line(1001, FColor::Cyan,    FString::Printf(TEXT("Speed:    %.1f km/h"), Ball->GetSpeedKmh()));
	Line(1002, FColor::Magenta, FString::Printf(TEXT("Spin:     %.0f rpm (%.1f rad/s)"), Ball->GetSpinRPM(), St.SpinRateRadS()));
	Line(1003, FColor::White,   FString::Printf(TEXT("Seam ang: %.1f deg | stability %.2f"), FMath::RadiansToDegrees(A.SeamAngleRad), St.SeamStability));
	Line(1004, FColor::Yellow,  FString::Printf(TEXT("Reynolds: %.0f | regime %.2f (%s)"), A.ReynoldsNumber, A.ReverseRegime, *Regime));
	Line(1005, FColor::Red,     FString::Printf(TEXT("Drag:     %.3f N (Cd %.3f)"), A.DragForce.Size(), A.DragCoefficient));
	Line(1006, FColor::Green,   FString::Printf(TEXT("Magnus:   %.3f N (Cl %.3f)"), A.MagnusForce.Size(), A.MagnusLiftCoefficient));
	Line(1007, FColor::Blue,    FString::Printf(TEXT("Swing:    %.3f N (Cs %.3f)"), A.SwingForce.Size(), A.SwingSideForceCoefficient));
}
