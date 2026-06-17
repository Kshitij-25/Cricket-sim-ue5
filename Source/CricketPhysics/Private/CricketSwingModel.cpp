#include "CricketSwingModel.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

namespace
{
	// Smooth ease in [0,1]: zero slope at both ends. Used for the sweet-spot path.
	FORCEINLINE double Smooth(double A)
	{
		A = FMath::Clamp(A, 0.0, 1.0);
		return A * A * (3.0 - 2.0 * A);
	}

	// Per-shot kinematic base, before footwork/handedness. Exit aim & speed are
	// kept identical to FCricketShotGenerator's ConfigFor so both batting paths
	// (timed swing here, teleported PlayShot there) drive the ball the same way
	// through the SAME collision solver — the hemispheres are one source of truth.
	struct FShotBase
	{
		FVector ContactOffset;       // sweet spot at contact, relative to stance (RH)
		FVector BackliftOffset;      // sweet spot at top of backlift
		FVector FollowOffset;        // sweet spot at end of follow-through
		FVector FaceAim;             // = exit direction = face normal at contact
		FVector ArcDir;              // in-plane swing direction (blade LongAxis)
		double  PeakSpeed;
		double  DownswingTime;
		double  FollowTime;
		bool    bDefensive;
	};

	FShotBase BaseFor(ECricketShotType Type)
	{
		switch (Type)
		{
		case ECricketShotType::DefensiveBlock:
			// Soft hands, bat angled down over the ball, met right under the eyes.
			return { FVector(-0.45, 0.00, 0.55), FVector(0.10, 0.0, 1.05), FVector(-0.48, 0.0, 0.62),
			         FVector(-1.0, 0.0, -0.12), FVector(0, 0, 1), 4.0, 0.14, 0.10, true };
		case ECricketShotType::StraightDrive:
			// Full, driven back down the ground; contact in front, under the eyes.
			return { FVector(-0.55, 0.00, 0.75), FVector(0.16, 0.0, 1.32), FVector(-0.72, 0.0, 1.38),
			         FVector(-1.0, 0.0, 0.18), FVector(0, 0, 1), 26.0, 0.16, 0.22, false };
		case ECricketShotType::CoverDrive:
			// Driven through the off side, slightly across the line.
			return { FVector(-0.55, 0.35, 0.72), FVector(0.16, 0.18, 1.30), FVector(-0.66, 0.55, 1.34),
			         FVector(-0.55, 0.83, 0.12), FVector(0, 0, 1), 24.0, 0.16, 0.22, false };
		case ECricketShotType::PullShot:
			// Short ball, contact back and high, swung across to the leg side.
			return { FVector(-0.10, -0.10, 1.15), FVector(0.22, -0.05, 1.45), FVector(-0.30, -0.70, 1.55),
			         FVector(-0.15, -0.92, 0.36), FVector(0, 0, 1), 25.0, 0.15, 0.24, false };
		default:
			return { FVector(-0.5, 0.0, 0.7), FVector(0.15, 0.0, 1.3), FVector(-0.7, 0.0, 1.35),
			         FVector(-1, 0, 0), FVector(0, 0, 1), 20.0, 0.16, 0.22, false };
		}
	}

	// Footwork shifts WHERE (and a touch of WHEN) the sweet spot meets the ball.
	// Front foot reaches down the pitch (-X) to the fuller ball and meets it a
	// little lower; back foot stays deep (+X) with room to go up for the short
	// ball. Whether that suits the delivery is then decided by geometry alone.
	void ApplyFootwork(ECricketFootwork Foot, FShotBase& B)
	{
		switch (Foot)
		{
		case ECricketFootwork::FrontFoot:
			B.ContactOffset += FVector(-0.30, 0.0, -0.08);
			B.FollowOffset  += FVector(-0.20, 0.0,  0.00);
			B.DownswingTime += 0.01; // a fraction longer to stride into it
			break;
		case ECricketFootwork::BackFoot:
			B.ContactOffset += FVector(+0.28, 0.0, +0.18);
			B.FollowOffset  += FVector(+0.18, 0.0,  0.10);
			B.DownswingTime -= 0.01; // a fraction quicker, playing late
			break;
		case ECricketFootwork::Neutral:
		default:
			break;
		}
	}
}

FCricketSwingProfile FCricketSwingModel::BuildProfile(
	ECricketShotType ShotType, ECricketFootwork Footwork, bool bRightHanded)
{
	FShotBase B = BaseFor(ShotType);
	ApplyFootwork(Footwork, B);

	// Mirror everything in Y for a left-hander (off side flips to -Y).
	if (!bRightHanded)
	{
		B.ContactOffset.Y  = -B.ContactOffset.Y;
		B.BackliftOffset.Y = -B.BackliftOffset.Y;
		B.FollowOffset.Y   = -B.FollowOffset.Y;
		B.FaceAim.Y        = -B.FaceAim.Y;
		B.ArcDir.Y         = -B.ArcDir.Y;
	}

	FCricketSwingProfile P;
	P.DownswingTimeSec    = B.DownswingTime;
	P.FollowThroughTimeSec = B.FollowTime;
	P.BackliftTimeSec     = 0.18;
	P.ContactOffsetM      = B.ContactOffset;
	P.BackliftOffsetM     = B.BackliftOffset;
	P.FollowThroughOffsetM = B.FollowOffset;
	P.FaceNormalAim       = B.FaceAim;
	P.ArcDir              = B.ArcDir;
	P.PeakBatSpeedMS      = B.PeakSpeed;
	P.StartSpeedFraction  = 0.25;
	P.bDefensive          = B.bDefensive;
	return P;
}

FCricketBatState FCricketSwingModel::EvaluateBat(
	const FCricketSwingProfile& Profile,
	const FCricketBattingInput& Input,
	const FVector& StanceOriginM,
	double SwingTimeSec,
	ECricketSwingPhase& OutPhase,
	double& OutBatSpeedMS)
{
	const double Power    = FMath::Clamp(Input.PowerScale, 0.0, 1.5);
	const double PeakSpeed = Profile.PeakBatSpeedMS * Power;
	const double StartSpeed = PeakSpeed * FMath::Clamp(Profile.StartSpeedFraction, 0.0, 1.0);

	const FVector A = StanceOriginM + Profile.BackliftOffsetM;     // backlift apex
	const FVector C = StanceOriginM + Profile.ContactOffsetM;      // contact zone
	const FVector F = StanceOriginM + Profile.FollowThroughOffsetM; // follow-through

	const double Tdown   = FMath::Max(Profile.DownswingTimeSec, 1e-3);
	const double Tfollow = FMath::Max(Profile.FollowThroughTimeSec, 1e-3);

	FVector Pos;
	double  Speed;
	if (SwingTimeSec <= 0.0)
	{
		Pos = A;
		Speed = 0.0;
		OutPhase = ECricketSwingPhase::Backlift;
	}
	else if (SwingTimeSec <= Tdown)
	{
		const double a = SwingTimeSec / Tdown;       // 0..1 through the downswing
		Pos = FMath::Lerp(A, C, Smooth(a));
		Speed = FMath::Lerp(StartSpeed, PeakSpeed, a * a); // accelerate, peaking at contact
		OutPhase = (a > 0.8) ? ECricketSwingPhase::Contact : ECricketSwingPhase::Downswing;
	}
	else
	{
		const double b = FMath::Clamp((SwingTimeSec - Tdown) / Tfollow, 0.0, 1.0);
		Pos = FMath::Lerp(C, F, Smooth(b));
		Speed = PeakSpeed * (1.0 - b);               // bleed off through the follow-through
		OutPhase = (b < 1.0) ? ECricketSwingPhase::FollowThrough : ECricketSwingPhase::Recovery;
	}
	OutBatSpeedMS = Speed;

	// Orientation: face normal = aim rotated by the fine-aim yaw (mirrored for LH).
	const double Yaw = Input.bRightHanded ? Input.AimYawDeg : -Input.AimYawDeg;
	FVector N = Profile.FaceNormalAim.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(-1, 0, 0));
	N = FRotator(0.0, Yaw, 0.0).RotateVector(N).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(-1, 0, 0));

	// LongAxis (toe->handle) = in-plane part of the swing arc; WidthAxis completes it.
	FVector LongAxis = (Profile.ArcDir - FVector::DotProduct(Profile.ArcDir, N) * N)
		.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 0, 1));
	const FVector WidthAxis = FVector::CrossProduct(N, LongAxis).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));

	// Bat velocity: mostly through the line (along the face normal) with a
	// tangential follow along the blade — the same recipe the shot generator uses,
	// so the collision solver sees a consistent strike.
	const FVector VelDir = (N * 0.92 + LongAxis * 0.39).GetSafeNormal(KINDA_SMALL_NUMBER, N);

	FCricketBatState Bat;
	Bat.SweetSpotLocation = Pos;
	Bat.FaceNormal = N;
	Bat.LongAxis = LongAxis;
	Bat.WidthAxis = WidthAxis;
	Bat.LinearVelocity = VelDir * Speed;
	Bat.AngularVelocity = FVector::ZeroVector; // rigid translation for the MVP swing
	Bat.Orthonormalize();
	return Bat;
}

bool FCricketSwingModel::DetectContact(
	const FCricketSwingProfile& Profile,
	const FCricketBattingInput& Input,
	const FVector& StanceOriginM,
	const FCricketBatProfile& BatProfile,
	const FVector& Ball0M,
	const FVector& Ball1M,
	double SwingTimeStartSec,
	double DeltaSec,
	int32 Substeps,
	FCricketContactSolution& OutSolution)
{
	OutSolution = FCricketContactSolution();

	const int32 N = FMath::Max(Substeps, 1);
	const double Dt = FMath::Max(DeltaSec, 1e-6);
	const FVector BallVel = (Ball1M - Ball0M) / Dt;

	// The ball reaches the face when its centre is within a ball-radius of the plane.
	const double ContactDist = BallRadiusM;
	// Blade extents along/across, padded by the ball radius (a grazing edge counts).
	const double AlongMin  = -(BatProfile.SweetSpotFromToeM + BallRadiusM);
	const double AlongMax  = (BatProfile.BladeLengthM - BatProfile.SweetSpotFromToeM) + BallRadiusM;
	const double AcrossMax = BatProfile.BladeWidthM * 0.5 + BallRadiusM;

	double dPrev = TNumericLimits<double>::Max();
	for (int32 s = 0; s <= N; ++s)
	{
		const double Frac = static_cast<double>(s) / static_cast<double>(N);
		const FVector BallPos = FMath::Lerp(Ball0M, Ball1M, Frac);
		const double SwingTime = SwingTimeStartSec + Frac * Dt;

		ECricketSwingPhase Phase;
		double BatSpeed;
		const FCricketBatState Bat = EvaluateBat(Profile, Input, StanceOriginM, SwingTime, Phase, BatSpeed);

		const FVector Rel = BallPos - Bat.SweetSpotLocation;
		const double d = FVector::DotProduct(Rel, Bat.FaceNormal);

		// Crossing: the ball was in front of the face and has now reached it.
		if (dPrev > ContactDist && d <= ContactDist)
		{
			const double Along  = FVector::DotProduct(Rel, Bat.LongAxis);
			const double Across = FVector::DotProduct(Rel, Bat.WidthAxis);
			const double Closing = -FVector::DotProduct(BallVel - Bat.LinearVelocity, Bat.FaceNormal);

			const bool bWithinBlade = (Along >= AlongMin && Along <= AlongMax && FMath::Abs(Across) <= AcrossMax);
			if (bWithinBlade && Closing > 0.0)
			{
				OutSolution.bHit = true;
				OutSolution.BatAtContact = Bat;
				OutSolution.ContactPointM = BallPos;
				OutSolution.ClosingSpeedMS = Closing;
				OutSolution.Phase = Phase;
				OutSolution.Timing = ClassifyTiming(Profile, SwingTime);
				return true;
			}
		}
		dPrev = d;
	}
	return false;
}

FCricketTimingResult FCricketSwingModel::ClassifyTiming(
	const FCricketSwingProfile& Profile, double ContactSwingTimeSec)
{
	FCricketTimingResult R;
	R.ContactSwingTimeSec = ContactSwingTimeSec;

	// +error = the sweet spot had not yet arrived (bat behind) => LATE. This sign
	// matches FCricketShotIntent::TimingErrorSec.
	const double Err = Profile.DownswingTimeSec - ContactSwingTimeSec;
	R.TimingErrorSec = Err;
	const double AbsE = FMath::Abs(Err);
	R.Normalized = FMath::Clamp(1.0 - AbsE / LooseWindowSec, 0.0, 1.0);

	if (AbsE <= PerfectWindowSec)
	{
		R.Quality = ECricketTimingQuality::Perfect;
	}
	else if (Err > 0.0) // bat behind the ball
	{
		R.Quality = (AbsE <= LooseWindowSec) ? ECricketTimingQuality::Late : ECricketTimingQuality::TooLate;
	}
	else // bat ahead of the ball
	{
		R.Quality = (AbsE <= LooseWindowSec) ? ECricketTimingQuality::Early : ECricketTimingQuality::TooEarly;
	}
	return R;
}
