#include "CricketBallIntegrator.h"
#include "CricketAerodynamics.h"
#include "CricketPhysicsConstants.h"

using namespace CricketPhysics;

FCricketBallIntegrator::FCricketBallIntegrator(const FCricketBallSurface& InSurface,
	const FCricketEnvironment& InEnvironment,
	const FCricketAeroCoefficients& InCoefficients,
	double InSubstep)
	: Surface(InSurface)
	, Environment(InEnvironment)
	, Coefficients(InCoefficients)
	, Substep(InSubstep > 0.0 ? InSubstep : DefaultSubstep)
{
}

FCricketBallIntegrator::FDerivative
FCricketBallIntegrator::Evaluate(const FCricketBallState& State) const
{
	const FCricketAeroResult Aero =
		FCricketAerodynamics::Evaluate(State, Surface, Environment, Coefficients);

	FDerivative D;
	D.dPosition = State.Velocity;
	// Newton: a = F/m + gravity. Gravity added here so the aero model stays pure.
	D.dVelocity = (Aero.Force / BallMassKg) + FVector(0.0, 0.0, -GravityMS2);
	D.dAngularVelocity = Aero.AngularAccel;
	// Seam normal evolves as a rigid rotation: dn/dt = omega_seam x n.
	D.dSeam = FVector::CrossProduct(Aero.SeamAngularVelocity, State.SeamNormal);
	return D;
}

namespace
{
	// Apply a derivative to a base state to form an intermediate sample state.
	FCricketBallState Step(const FCricketBallState& Base,
		const FCricketBallIntegrator::FDerivative& D, double Dt)
	{
		FCricketBallState S = Base;
		S.Position        = Base.Position        + D.dPosition * Dt;
		S.Velocity        = Base.Velocity        + D.dVelocity * Dt;
		S.AngularVelocity = Base.AngularVelocity + D.dAngularVelocity * Dt;
		S.SeamNormal      = Base.SeamNormal      + D.dSeam * Dt;
		S.TimeSinceRelease = Base.TimeSinceRelease + Dt;
		return S;
	}
}

int32 FCricketBallIntegrator::Advance(FCricketBallState& State, double DeltaSeconds)
{
	if (DeltaSeconds <= 0.0)
	{
		return 0;
	}

	Accumulator += DeltaSeconds;
	int32 Steps = 0;

	while (Accumulator >= Substep)
	{
		const double H = Substep;

		// Classic RK4.
		const FDerivative K1 = Evaluate(State);
		const FDerivative K2 = Evaluate(Step(State, K1, H * 0.5));
		const FDerivative K3 = Evaluate(Step(State, K2, H * 0.5));
		const FDerivative K4 = Evaluate(Step(State, K3, H));

		auto Weighted = [](const FVector& a, const FVector& b, const FVector& c, const FVector& d)
		{
			return (a + 2.0 * b + 2.0 * c + d) * (1.0 / 6.0);
		};

		const FVector AccelThisStep = Weighted(K1.dVelocity, K2.dVelocity, K3.dVelocity, K4.dVelocity);
		State.Acceleration = AccelThisStep; // committed accel, for telemetry/debug

		State.Position += Weighted(K1.dPosition, K2.dPosition, K3.dPosition, K4.dPosition) * H;
		State.Velocity += AccelThisStep * H;
		State.AngularVelocity += Weighted(K1.dAngularVelocity, K2.dAngularVelocity,
										  K3.dAngularVelocity, K4.dAngularVelocity) * H;
		State.SeamNormal += Weighted(K1.dSeam, K2.dSeam, K3.dSeam, K4.dSeam) * H;

		// Seam normal is a direction; keep it unit length after integration drift.
		State.SeamNormal = State.SeamNormal.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0.0, 1.0, 0.0));

		State.TimeSinceRelease += H;
		Accumulator -= H;
		++Steps;
	}

	return Steps;
}
