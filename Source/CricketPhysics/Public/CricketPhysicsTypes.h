#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.generated.h"

/**
 * Bowling mode hint. The aerodynamics model does NOT branch on this to decide
 * outcomes — outcomes emerge from the state — but it scales authoring defaults
 * and lets tooling validate plausible inputs (e.g. spin rpm for a quick).
 */
UENUM(BlueprintType)
enum class ECricketDeliveryArchetype : uint8
{
	Pace        UMETA(DisplayName = "Pace"),
	SeamUp      UMETA(DisplayName = "Seam Up"),
	Swing       UMETA(DisplayName = "Swing"),
	OffSpin     UMETA(DisplayName = "Off Spin"),
	LegSpin     UMETA(DisplayName = "Leg Spin"),
	Custom      UMETA(DisplayName = "Custom")
};

/**
 * FCricketBallState — the complete dynamic state integrated each step.
 * All quantities are SI (metres, m/s, rad/s). Positions/velocities live in the
 * same axis convention as UE world space (see CricketPhysicsConstants.h).
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBallState
{
	GENERATED_BODY()

	/** Position of the ball centre (m). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|State")
	FVector Position = FVector::ZeroVector;

	/** Linear velocity (m/s). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|State")
	FVector Velocity = FVector::ZeroVector;

	/** Angular velocity / spin (rad/s). Magnitude = spin rate, direction = spin axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|State")
	FVector AngularVelocity = FVector::ZeroVector;

	/**
	 * Seam plane normal (unit, dimensionless). The seam great-circle lies in the
	 * plane perpendicular to this vector. Seam ORIENTATION relative to velocity
	 * is what generates conventional/reverse swing and seam-at-landing deviation.
	 * Carried in state so it can precess (wobble seam) during flight.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|State")
	FVector SeamNormal = FVector(0.0, 1.0, 0.0);

	/** Seconds since release. Drives time-dependent effects (wobble phase). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|State")
	double TimeSinceRelease = 0.0;

	/**
	 * Last committed linear acceleration (m/s^2), written by the integrator each
	 * step. Derived state — present in the model for debugging/telemetry and for
	 * systems that need the instantaneous force (e.g. the debug readout). Never
	 * an integration input.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|State")
	FVector Acceleration = FVector::ZeroVector;

	/**
	 * Seam stability in [0,1]. 1: a perfectly held, gyroscopically stable seam
	 * (seam-up fast bowling, big leg-spin revs). 0: a fully scrambled / wobble
	 * seam. Scales the wobble precession (see FCricketAeroCoefficients) so a
	 * "wobble-seam" delivery is simply a low-stability seam. Carried in state
	 * because a seam can destabilise in flight.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|State", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SeamStability = 1.0;

	FORCEINLINE double Speed() const { return Velocity.Size(); }

	/** Spin rate in rad/s (magnitude of angular velocity). */
	FORCEINLINE double SpinRateRadS() const { return AngularVelocity.Size(); }
};

/**
 * FCricketBallSurface — the asymmetric surface condition that makes a real ball
 * swing. Shine differential and roughness evolve over an innings; the model
 * reads them but does not own their evolution (the gameplay layer ages the ball).
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBallSurface
{
	GENERATED_BODY()

	/**
	 * Shine asymmetry in [-1, 1]. +1: the +SeamNormal side is the shiny side;
	 * -1: the -SeamNormal side is shiny. 0: symmetric (no conventional swing).
	 * The shiny side keeps a laminar boundary layer longer -> later separation.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|Surface", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	double ShineAsymmetry = 1.0;

	/**
	 * Overall surface roughness in [0, 1]. 0: pristine new ball. 1: heavily
	 * scuffed old ball. High roughness lowers the critical Reynolds number and
	 * enables reverse swing at lower speeds.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|Surface", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Roughness = 0.0;

	/**
	 * Seam proudness in [0, 1]. 1: new, proud, lacquered seam that trips the
	 * boundary layer strongly. 0: worn flat. Scales swing & seam-movement force.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ball|Surface", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SeamProudness = 1.0;
};

/**
 * FCricketEnvironment — atmospheric inputs that scale every aerodynamic force.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketEnvironment
{
	GENERATED_BODY()

	/** Air temperature (Celsius). Affects density. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Environment")
	double TemperatureC = 22.0;

	/** Relative humidity [0,1]. Humid air is slightly LESS dense. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Environment", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double RelativeHumidity = 0.5;

	/** Station pressure (hPa / millibar). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Environment")
	double PressureHpa = 1013.25;

	/** Ground-level wind velocity (m/s), world axes. Affects apparent airflow. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Environment")
	FVector Wind = FVector::ZeroVector;

	/** Computed air density (kg/m^3) from the above. See CricketAerodynamics. */
	double ComputeAirDensity() const;
};

/**
 * FCricketAeroCoefficients — the tunable coefficient set, separated from the
 * physical ball constants so designers can author "this ball, this day" without
 * touching code. Mirrored by UCricketBallProfileAsset for data-driven setups.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketAeroCoefficients
{
	GENERATED_BODY()

	/** Baseline drag coefficient (subcritical, smooth). Typical 0.40–0.50. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Drag")
	double BaseDragCoefficient = 0.45;

	/** Drag in the supercritical (post drag-crisis) regime. Typical 0.20–0.30. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Drag")
	double SupercriticalDragCoefficient = 0.24;

	/** Peak side-force coefficient achievable from seam swing. Typical ~0.3. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Swing")
	double MaxSwingSideForceCoefficient = 0.30;

	/** Seam angle (radians) giving maximum conventional swing. ~20 deg. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Swing")
	double OptimalSeamAngleRad = 0.349; // 20 degrees

	/** Magnus lift-curve slope vs spin ratio S = omega*r/v. Typical ~0.4–0.6. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Magnus")
	double MagnusLiftSlope = 0.5;

	/** Cap on Magnus lift coefficient (saturates at high spin ratio). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Magnus")
	double MaxMagnusLiftCoefficient = 0.45;

	/** Spin decay rate (1/s) from air torque. Small; ~0.05–0.2. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Spin")
	double SpinDecayRate = 0.1;

	/** Wobble-seam precession rate (rad/s) of the seam normal when "scrambled". */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Wobble")
	double WobbleSeamRateRadS = 0.0;

	/** Wobble-seam half-angle (radians) the seam normal oscillates through. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Wobble")
	double WobbleSeamAmplitudeRad = 0.0;

	/** Per-profile critical speed override (m/s) for the conventional->reverse flip. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aero|Swing")
	double SwingTransitionSpeed = 30.0;
};
