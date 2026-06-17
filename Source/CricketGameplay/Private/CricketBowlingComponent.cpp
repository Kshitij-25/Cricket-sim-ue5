#include "CricketBowlingComponent.h"
#include "CricketBall.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketBowlingActionAsset.h"
#include "CricketDeliveryGenerator.h"
#include "CricketPhysicsConstants.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

using namespace CricketPhysics;

UCricketBowlingComponent::UCricketBowlingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// A sensible default bowler so the component works the instant it is added.
	Action = UCricketBowlingActionAsset::MakeExpressQuick();
}

void UCricketBowlingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ActionAsset)
	{
		SetActionAsset(ActionAsset);
	}

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

	if (!bStrikerExplicit)
	{
		const AActor* Owner = GetOwner();
		const FVector Loc = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
		FVector Fwd = Owner ? Owner->GetActorForwardVector() : FVector(1, 0, 0);
		Fwd.Z = 0.0;
		Fwd = Fwd.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
		StrikerStumpsWorldCm = Loc + Fwd * (Action.ReleaseToStumpsM * MetersToUE);
		StrikerStumpsWorldCm.Z = Loc.Z; // stumps sit on the ground plane
	}
}

void UCricketBowlingComponent::SetTargetBall(ACricketBall* InBall)
{
	TargetBall = InBall;
}

void UCricketBowlingComponent::SetActionAsset(UCricketBowlingActionAsset* InAsset)
{
	if (!InAsset)
	{
		return;
	}
	ActionAsset = InAsset;
	SetAction(InAsset->Action);
}

void UCricketBowlingComponent::SetAction(const FCricketBowlingAction& InAction)
{
	Action = InAction;
	CurrentIntent.Arm = Action.Arm;
	if (Action.Presets.Num() > 0)
	{
		SelectPreset(0);
	}
}

UCricketBallPhysicsComponent* UCricketBowlingComponent::GetTargetBallPhysics() const
{
	return TargetBall.IsValid() ? TargetBall->GetBallPhysics() : nullptr;
}

FVector UCricketBowlingComponent::ComputeReleaseWorldCm() const
{
	const AActor* Owner = GetOwner();
	const FVector Loc = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;
	FVector Fwd = Owner ? Owner->GetActorForwardVector() : FVector(1, 0, 0);
	Fwd.Z = 0.0;
	Fwd = Fwd.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
	const FVector Lat = FVector(-Fwd.Y, Fwd.X, 0.0).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));

	double WidthSign = (CurrentIntent.Side == ECricketDeliverySide::OverTheWicket) ? 1.0 : -1.0;
	if (Action.Arm == ECricketBowlingArm::LeftArm)
	{
		WidthSign = -WidthSign;
	}

	return Loc
		+ FVector(0.0, 0.0, Action.ReleaseHeightM * MetersToUE)
		+ Lat * (Action.ReleaseWidthM * MetersToUE * WidthSign);
}

FVector UCricketBowlingComponent::GetReleaseWorldCm() const
{
	return ComputeReleaseWorldCm();
}

double UCricketBowlingComponent::GetGroundPlaneZM() const
{
	// The striker's stumps sit on the pitch, so their Z is the bounce-plane height.
	return StrikerStumpsWorldCm.Z * UEToMeters;
}

void UCricketBowlingComponent::StepLength(int32 Dir)
{
	constexpr int32 Count = static_cast<int32>(ECricketLength::Bouncer) + 1;
	int32 Idx = static_cast<int32>(CurrentIntent.Length) + FMath::Clamp(Dir, -1, 1);
	CurrentIntent.Length = static_cast<ECricketLength>(FMath::Clamp(Idx, 0, Count - 1));
}

void UCricketBowlingComponent::StepLine(int32 Dir)
{
	// Lines run off (index 0) -> leg (last). Dir>0 means "toward off" => lower index.
	constexpr int32 Count = static_cast<int32>(ECricketLine::DownLeg) + 1;
	int32 Idx = static_cast<int32>(CurrentIntent.Line) - FMath::Clamp(Dir, -1, 1);
	CurrentIntent.Line = static_cast<ECricketLine>(FMath::Clamp(Idx, 0, Count - 1));
}

void UCricketBowlingComponent::SelectPreset(int32 Index)
{
	if (Action.Presets.Num() == 0)
	{
		return;
	}
	const int32 Clamped = FMath::Clamp(Index, 0, Action.Presets.Num() - 1);
	Action.Presets[Clamped].ApplyTo(CurrentIntent);
}

FCricketReleaseParameters UCricketBowlingComponent::BowlNow()
{
	UCricketBallPhysicsComponent* Phys = GetTargetBallPhysics();

	const FVector ReleaseCm = ComputeReleaseWorldCm();

	FCricketDeliveryContext Ctx;
	Ctx.ReleasePositionM = WorldToMeters(ReleaseCm);
	Ctx.StrikerStumpsM   = WorldToMeters(StrikerStumpsWorldCm);
	Ctx.GroundPlaneZM    = Ctx.StrikerStumpsM.Z; // bounce plane = the floor the live ball sweeps
	Ctx.Environment      = Environment;
	Ctx.AimPitchSurface  = PitchSurface;
	Ctx.BallCondition    = BallCondition;
	Ctx.Seed             = DeliverySeed;
	Ctx.HumanScatter     = HumanScatter;

	FCricketDeliveryDiagnostics Diag;
	const FCricketReleaseParameters P =
		FCricketDeliveryGenerator::Generate(CurrentIntent, Action, Ctx, &Diag);

	LastReleaseParams = P;
	LastDiagnostics   = Diag;
	++DeliverySeed;

	if (Phys)
	{
		// Push the delivery's physical condition onto the ball, then release it.
		Phys->Coefficients = P.Coefficients;
		Phys->Surface      = P.BallCondition;
		Phys->Environment  = Environment;
		Phys->PitchSurface = PitchSurface;
		Phys->ReleaseEx(MetersToWorld(P.ReleasePositionM), P.ReleaseVelocityMS,
			P.AngularVelocityRadS, P.SeamNormal, P.SeamStability);
	}

	OnDelivery.Broadcast(P, Diag);
	return P;
}

void UCricketBowlingComponent::AgeBall(double WearDelta)
{
	const double W = FMath::Clamp(WearDelta, -1.0, 1.0);
	BallCondition.Roughness    = FMath::Clamp(BallCondition.Roughness + W, 0.0, 1.0);
	BallCondition.SeamProudness = FMath::Clamp(BallCondition.SeamProudness - 0.4 * W, 0.0, 1.0);
	// Keep the shiny side's polarity; bleed its magnitude toward symmetric.
	const double ShineMag = FMath::Clamp(FMath::Abs(BallCondition.ShineAsymmetry) - 0.3 * W, 0.0, 1.0);
	BallCondition.ShineAsymmetry = FMath::Sign(BallCondition.ShineAsymmetry == 0.0 ? 1.0 : BallCondition.ShineAsymmetry) * ShineMag;
}

void UCricketBowlingComponent::ResetBall()
{
	BallCondition = FCricketBallSurface();
}
