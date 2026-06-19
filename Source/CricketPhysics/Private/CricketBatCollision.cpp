#include "CricketBatCollision.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

void FCricketBatState::Orthonormalize()
{
	FaceNormal = FaceNormal.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(-1, 0, 0));
	// Project the long axis perpendicular to the face normal.
	LongAxis = (LongAxis - FVector::DotProduct(LongAxis, FaceNormal) * FaceNormal)
		.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 0, 1));
	WidthAxis = FVector::CrossProduct(FaceNormal, LongAxis).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
}

void FCricketBatCollision::AnalyzeContact(
	const FCricketBatState& InBat,
	const FCricketBatProfile& Profile,
	const FVector& ContactPointM,
	FCricketBatImpactReport& Report)
{
	FCricketBatState Bat = InBat;
	Bat.Orthonormalize();

	const FVector Rel = ContactPointM - Bat.SweetSpotLocation;
	const double Along = FVector::DotProduct(Rel, Bat.LongAxis);   // + toward handle
	const double Across = FVector::DotProduct(Rel, Bat.WidthAxis); // + outside edge

	Report.AlongBladeM = Along;
	Report.AcrossFaceM = Across;
	const double Dist2D = FMath::Sqrt(Along * Along + Across * Across);
	Report.DistanceFromSweetSpotM = Dist2D;

	// Quality: Gaussian falloff from the sweet spot. 1 at centre.
	const double Q = Profile.QualityFalloffM > KINDA_SMALL_NUMBER
		? FMath::Exp(-FMath::Square(Dist2D / Profile.QualityFalloffM))
		: (Dist2D < Profile.SweetSpotRadiusM ? 1.0 : 0.0);
	Report.Quality = Q;

	// Location-dependent effective mass and restitution follow the quality.
	Report.EffectiveMassKg = FMath::Lerp(Profile.EffectiveMassEdgeKg, Profile.EffectiveMassSweetSpotKg, Q);
	Report.RestitutionUsed = FMath::Lerp(Profile.RestitutionEdge, Profile.RestitutionSweetSpot, Q);

	// Fractional offsets used for classification.
	const double HalfWidth = FMath::Max(Profile.BladeWidthM * 0.5, KINDA_SMALL_NUMBER);
	const double AcrossFrac = FMath::Clamp(FMath::Abs(Across) / HalfWidth, 0.0, 1.0);
	const double UpReach = FMath::Max(Profile.BladeLengthM - Profile.SweetSpotFromToeM, KINDA_SMALL_NUMBER);
	const double DownReach = FMath::Max(Profile.SweetSpotFromToeM, KINDA_SMALL_NUMBER);
	const double AlongFrac = FMath::Clamp(Along >= 0.0 ? Along / UpReach : -Along / DownReach, 0.0, 1.0);

	Report.EdgeFactor = FMath::Clamp(FMath::Max(AcrossFrac, AlongFrac), 0.0, 1.0);

	// Side classification (across the face).
	if (AcrossFrac >= 0.55)
	{
		Report.Side = Across > 0.0 ? ECricketContactSide::OutsideEdge : ECricketContactSide::InsideEdge;
	}
	else
	{
		Report.Side = ECricketContactSide::Centre;
	}

	// Primary region (priority: toe -> side edge -> top/bottom -> middle).
	const double HeightFromToe = Profile.SweetSpotFromToeM + Along;
	if (HeightFromToe < 0.03)
	{
		Report.Region = ECricketContactRegion::Toe;
	}
	else if (AcrossFrac >= 0.55)
	{
		Report.Region = AcrossFrac >= 0.82 ? ECricketContactRegion::ThinEdge : ECricketContactRegion::ThickEdge;
	}
	else if (AlongFrac >= 0.5 && Along > 0.0)
	{
		Report.Region = ECricketContactRegion::TopEdge;
	}
	else if (AlongFrac >= 0.45 && Along < 0.0)
	{
		Report.Region = ECricketContactRegion::BottomEdge;
	}
	else
	{
		Report.Region = ECricketContactRegion::Middle;
	}

	Report.bIsEdge = (Report.Side != ECricketContactSide::Centre)
		|| (Report.Region != ECricketContactRegion::Middle)
		|| Report.EdgeFactor > 0.45;
}

bool FCricketBatCollision::Resolve(
	const FCricketBallState& BallIn,
	const FCricketBatState& InBat,
	const FCricketBatProfile& Profile,
	const FVector& ContactPointM,
	FCricketBallState& BallOut,
	FCricketBatImpactReport& Report)
{
	Report = FCricketBatImpactReport();
	BallOut = BallIn;

	FCricketBatState Bat = InBat;
	Bat.Orthonormalize();

	// 1. Contact analysis -> region, effective mass, restitution.
	AnalyzeContact(Bat, Profile, ContactPointM, Report);

	const double m = BallMassKg;
	const double R = BallRadiusM;
	const double I = BallInertia;
	const double M = FMath::Max(Report.EffectiveMassKg, KINDA_SMALL_NUMBER);
	const double e = Report.RestitutionUsed;

	const FVector N = Bat.FaceNormal;                 // nominal face normal, toward the ball

	// Curved blade: the local contact normal tilts off the flat face toward the
	// edge/toe the further the contact sits from the spine/sweet line. A flat
	// face (EdgeCurvature 0) leaves Neff == N, so middled drives are unaffected;
	// an edge hit gets an oblique normal that both deflects the ball sideways and
	// reduces the closing speed (pace bleed) — the physics the EdgeImpact case
	// expects, and what a real outside edge does.
	const double HalfWidthM = FMath::Max(Profile.BladeWidthM * 0.5, KINDA_SMALL_NUMBER);
	const double UpReachM = FMath::Max(Profile.BladeLengthM - Profile.SweetSpotFromToeM, KINDA_SMALL_NUMBER);
	const double DownReachM = FMath::Max(Profile.SweetSpotFromToeM, KINDA_SMALL_NUMBER);
	const double AcrossSigned = FMath::Clamp(Report.AcrossFaceM / HalfWidthM, -1.0, 1.0);
	const double AlongSigned = FMath::Clamp(
		Report.AlongBladeM >= 0.0 ? Report.AlongBladeM / UpReachM : Report.AlongBladeM / DownReachM, -1.0, 1.0);
	const FVector Neff = (N
		+ Profile.EdgeCurvature * AcrossSigned * Bat.WidthAxis
		+ Profile.EdgeCurvature * AlongSigned * Bat.LongAxis).GetSafeNormal(KINDA_SMALL_NUMBER, N);

	const FVector Vbat = Bat.VelocityAt(ContactPointM);
	const FVector Vb = BallIn.Velocity;

	Report.IncomingSpeedMS = Vb.Size();

	// Relative velocity of ball w.r.t. bat along the (effective) contact normal.
	const FVector U = Vb - Vbat;
	const double Un = FVector::DotProduct(U, Neff);

	// Resolve only a closing contact (ball moving into the face).
	if (Un >= 0.0)
	{
		Report.bMadeContact = false;
		return false;
	}
	Report.bMadeContact = true;

	// 2a. Normal impulse with restitution against the bat's effective mass.
	const double Mu = (m * M) / (m + M);     // reduced mass
	const double Jn = -(1.0 + e) * Un * Mu;  // > 0
	const FVector DvNormal = (Jn / m) * Neff;

	// 2b. Tangential friction + spin coupling (mirrors FCricketPitchInteraction).
	const FVector RContact = -R * Neff;      // ball surface point touching the face
	const FVector BallSurfaceVel = Vb + FVector::CrossProduct(BallIn.AngularVelocity, RContact);
	const FVector USurf = BallSurfaceVel - Vbat;
	const FVector Ut = USurf - FVector::DotProduct(USurf, Neff) * Neff;
	const double UtLen = Ut.Size();

	FVector DvTangent = FVector::ZeroVector;
	if (UtLen > KINDA_SMALL_NUMBER)
	{
		const FVector TangDir = Ut / UtLen;
		const double K = 1.0 + (m * R * R) / I;       // = 2.5
		const double JtStickVel = UtLen / K;          // delta-v to arrest sliding
		const double JtMaxVel = Profile.Friction * (Jn / m); // Coulomb limit (delta-v units)
		const double JtVel = FMath::Min(JtStickVel, JtMaxVel);
		DvTangent = -JtVel * TangDir;
	}

	const FVector Vout = Vb + DvNormal + DvTangent;

	// Spin response to the tangential impulse: dOmega = (r x J)/I, J in force units.
	const FVector TangImpulse = DvTangent * m;
	const FVector DOmega = FVector::CrossProduct(RContact, TangImpulse) / I;
	const FVector Wout = BallIn.AngularVelocity + DOmega;

	// 3. Write the post-impact ball (a fresh flight off the bat).
	BallOut.Position = ContactPointM;
	BallOut.Velocity = Vout;
	BallOut.AngularVelocity = Wout;
	BallOut.Acceleration = FVector::ZeroVector;
	BallOut.TimeSinceRelease = 0.0;

	// --- Outcomes ---
	const double ExitSpeed = Vout.Size();
	Report.OutgoingVelocity = Vout;
	Report.ExitSpeedMS = ExitSpeed;
	Report.LaunchAngleDeg = ExitSpeed > KINDA_SMALL_NUMBER
		? FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Vout.Z / ExitSpeed, -1.0, 1.0))) : 0.0;

	// Deflection: horizontal angle between exit and the face normal.
	const FVector Nh = FVector(N.X, N.Y, 0).GetSafeNormal();
	const FVector Vh = FVector(Vout.X, Vout.Y, 0).GetSafeNormal();
	if (!Nh.IsNearlyZero() && !Vh.IsNearlyZero())
	{
		Report.DeflectionAngleDeg = FMath::RadiansToDegrees(
			FMath::Acos(FMath::Clamp(FVector::DotProduct(Nh, Vh), -1.0, 1.0)));
	}

	Report.OutgoingSpin = Wout;
	Report.SpinTransferRadS = (Wout - BallIn.AngularVelocity).Size();

	// Energy bookkeeping (no energy is created).
	Report.EnergyInJ = 0.5 * m * Vb.SizeSquared() + 0.5 * M * Vbat.SizeSquared();
	Report.EnergyOutBallJ = 0.5 * m * Vout.SizeSquared();
	Report.EnergyTransferFraction = Report.EnergyInJ > KINDA_SMALL_NUMBER
		? Report.EnergyOutBallJ / Report.EnergyInJ : 0.0;

	return true;
}
