#include "CricketAerodynamics.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

// ---------------------------------------------------------------------------
// Environment: air density from temperature, humidity and pressure.
// Uses the partial-pressure method (dry air + water vapour). Humid air is
// LESS dense than dry air at the same temperature/pressure, which is why the
// "swings more when humid" folklore is NOT a density effect — left tunable.
// ---------------------------------------------------------------------------
double FCricketEnvironment::ComputeAirDensity() const
{
	constexpr double Rd = 287.058; // specific gas constant, dry air (J/(kg*K))
	constexpr double Rv = 461.495; // specific gas constant, water vapour
	const double T = TemperatureC + 273.15;        // Kelvin
	const double P = PressureHpa * 100.0;           // Pa

	// Saturation vapour pressure (Tetens equation), Pa.
	const double Psat = 610.78 * FMath::Exp(17.27 * TemperatureC / (TemperatureC + 237.3));
	const double Pv = FMath::Clamp(RelativeHumidity, 0.0, 1.0) * Psat; // actual vapour pressure
	const double Pd = P - Pv;                                          // dry-air partial pressure

	const double Rho = (Pd / (Rd * T)) + (Pv / (Rv * T));
	return Rho > 0.0 ? Rho : AirDensitySeaLevel;
}

double FCricketAerodynamics::ComputeReynolds(double AirSpeed, double AirDensity)
{
	// Re = rho * v * d / mu ; mu = rho * nu  =>  Re = v * d / nu (density cancels for ideal gas nu).
	// We keep the explicit form so tuning can decouple them if desired.
	const double Mu = AirDensity * AirKinematicViscosity;
	if (Mu <= 0.0)
	{
		return 0.0;
	}
	return (AirDensity * AirSpeed * BallDiameterM) / Mu;
}

double FCricketAerodynamics::ComputeReverseRegime(double AirSpeed, const FCricketBallSurface& Surface,
	const FCricketAeroCoefficients& Coeffs)
{
	// A rough ball reverses at lower speed: drop the effective transition speed
	// by up to ~8 m/s as roughness goes 0 -> 1.
	const double EffectiveTransition =
		Coeffs.SwingTransitionSpeed - 8.0 * FMath::Clamp(Surface.Roughness, 0.0, 1.0);

	// Smooth transition over a ~6 m/s band centred on the effective speed.
	const double Lo = EffectiveTransition - 3.0;
	const double Hi = EffectiveTransition + 3.0;
	return FMath::SmoothStep(Lo, Hi, AirSpeed); // 0 below Lo, 1 above Hi
}

namespace
{
	/**
	 * Seam-presentation shape: 0 at seam angle 0, peaks at 1 at the optimal
	 * angle, then falls off as the seam "stalls" past optimal. Gamma-like curve
	 * x*exp(1-x) with x = theta/thetaOpt has its maximum value 1 exactly at x=1.
	 */
	double SeamShape(double SeamAngleRad, double OptimalRad)
	{
		if (OptimalRad <= KINDA_SMALL_NUMBER)
		{
			return 0.0;
		}
		const double X = FMath::Abs(SeamAngleRad) / OptimalRad;
		return X * FMath::Exp(1.0 - X);
	}
}

FCricketAeroResult FCricketAerodynamics::Evaluate(
	const FCricketBallState& State,
	const FCricketBallSurface& Surface,
	const FCricketEnvironment& Environment,
	const FCricketAeroCoefficients& Coeffs)
{
	FCricketAeroResult Out;

	const double Rho = Environment.ComputeAirDensity();
	const double A = BallCrossSectionM2;

	// Airspeed is relative to the moving air mass (wind).
	const FVector VRel = State.Velocity - Environment.Wind;
	const double Speed = VRel.Size();
	Out.ReynoldsNumber = ComputeReynolds(Speed, Rho);

	// Below a crawl there is no meaningful aerodynamic force.
	if (Speed < KINDA_SMALL_NUMBER)
	{
		// Still apply spin decay so a stationary spinning ball bleeds spin.
		Out.AngularAccel = -Coeffs.SpinDecayRate * State.AngularVelocity;
		return Out;
	}

	const FVector VHat = VRel / Speed;
	const double DynPressureArea = 0.5 * Rho * A * Speed * Speed; // 0.5*rho*A*v^2

	// --- 1. DRAG -----------------------------------------------------------
	// Drag crisis: Cd drops as the boundary layer goes turbulent past critical
	// speed. Roughness lowers the critical speed (old balls hit the crisis sooner).
	const double Reverse = ComputeReverseRegime(Speed, Surface, Coeffs);
	Out.ReverseRegime = Reverse;
	const double Cd = FMath::Lerp(Coeffs.BaseDragCoefficient,
								   Coeffs.SupercriticalDragCoefficient, Reverse);
	Out.DragCoefficient = Cd;
	const FVector Drag = -Cd * DynPressureArea * VHat;

	// --- 2. MAGNUS (carry / dip / drift) -----------------------------------
	// Lift coefficient scales with spin ratio S = omega*r/v and saturates.
	FVector Magnus = FVector::ZeroVector;
	const double Omega = State.AngularVelocity.Size();
	if (Omega > KINDA_SMALL_NUMBER)
	{
		const double SpinRatio = (Omega * BallRadiusM) / Speed;
		const double Cl = FMath::Min(Coeffs.MagnusLiftSlope * SpinRatio,
									 Coeffs.MaxMagnusLiftCoefficient);
		Out.MagnusLiftCoefficient = Cl;

		// Direction = omega x v (verified against UE left-handed cross product:
		// backspin -> lift up, topspin -> dip, sidespin -> lateral drift).
		const FVector MagnusDir = FVector::CrossProduct(State.AngularVelocity, VHat);
		const double MagnusDirLen = MagnusDir.Size();
		if (MagnusDirLen > KINDA_SMALL_NUMBER)
		{
			Magnus = (MagnusDir / MagnusDirLen) * (Cl * DynPressureArea);
		}
	}

	// --- 3. SWING (conventional + reverse) ---------------------------------
	// Seam angle = signed angle between velocity and the seam plane:
	// theta = asin(vHat . seamNormal). Lateral direction = seam-normal component
	// perpendicular to velocity.
	FVector Swing = FVector::ZeroVector;
	const FVector SeamN = State.SeamNormal.GetSafeNormal();
	if (!SeamN.IsNearlyZero())
	{
		const double SinTheta = FMath::Clamp(FVector::DotProduct(VHat, SeamN), -1.0, 1.0);
		const double SeamAngle = FMath::Asin(SinTheta);
		Out.SeamAngleRad = SeamAngle;

		// Lateral unit vector: seam normal projected perpendicular to velocity.
		FVector Lateral = SeamN - SinTheta * VHat;
		const double LatLen = Lateral.Size();
		if (LatLen > KINDA_SMALL_NUMBER)
		{
			Lateral /= LatLen;

			const double Shape = SeamShape(SeamAngle, Coeffs.OptimalSeamAngleRad);

			// Conventional swing needs a shiny/laminar side; reverse needs a
			// rough surface and pace. They push in opposite lateral directions.
			const double ConvGate = (1.0 - Reverse) * FMath::Abs(Surface.ShineAsymmetry);
			const double RevGate  = Reverse * (0.5 + 0.5 * Surface.Roughness);

			const double Cs = Coeffs.MaxSwingSideForceCoefficient * Shape
							* Surface.SeamProudness * (ConvGate - RevGate);
			Out.SwingSideForceCoefficient = Cs;

			Swing = Lateral * (Cs * DynPressureArea);
		}
	}

	Out.DragForce = Drag;
	Out.MagnusForce = Magnus;
	Out.SwingForce = Swing;
	Out.Force = Drag + Magnus + Swing;

	// --- 4. SPIN DECAY -----------------------------------------------------
	Out.AngularAccel = -Coeffs.SpinDecayRate * State.AngularVelocity;

	// --- 5. WOBBLE SEAM ----------------------------------------------------
	// The seam normal precesses about the flight line, oscillating its
	// presentation angle. This makes late swing direction inconsistent and
	// randomises seam orientation at landing — the hallmark of the wobble ball.
	// Effective wobble grows as the seam destabilises: a held seam (stability 1)
	// does not wobble; a scrambled seam (stability 0) wobbles at full amplitude.
	const double Instability = 1.0 - FMath::Clamp(State.SeamStability, 0.0, 1.0);
	const double EffWobbleAmp = Coeffs.WobbleSeamAmplitudeRad * Instability;
	if (EffWobbleAmp > 0.0 && Coeffs.WobbleSeamRateRadS > 0.0)
	{
		const double WobbleOmega = EffWobbleAmp * Coeffs.WobbleSeamRateRadS
			* FMath::Cos(Coeffs.WobbleSeamRateRadS * State.TimeSinceRelease);
		Out.SeamAngularVelocity = VHat * WobbleOmega;
	}

	return Out;
}
