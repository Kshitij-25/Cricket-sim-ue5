#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.h"
#include "CricketPitchInteraction.generated.h"

/**
 * FCricketSurfacePatch — the local pitch condition at a single point of impact.
 *
 * This is the heart of the pitch SIMULATION: bounce, pace, turn and seam
 * movement all EMERGE from these surface properties at the spot where the ball
 * lands, never from a random number applied to an outcome. The gameplay layer
 * samples one of these from a UCricketPitchProfileAsset (per length zone, with
 * wear/footmarks/moisture folded in) and hands it to FCricketPitchInteraction.
 *
 * All seven brief-mandated material parameters are first-class fields here:
 *   Hardness, Roughness, Moisture, GrassCoverage, Friction, Restitution, Wear.
 * Unevenness is the eighth — the deterministic bounce-CONSISTENCY driver.
 *
 * Ranges are chosen so the struct's DEFAULTS describe a fair, balanced Test
 * surface; the FCricketPitchMaterialLibrary presets push them toward the three
 * shipping personalities (hard, dry, green).
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketSurfacePatch
{
	GENERATED_BODY()

	/** Hardness [0,1]. 1: rock-hard (high, true, fast bounce). 0: soft/wet (low, dead). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Hardness = 0.7;

	/**
	 * Surface roughness/abrasiveness [0,1]. Distinct from Friction: roughness is
	 * the micro-texture that lets a spinning ball BITE (more turn) and scuffs the
	 * ball over time. Dry/worn tracks are rough; a fresh rolled pitch is smooth.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Roughness = 0.15;

	/** Moisture [0,1]. Saps restitution (ball "stops on" the batter), greases the
	 *  surface (early skid, less turn) and adds seam grip under the lacquer. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Moisture = 0.1;

	/**
	 * Grass coverage [0,1]. 0: bare/dust. 1: lush green top. Live grass binds the
	 * surface (truer, often extra bounce), skids the ball on early (pace, less
	 * turn) and — crucially — grips a landing seam, which is what makes a GREEN
	 * pitch seam around. Dries out / is worn away over a match.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double GrassCoverage = 0.2;

	/** Coulomb friction coefficient (base). Dry/abrasive ~0.5; greasy/wet lower;
	 *  turning tracks higher. The spin solver modulates this by roughness/wear/
	 *  moisture/grass to get the effective grip. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0"))
	double Friction = 0.45;

	/**
	 * Restitution coefficient base [0,1] — the surface's intrinsic "bounciness"
	 * at low impact speed. The bounce solver modulates it by hardness, moisture,
	 * grass and impact speed to get the effective restitution. Hard pitches ~0.6;
	 * dead/wet pitches ~0.35.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	double Restitution = 0.5;

	/**
	 * Wear level [0,1] at this spot. 0: fresh. 1: heavily used (day-5 / footmark).
	 * Wear lowers effective hardness, raises roughness/grip (dust) and unevenness.
	 * Authored per-zone AND raised globally by the profile's day progression.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Wear = 0.0;

	/**
	 * Local unevenness [0,1] from cracks/footmarks. Scales the deterministic
	 * bounce perturbation supplied via FCricketImpact::Variance — i.e. it is the
	 * inverse of bounce CONSISTENCY. 0 = a true pitch; 1 = a minefield.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Unevenness = 0.0;
};

/**
 * FCricketImpact — per-bounce inputs beyond the surface and ball state.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketImpact
{
	GENERATED_BODY()

	/** Contact normal (unit), world axes. Flat pitch = +Z. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch")
	FVector ContactNormal = FVector(0.0, 0.0, 1.0);

	/**
	 * How squarely the seam strikes the surface [0,1]. 1: seam lands flush
	 * (maximum seam movement); 0: lands on the smooth leather (no seam deviation).
	 * Driven by SeamNormal orientation at the moment of contact.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SeamContact = 0.0;

	/**
	 * Deterministic perturbation in [-1,1] supplied by the caller (e.g. hashed
	 * from the landing position). Combined with patch Unevenness to produce
	 * repeatable-but-varied bounce. Keeping it an input preserves determinism.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Pitch", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	double Variance = 0.0;
};

/**
 * FCricketBounceReport — diagnostic summary of what one bounce did. Consumed by
 * the debug overlay (bounce angle, friction, turn, seam deviation) and by the
 * automation tests that compare pitches. Purely descriptive; never an input.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBounceReport
{
	GENERATED_BODY()

	/** Effective normal restitution actually applied this bounce. */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double RestitutionUsed = 0.0;

	/** True if the ball gripped (bit) rather than skidded on. */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") bool   bGripped = false;

	/** Grip level [0,1]: fraction of the slip the surface could arrest (1 = full grip). */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double GripLevel = 0.0;

	/** Effective Coulomb friction coefficient used (after surface modulation). */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double FrictionUsed = 0.0;

	/** Total lateral (cross-line) deviation imparted by the bounce (m/s): turn + seam. */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double LateralDeviationMS = 0.0;

	/** Lateral component from spin grip alone — the TURN off the pitch (m/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double TurnMS = 0.0;

	/** Lateral component from the seam striking the surface — SEAM movement (m/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double SeamDeviationMS = 0.0;

	/** Fraction of incoming speed retained after the bounce (pace off the pitch). */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double SpeedRetainedFrac = 0.0;

	/** Incoming angle below horizontal at contact (deg). */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double IncomingAngleDeg = 0.0;

	/** Outgoing angle above horizontal after the bounce (deg) — the BOUNCE ANGLE. */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double BounceAngleDeg = 0.0;

	/** Ballistic apex height the ball would reach after this bounce (m), aero ignored. */
	UPROPERTY(BlueprintReadOnly, Category = "Pitch") double BounceHeightM = 0.0;
};

/**
 * FCricketBounceContext — the working set shared between the three solvers for a
 * single bounce. The orchestrator (FCricketPitchInteraction) decomposes the
 * incoming state ONCE, then the bounce / spin / seam solvers each read what they
 * need and accumulate their contribution. This keeps the solvers independent and
 * testable while avoiding recomputing the velocity decomposition three times.
 *
 * All SI. The post-bounce velocity is assembled as:
 *   NewVelocity = NewVNormal + VTangent + TangImpulse + SeamImpulse
 */
struct FCricketBounceContext
{
	// --- Filled by the orchestrator (incoming decomposition) ---------------
	FVector N = FVector(0, 0, 1);          // unit contact normal
	double  Vn = 0.0;                       // signed normal speed (negative = approaching)
	double  ImpactSpeed = 0.0;              // |Vn|
	FVector VNormal = FVector::ZeroVector;  // normal component of incoming velocity
	FVector VTangent = FVector::ZeroVector; // tangential component of incoming velocity
	FVector InVelocity = FVector::ZeroVector;
	double  InSpeed = 0.0;

	// --- Filled by the bounce solver ---------------------------------------
	double  Restitution = 0.0;             // effective e
	double  Jn = 0.0;                       // normal impulse magnitude in delta-v units = (1+e)|Vn|
	FVector NewVNormal = FVector::ZeroVector;

	// --- Filled by the spin solver -----------------------------------------
	FVector TangImpulse = FVector::ZeroVector; // friction impulse (delta-v)

	// --- Filled by the seam solver -----------------------------------------
	FVector SeamImpulse = FVector::ZeroVector; // lateral seam-movement impulse (delta-v)
};

/**
 * FCricketPitchInteraction — the PITCH PHYSICS MODULE.
 *
 * Resolves a ball/pitch bounce with an impulse-based contact model by
 * orchestrating three focused solvers (this is the public façade the gameplay
 * layer and trajectory predictor call; the signature is intentionally stable):
 *
 *   1. FCricketBounceSolver — speed-dependent restitution, pace-off, bounce
 *      angle/height (normal response).
 *   2. FCricketSpinSolver   — Coulomb friction with a grip/skid threshold and
 *      spin->translation coupling (this is the TURN).
 *   3. FCricketSeamSolver   — seam-strike lateral deviation incl. wobble-seam
 *      inconsistency (SEAM movement off the pitch).
 *
 * Turn, seam movement and bounce variation all EMERGE here from the surface
 * properties; none are scripted. Deterministic given the same inputs.
 */
class CRICKETPHYSICS_API FCricketPitchInteraction
{
public:
	/**
	 * Resolve a single bounce. Mutates State (velocity, spin) in place and
	 * returns a diagnostic report.
	 */
	static FCricketBounceReport ResolveBounce(
		FCricketBallState& State,
		const FCricketSurfacePatch& Patch,
		const FCricketImpact& Impact);
};
