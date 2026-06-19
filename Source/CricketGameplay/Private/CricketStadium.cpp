#include "CricketStadium.h"
#include "CricketStadiumModel.h"
#include "CricketBall.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketPitchInteraction.h"
#include "CricketBowlingTypes.h" // CricketField:: dimensions (pitch length)
#include "CricketPhysicsConstants.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

using namespace CricketPhysics;

namespace
{
	TAutoConsoleVariable<int32> CVarStadiumDebug(TEXT("cricket.Debug.Stadium"), 0,
		TEXT("Stadium environment debug visualization. 0=off, 1=on"));

	FColor ResultColor(ECricketBoundaryResult R)
	{
		switch (R)
		{
		case ECricketBoundaryResult::Six:  return FColor::Red;
		case ECricketBoundaryResult::Four: return FColor::Cyan;
		default:                           return FColor::Green;
		}
	}
}

ACricketStadium::ACricketStadium()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void ACricketStadium::BeginPlay()
{
	Super::BeginPlay();

	if (Field.Positions.Num() == 0) { Field = FCricketStadiumModel::DefaultField(); }

	if (!Ball.IsValid())
	{
		for (TActorIterator<ACricketBall> It(GetWorld()); It; ++It) { Ball = *It; break; }
	}
	if (UCricketBallPhysicsComponent* BP = BallPhysics())
	{
		BP->OnBounce.AddDynamic(this, &ACricketStadium::HandleBounce);
	}
	ApplyEnvironmentToBall();
}

void ACricketStadium::SetBall(ACricketBall* InBall)
{
	Ball = InBall;
	if (UCricketBallPhysicsComponent* BP = BallPhysics())
	{
		BP->OnBounce.AddDynamic(this, &ACricketStadium::HandleBounce);
	}
}

UCricketBallPhysicsComponent* ACricketStadium::BallPhysics() const
{
	return Ball.IsValid() ? Ball->GetBallPhysics() : nullptr;
}

FCricketGroundDimensions ACricketStadium::GetDimensions() const
{
	FCricketGroundDimensions D;
	D.CenterM = WorldToMeters(GetActorLocation());
	FVector Fwd = GetActorForwardVector(); Fwd.Z = 0.0;
	D.PitchAxis = Fwd.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
	D.StraightBoundaryM = StraightBoundaryM;
	D.SquareBoundaryM = SquareBoundaryM;
	D.RopeHeightM = RopeHeightM;
	D.PitchLengthM = CricketField::PitchLengthM;
	return D;
}

FVector ACricketStadium::GetFieldPositionWorldCm(ECricketFieldPosition Position) const
{
	return MetersToWorld(FCricketStadiumModel::FieldPositionWorldM(GetDimensions(), Position, bRightHandedBatter));
}

FVector ACricketStadium::GetStrikerStumpsCm() const { return MetersToWorld(GetDimensions().StrikerStumpsM()); }
FVector ACricketStadium::GetBowlerStumpsCm() const { return MetersToWorld(GetDimensions().BowlerStumpsM()); }

bool ACricketStadium::IsInsideBoundaryCm(const FVector& WorldCm) const
{
	return FCricketStadiumModel::IsInsideBoundary(GetDimensions(), WorldToMeters(WorldCm));
}

ECricketBoundaryResult ACricketStadium::ValidateBoundaryCatchCm(const FVector& WorldCm) const
{
	return FCricketStadiumModel::ValidateBoundaryCatch(GetDimensions(), WorldToMeters(WorldCm));
}

void ACricketStadium::ApplyEnvironmentToBall()
{
	if (UCricketBallPhysicsComponent* BP = BallPhysics())
	{
		// Push the venue atmosphere onto the ball — wind/humidity/pressure are read by
		// the aerodynamics, so this is a real effect on flight (applied next release).
		BP->Environment = Environment.Atmosphere;
	}
}

void ACricketStadium::SetTimeOfDay(ECricketTimeOfDay TimeOfDay)
{
	Environment.TimeOfDay = TimeOfDay;
	Environment.bFloodlightsOn = (TimeOfDay != ECricketTimeOfDay::Day);
}

void ACricketStadium::SetWindMS(const FVector& WindMS)
{
	Environment.Atmosphere.Wind = WindMS;
	ApplyEnvironmentToBall();
}

void ACricketStadium::HandleBounce(FCricketBounceReport)
{
	UCricketBallPhysicsComponent* BP = BallPhysics();
	if (!BP || bBoundaryAwarded) { return; }

	const FVector BallM = BP->GetState().Position;
	if (!bHasFirstBounce)
	{
		FirstBounceCm = MetersToWorld(BallM);
		bHasFirstBounce = true;
	}
	if (FCricketStadiumModel::IsInsideBoundary(GetDimensions(), BallM))
	{
		bBouncedInside = true;
	}
}

void ACricketStadium::UpdateBoundaryDetection()
{
	UCricketBallPhysicsComponent* BP = BallPhysics();
	const bool bLive = BP && BP->IsBallInFlight();
	const FCricketGroundDimensions D = GetDimensions();

	if (bLive && !bPrevBallLive)
	{
		bBouncedInside = false;
		bBoundaryAwarded = false;
		bHasFirstBounce = false;
	}

	if (bLive && !bBoundaryAwarded)
	{
		const FVector BallM = BP->GetState().Position;
		if (!FCricketStadiumModel::IsInsideBoundary(D, BallM))
		{
			const ECricketBoundaryResult Result = bBouncedInside ? ECricketBoundaryResult::Four : ECricketBoundaryResult::Six;
			const FVector CrossingCm = MetersToWorld(BallM);
			bBoundaryAwarded = true;
			Shots.Add({ bHasFirstBounce ? FirstBounceCm : CrossingCm, CrossingCm, Result });
			if (Shots.Num() > 256) { Shots.RemoveAt(0, 1, EAllowShrinking::No); }
			OnBoundaryEvent.Broadcast(Result, CrossingCm);
			BP->Freeze(); // ball retrieval: it is dead once it crosses the rope
		}
	}
	else if (!bLive && bPrevBallLive && !bBoundaryAwarded && bHasFirstBounce)
	{
		// The ball stopped inside the field — in play (fielded), not a boundary.
		bBoundaryAwarded = true;
		Shots.Add({ FirstBounceCm, FirstBounceCm, ECricketBoundaryResult::InPlay });
	}

	bPrevBallLive = bLive;
}

void ACricketStadium::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateBoundaryDetection();
	DrawStadium();
}

void ACricketStadium::DrawStadium() const
{
	UWorld* W = GetWorld();
	if (!W || CVarStadiumDebug.GetValueOnGameThread() == 0) { return; }

	const FCricketGroundDimensions D = GetDimensions();
	const FVector Axis = D.PitchAxis.GetSafeNormal();
	const FVector Side = D.SideAxis();
	const FVector CenterCm = MetersToWorld(D.CenterM);

	auto BoundaryAtAngle = [&](double A, double Scale) -> FVector
	{
		const FVector Dir = Axis * FMath::Cos(A) + Side * FMath::Sin(A);
		const double R = FCricketStadiumModel::BoundaryRadiusAtAngleM(D, A) * Scale;
		return MetersToWorld(D.CenterM + Dir * R) + FVector(0, 0, D.RopeHeightM * MetersToUE);
	};

	// Boundary rope + the catch zone band just inside it.
	const int32 N = 72;
	for (int32 i = 0; i < N; ++i)
	{
		const double A0 = 2.0 * PI * i / N, A1 = 2.0 * PI * (i + 1) / N;
		DrawDebugLine(W, BoundaryAtAngle(A0, 1.0), BoundaryAtAngle(A1, 1.0), FColor::White, false, -1.f, 0, 3.f);
		DrawDebugLine(W, BoundaryAtAngle(A0, 0.96), BoundaryAtAngle(A1, 0.96), FColor(90, 90, 90), false, -1.f, 0, 1.f);
	}

	// 30-yard inner ring (27.43 m).
	DrawDebugCircle(W, CenterCm, 27.43f * (float)MetersToUE, 64, FColor(170, 170, 0), false, -1.f, 0, 1.5f,
		FVector(1, 0, 0), FVector(0, 1, 0), false);

	// The pitch.
	const FVector S = GetStrikerStumpsCm(), B = GetBowlerStumpsCm();
	const FVector HalfW = Side * (1.52 * MetersToUE);
	DrawDebugLine(W, S - HalfW, B - HalfW, FColor::Silver, false, -1.f, 0, 2.f);
	DrawDebugLine(W, S + HalfW, B + HalfW, FColor::Silver, false, -1.f, 0, 2.f);
	DrawDebugLine(W, S - HalfW, S + HalfW, FColor::Silver, false, -1.f, 0, 2.f);
	DrawDebugLine(W, B - HalfW, B + HalfW, FColor::Silver, false, -1.f, 0, 2.f);

	// Fielding positions.
	for (ECricketFieldPosition P : Field.Positions)
	{
		const FVector Pos = GetFieldPositionWorldCm(P);
		DrawDebugSphere(W, Pos + FVector(0, 0, 30), 22.f, 10, FColor(0, 150, 255), false, -1.f, 0, 1.5f);
		DrawDebugString(W, Pos + FVector(0, 0, 120),
			StaticEnum<ECricketFieldPosition>()->GetDisplayNameTextByValue((int64)P).ToString(), nullptr, FColor::White, 0.f, false, 1.0f);
	}

	// Sight screens beyond each straight boundary.
	auto SightScreen = [&](const FVector& AtCm)
	{
		DrawDebugBox(W, AtCm + FVector(0, 0, 300), FVector(20, 400, 300), Axis.Rotation().Quaternion(), FColor::White, false, -1.f, 0, 2.f);
	};
	SightScreen(MetersToWorld(D.CenterM + Axis * D.StraightBoundaryM));
	SightScreen(MetersToWorld(D.CenterM - Axis * D.StraightBoundaryM));

	// Umpires: bowler's end (behind the stumps) and square leg (square of the striker).
	const FVector LegDir = bRightHandedBatter ? -Side : Side;
	DrawDebugSphere(W, B + Axis * (1.5 * MetersToUE) + FVector(0, 0, 95), 35.f, 8, FColor::Black, false, -1.f, 0, 2.f);
	DrawDebugString(W, B + Axis * (1.5 * MetersToUE) + FVector(0, 0, 200), TEXT("umpire"), nullptr, FColor::Silver, 0.f, false, 0.9f);
	DrawDebugSphere(W, S + LegDir * (18.0 * MetersToUE) + FVector(0, 0, 95), 35.f, 8, FColor::Black, false, -1.f, 0, 2.f);
	DrawDebugString(W, S + LegDir * (18.0 * MetersToUE) + FVector(0, 0, 200), TEXT("square-leg umpire"), nullptr, FColor::Silver, 0.f, false, 0.9f);

	// Team area / pavilion beyond the square boundary.
	const FVector Pavilion = MetersToWorld(D.CenterM + Side * (D.SquareBoundaryM + 10.0));
	DrawDebugBox(W, Pavilion + FVector(0, 0, 400), FVector(1000, 200, 400), Axis.Rotation().Quaternion(), FColor(120, 80, 40), false, -1.f, 0, 2.f);
	DrawDebugString(W, Pavilion + FVector(0, 0, 850), TEXT("team area"), nullptr, FColor::Silver, 0.f, false, 1.0f);

	// Heatmap + shot distribution (wagon wheel from the striker).
	for (const FShotRecord& Shot : Shots)
	{
		DrawDebugSphere(W, Shot.LandingCm + FVector(0, 0, 10), 18.f, 10, ResultColor(Shot.Result), false, -1.f, 0, 2.f);
		if (Shot.Result == ECricketBoundaryResult::Four || Shot.Result == ECricketBoundaryResult::Six)
		{
			DrawDebugLine(W, S, Shot.CrossingCm, ResultColor(Shot.Result), false, -1.f, 0, 1.5f);
		}
	}

	if (GEngine)
	{
		int32 Fours = 0, Sixes = 0;
		for (const FShotRecord& Shot : Shots) { if (Shot.Result == ECricketBoundaryResult::Four) { ++Fours; } else if (Shot.Result == ECricketBoundaryResult::Six) { ++Sixes; } }
		const FString TodStr = StaticEnum<ECricketTimeOfDay>()->GetDisplayNameTextByValue((int64)Environment.TimeOfDay).ToString();
		GEngine->AddOnScreenDebugMessage(9000, 0.f, FColor::White,
			FString::Printf(TEXT("Stadium: straight %.0fm  square %.0fm  | %s%s | wind %.0f m/s"),
				StraightBoundaryM, SquareBoundaryM, *TodStr, Environment.bFloodlightsOn ? TEXT(" (lights)") : TEXT(""),
				Environment.Atmosphere.Wind.Size()));
		GEngine->AddOnScreenDebugMessage(9001, 0.f, FColor::Cyan,
			FString::Printf(TEXT("Shots: %d  (4s: %d  6s: %d)  — wagon wheel + landing heatmap"), Shots.Num(), Fours, Sixes));
	}
}
