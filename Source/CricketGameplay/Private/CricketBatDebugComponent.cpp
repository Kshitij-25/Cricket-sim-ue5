#include "CricketBatDebugComponent.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketPhysicsConstants.h"
#include "CricketTrajectoryPredictor.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

namespace
{
	TAutoConsoleVariable<int32> CVarBatDebug(TEXT("cricket.Debug.Bat"), 1,
		TEXT("Bat-ball impact debug visualization. 0=off, 1=on"));

	FColor RegionColor(ECricketContactRegion Region)
	{
		switch (Region)
		{
		case ECricketContactRegion::Middle: return FColor::Green;
		case ECricketContactRegion::ThinEdge: return FColor::Red;
		case ECricketContactRegion::ThickEdge: return FColor::Orange;
		case ECricketContactRegion::TopEdge: return FColor::Yellow;
		case ECricketContactRegion::BottomEdge: return FColor::Yellow;
		case ECricketContactRegion::Toe: return FColor::Purple;
		default: return FColor::White;
		}
	}
}

UCricketBatDebugComponent::UCricketBatDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UCricketBatDebugComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Ball = Owner->FindComponentByClass<UCricketBallPhysicsComponent>();
		if (Ball)
		{
			Ball->OnBatImpact.AddDynamic(this, &UCricketBatDebugComponent::HandleBatImpact);
		}
	}
}

void UCricketBatDebugComponent::HandleBatImpact(FCricketBatImpactReport Report)
{
	if (!Ball) { return; }

	LastReport = Report;
	LastBat = Ball->GetLastBatState();
	LastContactCm = Ball->GetLastBatContactCm();
	TimeSinceImpact = 0.f;
	bHasImpact = true;

	// Predict the post-impact flight ONCE, from the launch state, and cache it
	// in world cm so the path stays put while it's displayed.
	FCricketBallState Post;
	Post.Position = WorldToMeters(LastContactCm);
	Post.Velocity = Report.OutgoingVelocity;
	Post.AngularVelocity = Report.OutgoingSpin;

	FCricketPredictionParams Params;
	Params.MaxTime = PredictionSeconds;
	Params.PitchPlaneZ = 0.0;
	Params.bResolveBounces = true;
	const FCricketTrajectoryPrediction Pred =
		FCricketTrajectoryPredictor::Predict(Post, Ball->GetIntegrator(), Params);

	PredictedPathCm.Reset();
	for (const FCricketTrajectorySample& S : Pred.Samples)
	{
		PredictedPathCm.Add(MetersToWorld(S.Position));
	}
	PredictedBouncesCm.Reset();
	for (const FVector& B : Pred.BouncePoints)
	{
		PredictedBouncesCm.Add(MetersToWorld(B));
	}
}

void UCricketBatDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Ball || !GetWorld() || !bHasImpact) { return; }
	if (CVarBatDebug.GetValueOnGameThread() == 0) { return; }

	TimeSinceImpact += DeltaTime;
	if (TimeSinceImpact > DisplaySeconds) { return; }

	DrawBatFace(LastBat);
	DrawImpact();
	DrawReadout();
}

void UCricketBatDebugComponent::DrawBatFace(const FCricketBatState& Bat) const
{
	const FCricketBatProfile& P = DebugBatProfile;
	const double HalfW = P.BladeWidthM * 0.5;

	// Blade rectangle in SI, then to cm.
	const FVector Toe = Bat.SweetSpotLocation - Bat.LongAxis * P.SweetSpotFromToeM;
	const FVector Top = Bat.SweetSpotLocation + Bat.LongAxis * (P.BladeLengthM - P.SweetSpotFromToeM);
	const FVector W = Bat.WidthAxis * HalfW;

	auto CM = [](const FVector& M) { return MetersToWorld(M); };
	const FVector C1 = CM(Toe - W), C2 = CM(Toe + W), C3 = CM(Top + W), C4 = CM(Top - W);
	DrawDebugLine(GetWorld(), C1, C2, FColor::Silver, false, -1.f, 0, 1.5f);
	DrawDebugLine(GetWorld(), C2, C3, FColor::Silver, false, -1.f, 0, 1.5f);
	DrawDebugLine(GetWorld(), C3, C4, FColor::Silver, false, -1.f, 0, 1.5f);
	DrawDebugLine(GetWorld(), C4, C1, FColor::Silver, false, -1.f, 0, 1.5f);

	// Sweet-spot zone (green circle in the face plane).
	DrawDebugCircle(GetWorld(), CM(Bat.SweetSpotLocation),
		static_cast<float>(P.SweetSpotRadiusM * MetersToUE), 24, FColor::Green, false, -1.f, 0, 1.5f,
		Bat.LongAxis, Bat.WidthAxis, false);

	// Edge zones: the outer width bands (beyond 55% of half-width).
	const FVector EdgeOff = Bat.WidthAxis * (HalfW * 0.55);
	DrawDebugLine(GetWorld(), CM(Toe + EdgeOff), CM(Top + EdgeOff), FColor::Red, false, -1.f, 0, 1.f);
	DrawDebugLine(GetWorld(), CM(Toe - EdgeOff), CM(Top - EdgeOff), FColor::Red, false, -1.f, 0, 1.f);

	// Bat angle (face normal) and swing path (bat velocity).
	const FVector ContactCm = LastContactCm;
	DrawDebugDirectionalArrow(GetWorld(), ContactCm, ContactCm + Bat.FaceNormal * 22.f, 10.f,
		FColor::Cyan, false, -1.f, 0, 2.f); // bat angle / face normal
	const FVector SwingCm = CM(Bat.SweetSpotLocation);
	DrawDebugDirectionalArrow(GetWorld(), SwingCm, SwingCm + Bat.LinearVelocity * 4.f, 10.f,
		FColor(0, 200, 255), false, -1.f, 0, 2.f); // swing path
}

void UCricketBatDebugComponent::DrawImpact() const
{
	// Contact point, coloured by region.
	DrawDebugSphere(GetWorld(), LastContactCm, 4.f, 12, RegionColor(LastReport.Region), false, -1.f, 0, 2.f);

	// Exit velocity vector (length ~ exit speed).
	const FVector ExitDirCm = LastReport.OutgoingVelocity * 4.f;
	DrawDebugDirectionalArrow(GetWorld(), LastContactCm, LastContactCm + ExitDirCm, 14.f,
		FColor::Orange, false, -1.f, 0, 2.5f);

	// Predicted post-impact path.
	for (int32 i = 1; i < PredictedPathCm.Num(); ++i)
	{
		DrawDebugLine(GetWorld(), PredictedPathCm[i - 1], PredictedPathCm[i], FColor::Emerald, false, -1.f, 0, 1.5f);
	}
	for (const FVector& B : PredictedBouncesCm)
	{
		DrawDebugSphere(GetWorld(), B, 6.f, 10, FColor::Yellow, false, -1.f, 0, 1.f);
	}
}

void UCricketBatDebugComponent::DrawReadout() const
{
	if (!GEngine) { return; }

	const UEnum* RegionEnum = StaticEnum<ECricketContactRegion>();
	const UEnum* SideEnum = StaticEnum<ECricketContactSide>();
	const FString RegionStr = RegionEnum ? RegionEnum->GetDisplayNameTextByValue((int64)LastReport.Region).ToString() : TEXT("?");
	const FString SideStr = SideEnum ? SideEnum->GetDisplayNameTextByValue((int64)LastReport.Side).ToString() : TEXT("?");

	auto Line = [&](int32 Key, const FColor& C, const FString& Text)
	{
		GEngine->AddOnScreenDebugMessage(Key, 0.f, C, Text);
	};

	Line(2001, FColor::White,  FString::Printf(TEXT("Contact:  %s / %s (edge %.2f, quality %.2f)"),
		*RegionStr, *SideStr, LastReport.EdgeFactor, LastReport.Quality));
	Line(2002, FColor::Orange, FString::Printf(TEXT("Exit:     %.1f km/h  (in %.1f km/h)"),
		MsToKmh(LastReport.ExitSpeedMS), MsToKmh(LastReport.IncomingSpeedMS)));
	Line(2003, FColor::Cyan,   FString::Printf(TEXT("Launch:   %.1f deg | Deflection %.1f deg"),
		LastReport.LaunchAngleDeg, LastReport.DeflectionAngleDeg));
	Line(2004, FColor::Magenta,FString::Printf(TEXT("Spin xfer:%.1f rad/s"), LastReport.SpinTransferRadS));
	Line(2005, FColor::Yellow, FString::Printf(TEXT("Energy:   %.0f%% to ball (eff mass %.2f kg, e %.2f)"),
		LastReport.EnergyTransferFraction * 100.0, LastReport.EffectiveMassKg, LastReport.RestitutionUsed));
}
