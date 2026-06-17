#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.h"

/**
 * Instantaneous aerodynamic result for one evaluation of the force model.
 * Gravity is NOT included here — it is added by the integrator so this stays a
 * pure aerodynamic model. Quantities are SI.
 */
struct FCricketAeroResult
{
	/** Net aerodynamic force on the ball centre of mass (N). */
	FVector Force = FVector::ZeroVector;

	// Separated components (N) — sum to Force. Exposed for debug visualisation.
	FVector DragForce = FVector::ZeroVector;
	FVector MagnusForce = FVector::ZeroVector;
	FVector SwingForce = FVector::ZeroVector;

	/** Angular acceleration from air torque, i.e. spin decay (rad/s^2). */
	FVector AngularAccel = FVector::ZeroVector;

	/** Angular velocity of the seam normal itself — the wobble-seam precession (rad/s). */
	FVector SeamAngularVelocity = FVector::ZeroVector;

	// --- Diagnostics (for tuning HUD / unit tests; not used by integration) ---
	double DragCoefficient = 0.0;
	double MagnusLiftCoefficient = 0.0;
	double SwingSideForceCoefficient = 0.0;
	double SeamAngleRad = 0.0;
	double ReynoldsNumber = 0.0;
	double ReverseRegime = 0.0; // 0 = conventional, 1 = reverse
};

/**
 * FCricketAerodynamics
 *
 * Stateless evaluator for the full cricket-ball aerodynamic model:
 *   - quadratic drag with a Reynolds-driven drag-crisis transition
 *   - Magnus force from spin (carry, dip, drift) via a saturating lift curve
 *   - conventional AND reverse swing from seam presentation + surface state
 *   - wobble-seam precession of the seam normal
 *   - spin decay torque
 *
 * Every phenomenon emerges from the state + surface + environment; nothing is
 * scripted per delivery. The same function evaluates a 145 km/h reverse-swinging
 * yorker and a 80 km/h drifting off-break.
 */
class CRICKETPHYSICS_API FCricketAerodynamics
{
public:
	/** Evaluate the aerodynamic force/torque for the given instantaneous state. */
	static FCricketAeroResult Evaluate(
		const FCricketBallState& State,
		const FCricketBallSurface& Surface,
		const FCricketEnvironment& Environment,
		const FCricketAeroCoefficients& Coeffs);

	/** Reynolds number for a given airspeed (m/s) and air density (kg/m^3). */
	static double ComputeReynolds(double AirSpeed, double AirDensity);

	/**
	 * Reverse-swing regime factor in [0,1]: 0 = fully conventional, 1 = fully
	 * reverse. Rises with airspeed and surface roughness around the transition.
	 */
	static double ComputeReverseRegime(double AirSpeed, const FCricketBallSurface& Surface,
		const FCricketAeroCoefficients& Coeffs);
};
