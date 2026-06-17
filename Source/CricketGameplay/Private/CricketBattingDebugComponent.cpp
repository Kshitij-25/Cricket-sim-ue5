#include "CricketBattingDebugComponent.h"
#include "CricketBattingComponent.h"
#include "CricketPhysicsConstants.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

namespace
{
	TAutoConsoleVariable<int32> CVarBattingDebug(TEXT("cricket.Debug.Batting"), 1,
		TEXT("Batting motion debug visualization. 0=off, 1=on"));

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

	FColor TimingColor(ECricketTimingQuality Q)
	{
		switch (Q)
		{
		case ECricketTimingQuality::Perfect: return FColor::Green;
		case ECricketTimingQuality::Early:
		case ECricketTimingQuality::Late: return FColor::Yellow;
		default: return FColor::Red;
		}
	}

	// Bat speed -> colour ramp (blue slow -> red fast), for the swing trail.
	FColor SpeedColor(double SpeedMS)
	{
		const float A = (float)FMath::Clamp(SpeedMS / 30.0, 0.0, 1.0);
		return FLinearColor(A, 0.25f, 1.0f - A).ToFColor(true);
	}
}

UCricketBattingDebugComponent::UCricketBattingDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCricketBattingDebugComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Batting = Owner->FindComponentByClass<UCricketBattingComponent>();
		if (Batting)
		{
			Batting->OnShotPlayed.AddDynamic(this, &UCricketBattingDebugComponent::HandleShotPlayed);
		}
	}
}

void UCricketBattingDebugComponent::HandleShotPlayed(FCricketBatImpactReport, FCricketTimingResult)
{
	TimeSinceContact = 0.f;
	bHasContact = true;
}

void UCricketBattingDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Batting || !GetWorld()) { return; }
	if (CVarBattingDebug.GetValueOnGameThread() == 0) { return; }

	TimeSinceContact += DeltaTime;

	DrawFeet();
	DrawBatAndPlane();
	DrawSwingTrail();
	if (bHasContact && TimeSinceContact <= ContactDisplaySeconds)
	{
		DrawContact();
	}
	DrawReadout();
}

void UCricketBattingDebugComponent::DrawSwingTrail() const
{
	const TArray<FVector>& Trail = Batting->GetSwingTrailCm();
	for (int32 i = 1; i < Trail.Num(); ++i)
	{
		// Colour each segment by how far along the swing it is (proxy for speed).
		const double Frac = (double)i / FMath::Max(Trail.Num() - 1, 1);
		DrawDebugLine(GetWorld(), Trail[i - 1], Trail[i], SpeedColor(Frac * 28.0), false, -1.f, 0, 2.0f);
	}
}

void UCricketBattingDebugComponent::DrawBatAndPlane() const
{
	const FCricketBatState& Bat = Batting->GetCurrentBatState();
	const FCricketBatProfile& P = Batting->GetBatProfile();

	const FVector SweetCm = MetersToWorld(Bat.SweetSpotLocation);
	const double HalfW = P.BladeWidthM * 0.5;
	const FVector Toe = Bat.SweetSpotLocation - Bat.LongAxis * P.SweetSpotFromToeM;
	const FVector Top = Bat.SweetSpotLocation + Bat.LongAxis * (P.BladeLengthM - P.SweetSpotFromToeM);
	const FVector W = Bat.WidthAxis * HalfW;

	auto CM = [](const FVector& M) { return MetersToWorld(M); };
	const FVector C1 = CM(Toe - W), C2 = CM(Toe + W), C3 = CM(Top + W), C4 = CM(Top - W);
	DrawDebugLine(GetWorld(), C1, C2, FColor::Silver, false, -1.f, 0, 1.5f);
	DrawDebugLine(GetWorld(), C2, C3, FColor::Silver, false, -1.f, 0, 1.5f);
	DrawDebugLine(GetWorld(), C3, C4, FColor::Silver, false, -1.f, 0, 1.5f);
	DrawDebugLine(GetWorld(), C4, C1, FColor::Silver, false, -1.f, 0, 1.5f);

	// Sweet-spot zone.
	DrawDebugCircle(GetWorld(), SweetCm, (float)(P.SweetSpotRadiusM * MetersToUE), 20, FColor::Green,
		false, -1.f, 0, 1.5f, Bat.LongAxis, Bat.WidthAxis, false);

	// Bat angle (face normal) and swing path (bat velocity).
	DrawDebugDirectionalArrow(GetWorld(), SweetCm, SweetCm + Bat.FaceNormal * 25.f, 10.f, FColor::Cyan, false, -1.f, 0, 2.f);
	DrawDebugDirectionalArrow(GetWorld(), SweetCm, SweetCm + Bat.LinearVelocity * 4.f, 12.f, FColor(0, 200, 255), false, -1.f, 0, 2.f);

	// Swing plane: the face plane the bat sweeps, centred on the nominal contact zone.
	const FVector ContactM = Batting->GetStanceOriginM() + Batting->GetActiveProfile().ContactOffsetM;
	const FVector PlaneC = CM(ContactM);
	const FVector U = Bat.LongAxis * (0.45 * MetersToUE);
	const FVector V = Bat.WidthAxis * (0.20 * MetersToUE);
	DrawDebugLine(GetWorld(), PlaneC - U - V, PlaneC + U - V, FColor(80, 80, 200), false, -1.f, 0, 1.f);
	DrawDebugLine(GetWorld(), PlaneC + U - V, PlaneC + U + V, FColor(80, 80, 200), false, -1.f, 0, 1.f);
	DrawDebugLine(GetWorld(), PlaneC + U + V, PlaneC - U + V, FColor(80, 80, 200), false, -1.f, 0, 1.f);
	DrawDebugLine(GetWorld(), PlaneC - U + V, PlaneC - U - V, FColor(80, 80, 200), false, -1.f, 0, 1.f);
}

void UCricketBattingDebugComponent::DrawFeet() const
{
	const FVector StanceCm = MetersToWorld(Batting->GetStanceOriginM());
	DrawDebugSphere(GetWorld(), StanceCm, 4.f, 8, FColor::White, false, -1.f, 0, 1.f);

	// Front foot strides down the pitch (-X); back foot sits behind (+X). The Y
	// split is the stance width. Highlights the foot the current footwork loads.
	const ECricketFootwork Foot = Batting->GetInput().Footwork;
	const double SideY = Batting->GetInput().bRightHanded ? 1.0 : -1.0;
	const FVector Front = StanceCm + FVector(-45.0, -15.0 * SideY, 0.0);
	const FVector Back  = StanceCm + FVector(+18.0, +15.0 * SideY, 0.0);

	const FColor FrontC = (Foot == ECricketFootwork::FrontFoot) ? FColor::Green : FColor(120, 120, 120);
	const FColor BackC  = (Foot == ECricketFootwork::BackFoot)  ? FColor::Green : FColor(120, 120, 120);
	DrawDebugBox(GetWorld(), Front, FVector(12, 6, 2), FQuat::Identity, FrontC, false, -1.f, 0, 1.5f);
	DrawDebugBox(GetWorld(), Back,  FVector(12, 6, 2), FQuat::Identity, BackC,  false, -1.f, 0, 1.5f);
}

void UCricketBattingDebugComponent::DrawContact() const
{
	const FVector ContactCm = Batting->GetLastContactCm();
	const FCricketBatImpactReport& R = Batting->GetLastReport();
	DrawDebugSphere(GetWorld(), ContactCm, 5.f, 12, RegionColor(R.Region), false, -1.f, 0, 2.5f);
	DrawDebugDirectionalArrow(GetWorld(), ContactCm, ContactCm + R.OutgoingVelocity * 4.f, 14.f,
		FColor::Orange, false, -1.f, 0, 2.5f);
}

void UCricketBattingDebugComponent::DrawReadout() const
{
	if (!GEngine) { return; }

	const UEnum* ShotEnum = StaticEnum<ECricketShotType>();
	const UEnum* FootEnum = StaticEnum<ECricketFootwork>();
	const UEnum* PhaseEnum = StaticEnum<ECricketSwingPhase>();
	const FCricketBattingInput& In = Batting->GetInput();
	const FString ShotStr = ShotEnum ? ShotEnum->GetDisplayNameTextByValue((int64)In.ShotType).ToString() : TEXT("?");
	const FString FootStr = FootEnum ? FootEnum->GetDisplayNameTextByValue((int64)In.Footwork).ToString() : TEXT("?");
	const FString PhaseStr = PhaseEnum ? PhaseEnum->GetDisplayNameTextByValue((int64)Batting->GetSwingPhase()).ToString() : TEXT("?");

	GEngine->AddOnScreenDebugMessage(3001, 0.f, FColor::White,
		FString::Printf(TEXT("Shot: %s  |  Footwork: %s  |  Aim %+.0f deg  |  Power %.0f%%"),
			*ShotStr, *FootStr, In.AimYawDeg, In.PowerScale * 100.0));
	GEngine->AddOnScreenDebugMessage(3002, 0.f, FColor(0, 200, 255),
		FString::Printf(TEXT("Phase: %s  |  Bat speed %.1f km/h"),
			*PhaseStr, MsToKmh(Batting->GetCurrentBatSpeedMS())));

	if (Batting->HasPlayed())
	{
		const FCricketTimingResult& T = Batting->GetLastTiming();
		const FCricketBatImpactReport& R = Batting->GetLastReport();
		const UEnum* TimingEnum = StaticEnum<ECricketTimingQuality>();
		const FString TimingStr = TimingEnum ? TimingEnum->GetDisplayNameTextByValue((int64)T.Quality).ToString() : TEXT("?");
		GEngine->AddOnScreenDebugMessage(3003, 0.f, TimingColor(T.Quality),
			FString::Printf(TEXT("Timing: %s  (%+.0f ms)"), *TimingStr, T.TimingErrorSec * 1000.0));
		GEngine->AddOnScreenDebugMessage(3004, 0.f, RegionColor(R.Region),
			FString::Printf(TEXT("Contact: %s  |  Exit %.1f km/h  |  Quality %.2f"),
				*StaticEnum<ECricketContactRegion>()->GetDisplayNameTextByValue((int64)R.Region).ToString(),
				MsToKmh(R.ExitSpeedMS), R.Quality));
	}
}
