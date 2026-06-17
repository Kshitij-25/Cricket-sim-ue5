#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.h"

/**
 * FCricketBallIntegrator
 *
 * Advances an FCricketBallState through the aerodynamic force field using a
 * fixed-substep classic RK4 integrator. RK4 (not Euler) because the forces are
 * velocity-squared and spin-coupled — over a ~0.5 s delivery, error accumulates
 * visibly in swing/dip with first-order methods.
 *
 * The substep is FIXED and independent of frame rate, which makes flight
 * DETERMINISTIC and identical on macOS and Windows regardless of fps. The
 * caller passes a wall-clock delta; the integrator slices it into substeps and
 * carries the remainder.
 */
class CRICKETPHYSICS_API FCricketBallIntegrator
{
public:
	/** Fixed integration substep (s). 1 ms is ample for a cricket ball. */
	static constexpr double DefaultSubstep = 0.001;

	FCricketBallIntegrator() = default;

	FCricketBallIntegrator(const FCricketBallSurface& InSurface,
		const FCricketEnvironment& InEnvironment,
		const FCricketAeroCoefficients& InCoefficients,
		double InSubstep = DefaultSubstep);

	/**
	 * Integrate State forward by DeltaSeconds of wall-clock time. Returns the
	 * number of fixed substeps actually taken this call.
	 */
	int32 Advance(FCricketBallState& State, double DeltaSeconds);

	const FCricketBallSurface&      GetSurface() const     { return Surface; }
	const FCricketEnvironment&      GetEnvironment() const { return Environment; }
	const FCricketAeroCoefficients& GetCoefficients() const { return Coefficients; }

	void SetSurface(const FCricketBallSurface& In)         { Surface = In; }
	void SetEnvironment(const FCricketEnvironment& In)     { Environment = In; }
	void SetCoefficients(const FCricketAeroCoefficients& In) { Coefficients = In; }

	/** Per-state time derivative used by RK4 (public so step helpers can name it). */
	struct FDerivative
	{
		FVector dPosition = FVector::ZeroVector;        // = velocity
		FVector dVelocity = FVector::ZeroVector;        // = acceleration
		FVector dAngularVelocity = FVector::ZeroVector; // = angular accel
		FVector dSeam = FVector::ZeroVector;            // = seam normal rate
	};

private:
	FDerivative Evaluate(const FCricketBallState& State) const;

	FCricketBallSurface Surface;
	FCricketEnvironment Environment;
	FCricketAeroCoefficients Coefficients;

	double Substep = DefaultSubstep;
	double Accumulator = 0.0; // leftover wall-clock time between Advance calls
};
