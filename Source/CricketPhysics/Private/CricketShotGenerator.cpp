#include "CricketShotGenerator.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

namespace
{
	struct FShotConfig
	{
		FVector ExitDir;   // intended exit direction = bat face normal (RH, off=+Y)
		FVector ArcDir;    // in-plane swing/follow direction -> defines LongAxis & timing
		double  SpeedMS;   // nominal sweet-spot bat speed
	};

	// Per-shot nominal kinematics. Directions are normalised in use.
	FShotConfig ConfigFor(ECricketShotType Type)
	{
		switch (Type)
		{
		case ECricketShotType::DefensiveBlock:
			// Dead bat, ball dropped down in front. Low speed => low exit.
			return { FVector(-1.0, 0.0, -0.12), FVector(0, 0, 1), 4.0 };
		case ECricketShotType::StraightDrive:
			// Driven back down the ground, slightly up and through.
			return { FVector(-1.0, 0.0, 0.18), FVector(0, 0, 1), 26.0 };
		case ECricketShotType::CoverDrive:
			// Through the off side, in front of square.
			return { FVector(-0.55, 0.83, 0.12), FVector(0, 0, 1), 24.0 };
		case ECricketShotType::PullShot:
			// Short ball pulled to the leg side, higher launch. Mistime => top edge.
			return { FVector(-0.15, -0.92, 0.36), FVector(0, 0, 1), 25.0 };
		default:
			return { FVector(-1, 0, 0), FVector(0, 0, 1), 20.0 };
		}
	}
}

void FCricketShotGenerator::GenerateBatState(
	const FCricketShotIntent& Intent,
	const FCricketBallState& BallAtContact,
	const FCricketBatProfile& Profile,
	FCricketBatState& OutBat,
	FVector& OutContactPointM)
{
	FShotConfig Cfg = ConfigFor(Intent.ShotType);

	// Mirror off/leg for a left-hander.
	if (!Intent.bRightHanded)
	{
		Cfg.ExitDir.Y = -Cfg.ExitDir.Y;
	}

	const FVector N = Cfg.ExitDir.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(-1, 0, 0));
	// LongAxis = in-plane component of the swing arc (the "along the blade" axis).
	FVector LongAxis = (Cfg.ArcDir - FVector::DotProduct(Cfg.ArcDir, N) * N)
		.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 0, 1));
	const FVector WidthAxis = FVector::CrossProduct(N, LongAxis).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));

	const double Speed = Cfg.SpeedMS * FMath::Clamp(Intent.PowerScale, 0.0, 1.5);

	// Bat velocity: mostly along the face normal (drives the ball), with a
	// tangential follow-through along the blade.
	const FVector LinVel = (N * 0.92 + LongAxis * 0.39).GetSafeNormal() * Speed;

	OutContactPointM = BallAtContact.Position;

	// Displace the sweet spot from the contact point by the timing & line errors.
	// Timing moves the strike ALONG the blade (early -> toward toe/handle); line
	// moves it ACROSS the face (toward an edge). This is the whole mishit model.
	const FVector SweetSpot = OutContactPointM
		- LongAxis * (Speed * Intent.TimingErrorSec)
		- WidthAxis * Intent.LineErrorM;

	OutBat.SweetSpotLocation = SweetSpot;
	OutBat.FaceNormal = N;
	OutBat.LongAxis = LongAxis;
	OutBat.WidthAxis = WidthAxis;
	OutBat.LinearVelocity = LinVel;
	OutBat.AngularVelocity = FVector::ZeroVector; // rigid translation for MVP
	OutBat.Orthonormalize();
}
