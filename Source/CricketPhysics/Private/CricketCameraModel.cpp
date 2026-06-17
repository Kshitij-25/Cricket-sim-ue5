#include "CricketCameraModel.h"

namespace
{
	const FVector UpV(0, 0, 1);

	FRotator LookRot(const FVector& From, const FVector& To)
	{
		return (To - From).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0)).Rotation();
	}

	// Pitch axis (batter -> bowler, flattened) and the side axis, from the ends.
	void PitchAxes(const FCricketCameraSubjects& S, FVector& OutPitchDir, FVector& OutSideDir)
	{
		FVector Dir = S.BowlerStumpsCm - S.BatterStumpsCm; Dir.Z = 0.0;
		OutPitchDir = Dir.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
		OutSideDir = FVector::CrossProduct(OutPitchDir, UpV).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
	}

	// Lateral (horizontal) distance from point P to the line through O along unit DirXY.
	double LateralFromLineXY(const FVector& P, const FVector& O, const FVector& DirXY)
	{
		FVector V = P - O; V.Z = 0.0;
		const FVector Along = DirXY * FVector::DotProduct(V, DirXY);
		return (V - Along).Size();
	}
}

FCricketCameraPose FCricketCameraModel::ComputePose(
	ECricketCameraMode Mode, const FCricketCameraSubjects& S, const FCricketCameraConfig& C)
{
	FCricketCameraPose P;
	P.FOVDeg = C.FOVDeg;

	FVector PitchDir, SideDir;
	PitchAxes(S, PitchDir, SideDir);
	const FVector MidPitch = (S.BatterStumpsCm + S.BowlerStumpsCm) * 0.5;
	const FVector BallTarget = S.bBallInFlight ? S.BallCm : MidPitch;

	switch (Mode)
	{
	case ECricketCameraMode::Batting:
	{
		// Behind the batter (opposite the bowler), elevated, looking down the pitch
		// so line & length read clearly; tracks the ball once it is live.
		P.LocationCm = S.BatterStumpsCm - PitchDir * C.DistanceCm + UpV * C.HeightCm;
		const FVector Target = S.bBallInFlight ? S.BallCm : (S.BatterStumpsCm + PitchDir * 900.0);
		P.Rotation = LookRot(P.LocationCm, Target);
		break;
	}
	case ECricketCameraMode::Bowling:
	{
		// Behind the bowler, lower, looking at the batter end — release/seam visible.
		P.LocationCm = S.BowlerStumpsCm + PitchDir * C.DistanceCm + UpV * C.BowlingHeightCm;
		const FVector Target = S.bBallInFlight ? S.BallCm : S.BatterStumpsCm;
		P.Rotation = LookRot(P.LocationCm, Target);
		break;
	}
	case ECricketCameraMode::Fielding:
	{
		// Trail the ball from above-behind; frame the active fielder too for catch
		// awareness; throw visibility follows the ball.
		P.LocationCm = S.BallCm - PitchDir * (C.FieldingDistanceCm * 0.6) + UpV * C.FieldingHeightCm;
		const FVector Target = S.bHasActiveFielder ? (S.BallCm + S.ActiveFielderCm) * 0.5 : S.BallCm;
		P.Rotation = LookRot(P.LocationCm, Target);
		break;
	}
	case ECricketCameraMode::Spectator:
	{
		// Side-on broadcast: square of the pitch, high.
		P.LocationCm = MidPitch + SideDir * C.SpectatorSideCm + UpV * C.SpectatorHeightCm;
		P.Rotation = LookRot(P.LocationCm, BallTarget);
		break;
	}
	case ECricketCameraMode::Free:
	{
		P.LocationCm = S.FreeLocationCm;
		P.Rotation = FRotator(S.FreePitchDeg, S.FreeYawDeg, 0.0);
		break;
	}
	case ECricketCameraMode::Orbit:
	{
		const FVector Pivot = S.OrbitPivotCm.IsNearlyZero() ? S.BallCm : S.OrbitPivotCm;
		const FVector Dir = FRotator(S.OrbitPitchDeg, S.OrbitYawDeg, 0.0).Vector();
		P.LocationCm = Pivot - Dir * C.OrbitRadiusCm;
		P.Rotation = Dir.Rotation();
		break;
	}
	case ECricketCameraMode::BallFollow:
	{
		// Chase from just behind the ball's travel.
		FVector Back = S.BallVelocityMS; Back.Z = 0.0;
		Back = Back.GetSafeNormal(KINDA_SMALL_NUMBER, PitchDir);
		P.LocationCm = S.BallCm - Back * (C.FieldingDistanceCm * 0.5) + UpV * (C.FieldingHeightCm * 0.5);
		P.Rotation = LookRot(P.LocationCm, S.BallCm);
		break;
	}
	case ECricketCameraMode::PhysicsInspection:
	{
		// Tight, side-on, close to the ball — read swing/spin and the seam.
		P.LocationCm = S.BallCm + SideDir * C.InspectionDistanceCm + UpV * 60.0;
		P.Rotation = LookRot(P.LocationCm, S.BallCm);
		P.FOVDeg = C.InspectionFOVDeg;
		break;
	}
	}
	return P;
}

FCricketCameraPose FCricketCameraModel::Blend(const FCricketCameraPose& From, const FCricketCameraPose& To, double Alpha)
{
	const double A = FMath::SmoothStep(0.0, 1.0, FMath::Clamp(Alpha, 0.0, 1.0));
	FCricketCameraPose Out;
	Out.LocationCm = FMath::Lerp(From.LocationCm, To.LocationCm, A);
	Out.Rotation = FQuat::Slerp(From.Rotation.Quaternion(), To.Rotation.Quaternion(), A).Rotator();
	Out.FOVDeg = FMath::Lerp(From.FOVDeg, To.FOVDeg, A);
	return Out;
}

double FCricketCameraModel::MaxLateralDeviationM(const TArray<FVector>& PathM, int32 StartIdx, int32 EndIdx)
{
	if (!PathM.IsValidIndex(StartIdx) || !PathM.IsValidIndex(EndIdx) || EndIdx <= StartIdx + 1)
	{
		return 0.0;
	}
	FVector ChordDir = PathM[EndIdx] - PathM[StartIdx]; ChordDir.Z = 0.0;
	ChordDir = ChordDir.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));

	double MaxDev = 0.0;
	for (int32 i = StartIdx + 1; i < EndIdx; ++i)
	{
		MaxDev = FMath::Max(MaxDev, LateralFromLineXY(PathM[i], PathM[StartIdx], ChordDir));
	}
	return MaxDev;
}

double FCricketCameraModel::SwingDeviationM(const TArray<FVector>& PathM, int32 BounceIdx)
{
	const int32 End = (BounceIdx > 0 && BounceIdx < PathM.Num()) ? BounceIdx : PathM.Num() - 1;
	return MaxLateralDeviationM(PathM, 0, End);
}

double FCricketCameraModel::SpinDeviationM(const TArray<FVector>& PathM, int32 BounceIdx)
{
	if (!PathM.IsValidIndex(BounceIdx) || BounceIdx < 2 || BounceIdx >= PathM.Num() - 1)
	{
		return 0.0;
	}
	// Incoming direction from a SHORT local tangent just before the bounce
	// (horizontal), so a curving (swinging) approach doesn't contaminate it.
	const int32 Back = FMath::Max(0, BounceIdx - 2);
	FVector InDir = PathM[BounceIdx] - PathM[Back]; InDir.Z = 0.0;
	InDir = InDir.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));

	// Max lateral departure of the post-bounce path from that incoming line.
	double MaxDev = 0.0;
	for (int32 i = BounceIdx + 1; i < PathM.Num(); ++i)
	{
		MaxDev = FMath::Max(MaxDev, LateralFromLineXY(PathM[i], PathM[BounceIdx], InDir));
	}
	return MaxDev;
}
