#include "CricketDeliveryGenerator.h"
#include "CricketBallIntegrator.h"
#include "CricketTrajectoryPredictor.h"
#include "CricketAerodynamics.h"
#include "CricketPhysicsConstants.h"
#include "Math/RandomStream.h"

using namespace CricketPhysics;

// ===========================================================================
// Internal helpers
// ===========================================================================
namespace
{
	// Tuning for the predictor-driven aim solve. SOLVE_DT trades length accuracy
	// for cost during bisection (~speed*DT of bounce-detection granularity);
	// DIAG_DT is the finer pass used once for the reported diagnostics.
	constexpr double SOLVE_DT       = 0.004;
	constexpr double DIAG_DT        = 0.002;
	constexpr double SOLVE_MAX_TIME = 3.0;
	constexpr int32  SOLVE_ITERS    = 24;
	// Elevation search bounds (deg). The floor must be steep enough for an express
	// bowler to dig a bouncer in to ~12 m from the striker (a ~-15 deg floor clamps
	// short of that at 150 km/h); the ceiling covers a looped-up spinner.
	constexpr double ELEV_LO_DEG    = -22.0;
	constexpr double ELEV_HI_DEG    = 28.0;
	constexpr double LENGTH_TOL_M   = 0.20;

	/** Orthonormal horizontal flight frame from the release->striker vector. */
	void BuildFrame(const FVector& ReleaseM, const FVector& StrikerM, FVector& OutFwd, FVector& OutLat)
	{
		FVector ToStriker = StrikerM - ReleaseM;
		ToStriker.Z = 0.0;
		OutFwd = ToStriker.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
		// Left-perpendicular in the ground plane: +Y (off side) when Fwd = +X.
		OutLat = FVector(-OutFwd.Y, OutFwd.X, 0.0).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
	}

	/** Unit launch direction for an elevation/azimuth (deg) in the flight frame. */
	FVector DirFromAngles(const FVector& Fwd, const FVector& Lat, double ElevDeg, double AzimDeg)
	{
		const double Th = FMath::DegreesToRadians(ElevDeg);
		const double Ph = FMath::DegreesToRadians(AzimDeg);
		const FVector Horiz = FMath::Cos(Ph) * Fwd + FMath::Sin(Ph) * Lat;
		const FVector Dir = FMath::Cos(Th) * Horiz + FMath::Sin(Th) * FVector::UpVector;
		return Dir.GetSafeNormal(KINDA_SMALL_NUMBER, Fwd);
	}

	/** Predict the first bounce of a state through the shared model. */
	bool PredictFirstBounce(const FCricketBallState& Init, const FCricketBallSurface& Surface,
		const FCricketEnvironment& Env, const FCricketAeroCoefficients& Coeffs,
		double GroundZ, double Dt, FVector& OutBounce)
	{
		FCricketBallIntegrator I(Surface, Env, Coeffs, FCricketBallIntegrator::DefaultSubstep);
		FCricketPredictionParams P;
		P.SampleInterval  = Dt;
		P.MaxTime         = SOLVE_MAX_TIME;
		P.PitchPlaneZ     = GroundZ;
		P.bResolveBounces = false; // stop at the first bounce — that is the pitch point
		const FCricketTrajectoryPrediction Pred = FCricketTrajectoryPredictor::Predict(Init, I, P);
		if (Pred.BouncePoints.Num() > 0)
		{
			OutBounce = Pred.BouncePoints[0];
			return true;
		}
		return false;
	}

	/** Authored, movement-specific physical state (everything but the launch direction). */
	struct FMovementAuthoring
	{
		FVector SpinAxis = FVector(0, -1, 0);
		double  SpinRPM = 0.0;
		FVector SeamNormal = FVector(0, 1, 0);
		double  SeamStability = 0.95;
		FCricketBallSurface Surface;
		FCricketAeroCoefficients Coeffs;
		ECricketWristPosition Wrist = ECricketWristPosition::BehindSeamUp;
		ECricketDeliveryArchetype Archetype = ECricketDeliveryArchetype::SeamUp;
		bool bSwingFamily = false;
	};

	/** Author seam orientation, spin, surface and coefficients from the movement archetype. */
	FMovementAuthoring AuthorMovement(const FCricketBowlingIntent& Intent, const FCricketBowlingAction& Action,
		const FCricketDeliveryContext& Context, const FVector& Fwd, const FVector& Lat, double SpeedMS,
		double SeamSeedSign)
	{
		FMovementAuthoring M;
		M.Surface = Context.BallCondition;
		M.Coeffs = FCricketAeroCoefficients(); // model defaults; movement may override
		M.SeamStability = Action.HeldSeamStability;

		const FVector Up = FVector::UpVector;
		const double SwingAmt = FMath::Clamp(Intent.SwingAmount, 0.0, 1.0);
		const double SpinAmt = FMath::Clamp(Intent.SpinAmount, 0.0, 1.0);

		// Backspin axis that holds a pace/swing seam and supplies carry: world -Y
		// (= -Lat). A light arm-slot tilt nudges it but the magnitude is the point.
		const FVector BackspinAxis = (-Lat + Up * 0.04).GetSafeNormal(KINDA_SMALL_NUMBER, -Lat);

		// Present a seam canted toward DesiredLatSign*Lat at the optimal swing angle.
		// The swing DIRECTION then EMERGES from the regime: below the per-delivery
		// transition speed it is conventional and swings toward the seam's lean; a
		// rough ball at pace tips into the reverse regime and the side force flips.
		// TransitionSpeedMS is set high for a shiny ball (so conventional swing holds
		// up to genuine swing-bowling pace) and left low for the scuffed reverse ball.
		auto AuthorSwingSeam = [&](double DesiredLatSign, double SeamAmt, double TransitionSpeedMS)
		{
			M.Coeffs.SwingTransitionSpeed = TransitionSpeedMS;
			const FVector SeamLat = (DesiredLatSign * Lat).GetSafeNormal(KINDA_SMALL_NUMBER, Lat);
			const double Alpha = M.Coeffs.OptimalSeamAngleRad * FMath::Clamp(SeamAmt, 0.0, 1.0);
			M.SeamNormal = (FMath::Sin(Alpha) * Fwd + FMath::Cos(Alpha) * SeamLat)
				.GetSafeNormal(KINDA_SMALL_NUMBER, SeamLat);
			M.SpinAxis = BackspinAxis;
			M.SpinRPM = Action.StockBackspinRPM;
			M.bSwingFamily = true;
		};

		switch (Intent.Movement)
		{
		case ECricketMovement::SeamUp:
		{
			// Held seam presented near-vertical, canted to a (seeded) side so it can
			// jag off the pitch but generates no deliberate swing in the air.
			const double TiltUp = FMath::DegreesToRadians(24.0);
			const FVector SeamLat = SeamSeedSign * Lat;
			M.SeamNormal = (FMath::Cos(TiltUp) * SeamLat + FMath::Sin(TiltUp) * Up).GetSafeNormal();
			M.SpinAxis = BackspinAxis;
			M.SpinRPM = Action.StockBackspinRPM;
			M.Wrist = ECricketWristPosition::BehindSeamUp;
			M.Archetype = ECricketDeliveryArchetype::SeamUp;
			break;
		}
		case ECricketMovement::Outswing:
			// Shiny ball, conventional regime to genuine swing pace: swings toward +Y (off).
			if (FMath::Abs(M.Surface.ShineAsymmetry) < 0.6) { M.Surface.ShineAsymmetry = 1.0; }
			AuthorSwingSeam(+1.0, SwingAmt, /*TransitionSpeed*/ 42.0);
			M.Wrist = ECricketWristPosition::CantedOut;
			M.Archetype = ECricketDeliveryArchetype::Swing;
			break;
		case ECricketMovement::Inswing:
			if (FMath::Abs(M.Surface.ShineAsymmetry) < 0.6) { M.Surface.ShineAsymmetry = 1.0; }
			AuthorSwingSeam(-1.0, SwingAmt, /*TransitionSpeed*/ 42.0);  // swings toward -Y (into RH bat)
			M.Wrist = ECricketWristPosition::CantedIn;
			M.Archetype = ECricketDeliveryArchetype::Swing;
			break;
		case ECricketMovement::ReverseSwing:
			// A scuffed, low-shine ball at pace. The seam is presented like an away
			// (outswing) seam, but the rough surface + speed put it in the REVERSE
			// regime, so the side force flips and the ball tails IN (-Y) — emergent.
			// (Shine magnitude only scales the conventional gate; its sign is irrelevant.)
			M.Surface.Roughness = FMath::Max(M.Surface.Roughness, 0.9);
			M.Surface.ShineAsymmetry = 0.2;
			M.Surface.SeamProudness = FMath::Min(M.Surface.SeamProudness, 0.6);
			AuthorSwingSeam(+1.0, FMath::Max(SwingAmt, 0.7), /*TransitionSpeed*/ 28.0);
			M.Wrist = ECricketWristPosition::CantedIn;
			M.Archetype = ECricketDeliveryArchetype::Swing;
			break;
		case ECricketMovement::WobbleSeam:
		{
			// Scrambled seam: low stability + a precession that makes late movement
			// inconsistent. A modest cant means it may go either way late.
			const double TiltUp = FMath::DegreesToRadians(35.0);
			const FVector SeamLat = SeamSeedSign * Lat;
			M.SeamNormal = (FMath::Cos(TiltUp) * SeamLat + FMath::Sin(TiltUp) * Up).GetSafeNormal();
			M.SpinAxis = BackspinAxis;
			M.SpinRPM = Action.StockBackspinRPM * 0.55; // scrambled, fewer revs
			M.SeamStability = 0.15;
			M.Coeffs.WobbleSeamRateRadS = 14.0;
			M.Coeffs.WobbleSeamAmplitudeRad = 0.28;
			M.Wrist = ECricketWristPosition::Scrambled;
			M.Archetype = ECricketDeliveryArchetype::Swing;
			break;
		}
		case ECricketMovement::OffBreak:
		{
			// Finger spin. World axis ~ (+Fwd, +Lat topspin, +Up): grips and turns -Y
			// (into a RH bat), dips (topspin) and drifts to the off (+Y).
			M.SpinAxis = (0.85 * Fwd + 0.35 * Lat + 0.39 * Up).GetSafeNormal();
			M.SpinRPM = Action.MaxSpinRPM * FMath::Lerp(0.60, 1.0, SpinAmt);
			// Seam perpendicular to flight (no swing), canted to aid the -Y turn at the bounce.
			const double Tilt = FMath::DegreesToRadians(35.0);
			M.SeamNormal = (-1.0 * FMath::Sin(Tilt) * Lat + FMath::Cos(Tilt) * Up).GetSafeNormal();
			M.SeamStability = 0.9;
			M.Wrist = ECricketWristPosition::FingerSpin;
			M.Archetype = ECricketDeliveryArchetype::OffSpin;
			break;
		}
		case ECricketMovement::LegBreak:
		{
			// Wrist spin. World axis ~ (-Fwd, +Lat topspin, -Up): grips and turns +Y
			// (away from a RH bat), dips, and drifts to the leg (-Y).
			M.SpinAxis = (-0.85 * Fwd + 0.35 * Lat - 0.39 * Up).GetSafeNormal();
			M.SpinRPM = Action.MaxSpinRPM * FMath::Lerp(0.60, 1.0, SpinAmt);
			const double Tilt = FMath::DegreesToRadians(35.0);
			M.SeamNormal = (+1.0 * FMath::Sin(Tilt) * Lat + FMath::Cos(Tilt) * Up).GetSafeNormal();
			M.SeamStability = 0.9;
			M.Wrist = ECricketWristPosition::WristSpin;
			M.Archetype = ECricketDeliveryArchetype::LegSpin;
			break;
		}
		default:
			break;
		}

		return M;
	}
}

// ===========================================================================
// Public authoring helpers
// ===========================================================================

ECricketBowlingStyle FCricketDeliveryGenerator::StyleForMovement(ECricketMovement Movement)
{
	switch (Movement)
	{
	case ECricketMovement::OffBreak: return ECricketBowlingStyle::OffSpin;
	case ECricketMovement::LegBreak: return ECricketBowlingStyle::LegSpin;
	case ECricketMovement::Outswing:
	case ECricketMovement::Inswing:
	case ECricketMovement::ReverseSwing:
	case ECricketMovement::WobbleSeam: return ECricketBowlingStyle::Swing;
	default: return ECricketBowlingStyle::Pace;
	}
}

double FCricketDeliveryGenerator::TargetLengthM(ECricketLength Length, ECricketBowlingStyle Style)
{
	const bool bSpin = (Style == ECricketBowlingStyle::OffSpin || Style == ECricketBowlingStyle::LegSpin);
	switch (Length)
	{
	case ECricketLength::FullToss:     return 0.3;
	case ECricketLength::Yorker:       return bSpin ? 1.0 : 0.6;
	case ECricketLength::Full:         return bSpin ? 1.8 : 2.2;
	case ECricketLength::GoodLength:   return bSpin ? 3.5 : 6.0;
	case ECricketLength::BackOfLength: return bSpin ? 5.0 : 8.0;
	case ECricketLength::Short:        return bSpin ? 7.0 : 10.0;
	case ECricketLength::Bouncer:      return bSpin ? 8.5 : 12.0;
	default:                           return 6.0;
	}
}

double FCricketDeliveryGenerator::TargetLineM(ECricketLine Line, ECricketBowlingArm Arm)
{
	(void)Arm; // Lines are batsman-relative (off=+Y for a RH striker); arm shifts the release point, not the channel.
	switch (Line)
	{
	case ECricketLine::WideOutsideOff: return 0.45;
	case ECricketLine::OutsideOff:     return 0.22;
	case ECricketLine::OffStump:       return CricketField::StumpHalfSpacingM;       // ~+0.114
	case ECricketLine::Middle:         return 0.0;
	case ECricketLine::LegStump:       return -CricketField::StumpHalfSpacingM;      // ~-0.114
	case ECricketLine::DownLeg:        return -0.30;
	default:                           return 0.0;
	}
}

double FCricketDeliveryGenerator::ResolveReleaseSpeedMS(const FCricketBowlingIntent& Intent, const FCricketBowlingAction& Action)
{
	const double Kmh = FMath::Lerp(Action.MinPaceKmh, Action.MaxPaceKmh, FMath::Clamp(Intent.Pace01, 0.0, 1.0));
	return KmhToMs(Kmh);
}

// ===========================================================================
// Generate
// ===========================================================================

FCricketReleaseParameters FCricketDeliveryGenerator::Generate(
	const FCricketBowlingIntent& Intent,
	const FCricketBowlingAction& Action,
	const FCricketDeliveryContext& Context,
	FCricketDeliveryDiagnostics* OutDiag)
{
	FCricketReleaseParameters Out;

	const ECricketBowlingStyle Style = StyleForMovement(Intent.Movement);
	const double GroundZ = Context.GroundPlaneZM; // world pitch plane (m); must match the live floor

	// --- Flight frame ------------------------------------------------------
	FVector Fwd, Lat;
	BuildFrame(Context.ReleasePositionM, Context.StrikerStumpsM, Fwd, Lat);

	// --- Seeded human scatter (applied to INPUTS only) ---------------------
	FRandomStream Rng(Context.Seed);
	const double Scatter = FMath::Clamp(Context.HumanScatter, 0.0, 1.0);
	auto Jitter = [&](double Amp) { return (Rng.FRand() * 2.0 - 1.0) * Amp * Scatter; };
	const double SeamSeedSign = (Rng.FRand() < 0.5) ? -1.0 : 1.0;

	double SpeedMS = ResolveReleaseSpeedMS(Intent, Action) + Jitter(2.0);
	SpeedMS = FMath::Max(SpeedMS, 5.0);

	double TargetLength = TargetLengthM(Intent.Length, Style) + Intent.LengthFineM + Jitter(0.5);
	TargetLength = FMath::Clamp(TargetLength, 0.1, CricketField::PitchLengthM - 1.0);

	double TargetLineY = TargetLineM(Intent.Line, Intent.Arm) + Intent.LineFineM + Jitter(0.12);

	// --- Author the movement (seam/spin/surface/coeffs/stability) ----------
	const FMovementAuthoring M = AuthorMovement(Intent, Action, Context, Fwd, Lat, SpeedMS, SeamSeedSign);
	const FVector AngularVel = M.SpinAxis.GetSafeNormal() * RpmToRadS(M.SpinRPM);

	// Template state shared by every aim-solve probe; only Velocity changes.
	auto MakeProbe = [&](const FVector& Vel)
	{
		FCricketBallState S;
		S.Position = Context.ReleasePositionM;
		S.Velocity = Vel;
		S.AngularVelocity = AngularVel;
		S.SeamNormal = M.SeamNormal;
		S.SeamStability = M.SeamStability;
		return S;
	};

	// --- Aim: azimuth from the chosen line (the ball then swings off it) ----
	const FVector AimPoint = Context.StrikerStumpsM + TargetLineY * Lat;
	FVector AimHoriz = AimPoint - Context.ReleasePositionM;
	AimHoriz.Z = 0.0;
	const double AzimuthDeg = FMath::RadiansToDegrees(
		FMath::Atan2(FVector::DotProduct(AimHoriz, Lat), FVector::DotProduct(AimHoriz, Fwd)));

	// --- Aim: solve release elevation so the ball pitches at TargetLength ---
	// Predicted length (m from the striker, along Fwd) for a candidate elevation.
	auto PredictLength = [&](double ElevDeg) -> double
	{
		const FVector Dir = DirFromAngles(Fwd, Lat, ElevDeg, AzimuthDeg);
		FVector Bounce;
		if (PredictFirstBounce(MakeProbe(Dir * SpeedMS), M.Surface, Context.Environment, M.Coeffs, GroundZ, SOLVE_DT, Bounce))
		{
			return FVector::DotProduct(Context.StrikerStumpsM - Bounce, Fwd);
		}
		return -100.0; // never pitched in front of the stumps => treat as very full
	};

	double Lo = ELEV_LO_DEG, Hi = ELEV_HI_DEG;
	const double LengthLo = PredictLength(Lo); // higher length (shorter delivery)
	const double LengthHi = PredictLength(Hi); // lower length (fuller delivery)
	double ElevationDeg;
	bool bConverged;
	if (TargetLength >= LengthLo)      { ElevationDeg = Lo; bConverged = false; }
	else if (TargetLength <= LengthHi) { ElevationDeg = Hi; bConverged = false; }
	else
	{
		for (int32 i = 0; i < SOLVE_ITERS; ++i)
		{
			const double Mid = 0.5 * (Lo + Hi);
			const double Lm = PredictLength(Mid);
			// Length decreases as elevation increases. Too long => raise elevation.
			if (Lm > TargetLength) { Lo = Mid; } else { Hi = Mid; }
		}
		ElevationDeg = 0.5 * (Lo + Hi);
		bConverged = true;
	}

	// --- Assemble the launch velocity --------------------------------------
	const FVector LaunchDir = DirFromAngles(Fwd, Lat, ElevationDeg, AzimuthDeg);
	const FVector LaunchVel = LaunchDir * SpeedMS;

	// --- Fill the release parameters ---------------------------------------
	Out.ReleaseSpeedMS      = SpeedMS;
	Out.ReleaseVelocityMS   = LaunchVel;
	Out.ReleasePositionM    = Context.ReleasePositionM;
	Out.ReleaseElevationDeg = ElevationDeg;
	Out.ReleaseAzimuthDeg   = AzimuthDeg;
	Out.SeamNormal          = M.SeamNormal.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
	Out.SpinAxis            = M.SpinAxis.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, -1, 0));
	Out.SpinRateRPM         = M.SpinRPM;
	Out.WristPosition       = M.Wrist;
	Out.BallCondition       = M.Surface;
	Out.AngularVelocityRadS = AngularVel;
	Out.SeamStability       = M.SeamStability;
	Out.Coefficients        = M.Coeffs;
	Out.Archetype           = M.Archetype;

	// --- Diagnostics (same model => prediction equals the actual flight) ----
	if (OutDiag)
	{
		FCricketDeliveryDiagnostics D;
		D.bAimConverged = bConverged;

		FVector Bounce;
		if (PredictFirstBounce(MakeProbe(LaunchVel), M.Surface, Context.Environment, M.Coeffs, GroundZ, DIAG_DT, Bounce))
		{
			D.PredictedPitchPointM   = Bounce;
			D.PredictedLengthM       = FVector::DotProduct(Context.StrikerStumpsM - Bounce, Fwd);
			D.PredictedLineAtPitchM  = FVector::DotProduct(Bounce - Context.StrikerStumpsM, Lat);
			D.AimResidualM           = FMath::Abs(D.PredictedLengthM - TargetLength);
			D.bAimConverged          = bConverged && (D.AimResidualM <= LENGTH_TOL_M);
		}

		// Free-flight swing: aim straight (azimuth 0) and measure lateral deviation
		// at the pitch — the honest "how much is it swinging", independent of aim.
		const FVector StraightDir = DirFromAngles(Fwd, Lat, ElevationDeg, 0.0);
		FVector StraightBounce;
		if (PredictFirstBounce(MakeProbe(StraightDir * SpeedMS), M.Surface, Context.Environment, M.Coeffs, GroundZ, DIAG_DT, StraightBounce))
		{
			D.FreeFlightSwingM = FVector::DotProduct(StraightBounce - Context.ReleasePositionM, Lat);
		}

		// Regime classification.
		if (M.bSwingFamily || Intent.Movement == ECricketMovement::ReverseSwing)
		{
			const double Reverse = FCricketAerodynamics::ComputeReverseRegime(SpeedMS, M.Surface, M.Coeffs);
			D.Regime = (Reverse > 0.5) ? ECricketSwingRegime::Reverse : ECricketSwingRegime::Conventional;
		}
		else
		{
			D.Regime = ECricketSwingRegime::None;
		}

		*OutDiag = D;
	}

	return Out;
}
