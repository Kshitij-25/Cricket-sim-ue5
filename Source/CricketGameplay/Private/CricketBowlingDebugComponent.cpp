#include "CricketBowlingDebugComponent.h"
#include "CricketBowlingComponent.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketBallIntegrator.h"
#include "CricketTrajectoryPredictor.h"
#include "CricketPhysicsConstants.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

namespace
{
	TAutoConsoleVariable<int32> CVarBowlDebug(TEXT("cricket.Bowl.Debug.Enable"), 0,
		TEXT("Master bowling debug overlay. 0=off, 1=on"));
	TAutoConsoleVariable<int32> CVarBowlMap(TEXT("cricket.Bowl.Debug.PitchMap"), 0,
		TEXT("Draw the pitch map (length zones + landing marks + stumps). 0=off, 1=on"));
	TAutoConsoleVariable<int32> CVarBowlSwing(TEXT("cricket.Bowl.Debug.Swing"), 0,
		TEXT("Draw aim line + swing prediction (chord vs curved path). 0=off, 1=on"));
	TAutoConsoleVariable<int32> CVarBowlReadout(TEXT("cricket.Bowl.Debug.Readout"), 0,
		TEXT("On-screen delivery parameter readout. 0=off, 1=on"));

	/** Pitch-map colour keyed to length-from-the-striker (m). */
	FColor ColorForLength(double L)
	{
		if (L < 2.0)  { return FColor(255, 220, 40); }   // full / yorker
		if (L < 4.0)  { return FColor::Green; }          // full
		if (L < 7.0)  { return FColor::Cyan; }           // good length
		if (L < 9.0)  { return FColor(60, 120, 255); }   // back of a length
		if (L < 11.0) { return FColor::Orange; }         // short
		return FColor::Red;                              // bouncer
	}

	void HorizFrame(const FVector& ReleaseCm, const FVector& StrikerCm, FVector& OutFwd, FVector& OutLat)
	{
		FVector F = StrikerCm - ReleaseCm; F.Z = 0.0;
		OutFwd = F.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
		OutLat = FVector(-OutFwd.Y, OutFwd.X, 0.0).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
	}
}

UCricketBowlingDebugComponent::UCricketBowlingDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics; // after the ball has advanced this frame
}

void UCricketBowlingDebugComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Bowl = Owner->FindComponentByClass<UCricketBowlingComponent>();
		if (Bowl)
		{
			Bowl->OnDelivery.AddDynamic(this, &UCricketBowlingDebugComponent::HandleDelivery);
		}
	}
}

void UCricketBowlingDebugComponent::RebindBall()
{
	if (!Bowl) { return; }
	UCricketBallPhysicsComponent* Phys = Bowl->GetTargetBallPhysics();
	if (Phys && Phys != BallPhys)
	{
		if (BallPhys) { BallPhys->OnBounce.RemoveDynamic(this, &UCricketBowlingDebugComponent::HandleBounce); }
		BallPhys = Phys;
		BallPhys->OnBounce.AddDynamic(this, &UCricketBowlingDebugComponent::HandleBounce);
	}
}

void UCricketBowlingDebugComponent::HandleDelivery(FCricketReleaseParameters Params, FCricketDeliveryDiagnostics Diagnostics)
{
	if (!Bowl) { return; }

	// Predict the flight once, through the same model, and cache it in cm.
	FCricketBallState S;
	S.Position        = Params.ReleasePositionM;
	S.Velocity        = Params.ReleaseVelocityMS;
	S.AngularVelocity = Params.AngularVelocityRadS;
	S.SeamNormal      = Params.SeamNormal;
	S.SeamStability   = Params.SeamStability;

	FCricketBallIntegrator I(Params.BallCondition, Bowl->Environment, Params.Coefficients);
	FCricketPredictionParams P;
	P.MaxTime         = PredictionSeconds;
	P.SampleInterval  = 0.005;
	P.PitchPlaneZ     = Bowl->GetGroundPlaneZM(); // same plane the generator solved against
	P.bResolveBounces = true;
	P.MaxBounces      = 2;
	P.PitchPatch      = Bowl->PitchSurface;

	const FCricketTrajectoryPrediction Pred = FCricketTrajectoryPredictor::Predict(S, I, P);

	PredictedPathCm.Reset();
	for (const FCricketTrajectorySample& Sample : Pred.Samples)
	{
		PredictedPathCm.Add(MetersToWorld(Sample.Position));
	}
	PredictedPitchCm = (Pred.BouncePoints.Num() > 0)
		? MetersToWorld(Pred.BouncePoints[0])
		: MetersToWorld(Diagnostics.PredictedPitchPointM);

	bHasPrediction = true;
	Trail.Reset();
	++BallsBowled;
}

void UCricketBowlingDebugComponent::HandleBounce(FCricketBounceReport /*Report*/)
{
	if (!Bowl || !BallPhys || !BallPhys->GetOwner()) { return; }

	FVector Fwd, Lat;
	HorizFrame(Bowl->GetReleaseWorldCm(), Bowl->GetStrikerStumpsWorldCm(), Fwd, Lat);

	FPitchMark Mark;
	Mark.LocationCm = BallPhys->GetOwner()->GetActorLocation();
	Mark.LengthM = FVector::DotProduct(Bowl->GetStrikerStumpsWorldCm() - Mark.LocationCm, Fwd) * UEToMeters;
	PitchMarks.Add(Mark);
	if (PitchMarks.Num() > MaxPitchMarks)
	{
		PitchMarks.RemoveAt(0, PitchMarks.Num() - MaxPitchMarks, EAllowShrinking::No);
	}
}

void UCricketBowlingDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Bowl || !GetWorld()) { return; }
	RebindBall();

	if (CVarBowlDebug.GetValueOnGameThread() == 0) { return; }

	// Maintain the actual flight trail.
	if (BallPhys && BallPhys->IsBallInFlight() && BallPhys->GetOwner())
	{
		Trail.Add(BallPhys->GetOwner()->GetActorLocation());
		if (Trail.Num() > MaxTrailPoints)
		{
			Trail.RemoveAt(0, Trail.Num() - MaxTrailPoints, EAllowShrinking::No);
		}
	}

	DrawReleaseAndSeam();
	DrawSpinAxis();
	if (CVarBowlSwing.GetValueOnGameThread() != 0) { DrawAimAndSwing(); }
	DrawActualTrajectory();
	DrawBouncePoints();
	if (CVarBowlMap.GetValueOnGameThread() != 0) { DrawPitchMap(); }
	if (CVarBowlReadout.GetValueOnGameThread() != 0) { DrawReadout(); }
}

void UCricketBowlingDebugComponent::DrawReleaseAndSeam() const
{
	UWorld* World = GetWorld();
	const FVector ReleaseCm = Bowl->GetReleaseWorldCm();

	// Release point.
	DrawDebugSphere(World, ReleaseCm, 8.f, 12, FColor::White, false, -1.f, 0, 1.5f);

	if (!bHasPrediction) { return; }
	const FCricketReleaseParameters& P = Bowl->GetLastReleaseParams();

	// Seam plane (a ring perpendicular to the seam normal) + the normal stub.
	const FVector N = P.SeamNormal.GetSafeNormal();
	if (N.IsNearlyZero()) { return; }
	FVector U = FVector::CrossProduct(N, FVector::UpVector);
	if (U.IsNearlyZero()) { U = FVector::CrossProduct(N, FVector::ForwardVector); }
	U = U.GetSafeNormal();
	const FVector V = FVector::CrossProduct(N, U).GetSafeNormal();
	DrawDebugCircle(World, ReleaseCm, 16.f, 32, FColor::Yellow, false, -1.f, 0, 1.4f, U, V, false);
	DrawDebugLine(World, ReleaseCm, ReleaseCm + N * 22.f, FColor::Orange, false, -1.f, 0, 1.6f);
}

void UCricketBowlingDebugComponent::DrawSpinAxis() const
{
	if (!bHasPrediction) { return; }
	UWorld* World = GetWorld();
	const FCricketReleaseParameters& P = Bowl->GetLastReleaseParams();
	const FVector ReleaseCm = Bowl->GetReleaseWorldCm();
	const FVector Axis = P.SpinAxis.GetSafeNormal();
	if (Axis.IsNearlyZero()) { return; }

	DrawDebugLine(World, ReleaseCm - Axis * 26.f, ReleaseCm + Axis * 26.f, FColor::Magenta, false, -1.f, 0, 2.2f);
	DrawDebugString(World, ReleaseCm + Axis * 30.f,
		FString::Printf(TEXT("%.0f rpm"), P.SpinRateRPM), nullptr, FColor::Magenta, 0.f);
}

void UCricketBowlingDebugComponent::DrawAimAndSwing() const
{
	if (!bHasPrediction) { return; }
	UWorld* World = GetWorld();
	const FVector ReleaseCm = Bowl->GetReleaseWorldCm();

	// Straight reference chord release -> predicted pitch: the curve's departure
	// from this line is the swing/dip.
	DrawDebugLine(World, ReleaseCm, PredictedPitchCm, FColor(120, 120, 120), false, -1.f, 0, 1.2f);

	// The predicted (curved) path through the real model.
	for (int32 i = 1; i < PredictedPathCm.Num(); ++i)
	{
		DrawDebugLine(World, PredictedPathCm[i - 1], PredictedPathCm[i], FColor::Emerald, false, -1.f, 0, 1.4f);
	}

	// Predicted pitch point.
	DrawDebugSphere(World, PredictedPitchCm, 7.f, 12, FColor::Yellow, false, -1.f, 0, 1.5f);

	const FCricketDeliveryDiagnostics& D = Bowl->GetLastDiagnostics();
	DrawDebugString(World, PredictedPitchCm + FVector(0, 0, 25.f),
		FString::Printf(TEXT("swing %.2f m"), D.FreeFlightSwingM), nullptr, FColor::Emerald, 0.f);
}

void UCricketBowlingDebugComponent::DrawActualTrajectory() const
{
	UWorld* World = GetWorld();
	for (int32 i = 1; i < Trail.Num(); ++i)
	{
		DrawDebugLine(World, Trail[i - 1], Trail[i], FColor::White, false, -1.f, 0, 2.f);
	}
}

void UCricketBowlingDebugComponent::DrawBouncePoints() const
{
	if (PitchMarks.Num() == 0) { return; }
	UWorld* World = GetWorld();
	// Highlight the most recent actual landing.
	const FPitchMark& Last = PitchMarks.Last();
	DrawDebugSphere(World, Last.LocationCm, 9.f, 14, FColor::Red, false, -1.f, 0, 2.f);
}

void UCricketBowlingDebugComponent::DrawPitchMap() const
{
	UWorld* World = GetWorld();
	const FVector ReleaseCm = Bowl->GetReleaseWorldCm();
	const FVector StrikerCm = Bowl->GetStrikerStumpsWorldCm();

	FVector Fwd, Lat;
	HorizFrame(ReleaseCm, StrikerCm, Fwd, Lat);

	const FVector Ground = StrikerCm; // the stumps sit on the pitch; draw zones at that ground height
	const double HalfW = 1.5 * MetersToUE;

	// Length-zone bands (m from the striker, back toward the bowler).
	struct FZone { double A, B; };
	static const FZone Zones[] = { {0,2},{2,4},{4,7},{7,9},{9,11},{11,14} };
	for (const FZone& Z : Zones)
	{
		const FVector Near = Ground - Fwd * (Z.A * MetersToUE);
		const FVector Far  = Ground - Fwd * (Z.B * MetersToUE);
		const FColor C = ColorForLength(0.5 * (Z.A + Z.B));
		const FVector C1 = Near - Lat * HalfW, C2 = Near + Lat * HalfW;
		const FVector C3 = Far + Lat * HalfW,  C4 = Far - Lat * HalfW;
		DrawDebugLine(World, C1, C2, C, false, -1.f, 0, 1.f);
		DrawDebugLine(World, C2, C3, C, false, -1.f, 0, 1.f);
		DrawDebugLine(World, C3, C4, C, false, -1.f, 0, 1.f);
		DrawDebugLine(World, C4, C1, C, false, -1.f, 0, 1.f);
	}

	// Stumps at the striker's end (three uprights across the stump width).
	const double Spacing = CricketField::StumpHalfSpacingM * MetersToUE;
	const double StumpH = CricketField::StumpHeightM * MetersToUE;
	for (int32 s = -1; s <= 1; ++s)
	{
		const FVector Base = Ground + Lat * (Spacing * s);
		DrawDebugLine(World, Base, Base + FVector(0, 0, StumpH), FColor::White, false, -1.f, 0, 2.f);
	}

	// Landing marks, colour-coded by length.
	for (const FPitchMark& M : PitchMarks)
	{
		DrawDebugSphere(World, M.LocationCm, 5.f, 10, ColorForLength(M.LengthM), false, -1.f, 0, 1.5f);
	}
}

void UCricketBowlingDebugComponent::DrawReadout() const
{
	if (!GEngine || !bHasPrediction) { return; }

	const FCricketReleaseParameters& P = Bowl->GetLastReleaseParams();
	const FCricketDeliveryDiagnostics& D = Bowl->GetLastDiagnostics();
	const FCricketBowlingIntent& In = Bowl->GetIntent();

	auto EnumStr = [](const UEnum* E, int64 V) { return E ? E->GetDisplayNameTextByValue(V).ToString() : TEXT("?"); };
	const FString MoveStr   = EnumStr(StaticEnum<ECricketMovement>(), (int64)In.Movement);
	const FString LenStr    = EnumStr(StaticEnum<ECricketLength>(), (int64)In.Length);
	const FString LineStr   = EnumStr(StaticEnum<ECricketLine>(), (int64)In.Line);
	const FString WristStr  = EnumStr(StaticEnum<ECricketWristPosition>(), (int64)P.WristPosition);
	const FString RegimeStr = EnumStr(StaticEnum<ECricketSwingRegime>(), (int64)D.Regime);

	auto Line = [&](int32 Key, const FColor& C, const FString& Text)
	{
		GEngine->AddOnScreenDebugMessage(Key, 0.f, C, Text);
	};

	Line(3001, FColor::Cyan,    FString::Printf(TEXT("[BOWLING] %s | %s | line %s"), *MoveStr, *LenStr, *LineStr));
	Line(3002, FColor::White,   FString::Printf(TEXT("Pace: %.1f km/h  elev %.1f  azim %.1f  wrist %s"),
		MsToKmh(P.ReleaseSpeedMS), P.ReleaseElevationDeg, P.ReleaseAzimuthDeg, *WristStr));
	Line(3003, FColor::Magenta, FString::Printf(TEXT("Spin: %.0f rpm  seam-stability %.2f"), P.SpinRateRPM, P.SeamStability));
	Line(3004, FColor::Yellow,  FString::Printf(TEXT("Ball: shine %.2f rough %.2f seam %.2f | regime %s"),
		P.BallCondition.ShineAsymmetry, P.BallCondition.Roughness, P.BallCondition.SeamProudness, *RegimeStr));
	Line(3005, FColor::Emerald, FString::Printf(TEXT("Predicted: length %.2f m  line %.2f m  swing %.2f m  %s"),
		D.PredictedLengthM, D.PredictedLineAtPitchM, D.FreeFlightSwingM,
		D.bAimConverged ? TEXT("(aim ok)") : TEXT("(aim clamped)")));
}
