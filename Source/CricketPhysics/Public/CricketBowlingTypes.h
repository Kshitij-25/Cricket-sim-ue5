#pragma once

#include "CoreMinimal.h"
#include "CricketPhysicsTypes.h"
#include "CricketPitchInteraction.h"
#include "CricketBowlingTypes.generated.h"

/**
 * CricketBowlingTypes
 *
 * The data model for the bowling system. It turns a human/AI INTENT (line,
 * length, pace, movement, swing/spin amount) into a complete set of physical
 * RELEASE PARAMETERS that feed the existing deterministic ball-physics core
 * (UCricketBallPhysicsComponent::ReleaseEx + the FCricketBallState it integrates).
 *
 * Nothing here scripts an outcome. The generator only decides the ball's
 * physical condition at the instant it leaves the hand — release velocity,
 * position, angle, seam orientation, spin axis, spin rate, wrist position and
 * ball condition. Swing, dip, drift, seam movement and turn then EMERGE from the
 * aerodynamic + pitch model exactly as for any other ball state.
 *
 * UNITS & AXES match CricketPhysics throughout: SI (m, m/s, rad/s, radians) and
 * the world axes +X down the pitch toward the striker, +Y the off side for a
 * right-hander, +Z up. The cm<->m boundary is crossed only in the gameplay layer
 * (the controller), never here.
 */

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------

/**
 * Which arm the bowler delivers with. The arm sets which side of the stumps the
 * ball is released from (and thus the natural angle across the batsman). Movement
 * labels (out/in-swing, off/leg-break) are defined RELATIVE TO THE STRIKER and are
 * arm-independent: an outswinger always leaves a right-hander (+Y) whoever bowls it.
 */
UENUM(BlueprintType)
enum class ECricketBowlingArm : uint8
{
	RightArm UMETA(DisplayName = "Right Arm"),
	LeftArm  UMETA(DisplayName = "Left Arm")
};

/** Angle of delivery relative to the stumps — shifts the release point in Y and the natural angle across the batter. */
UENUM(BlueprintType)
enum class ECricketDeliverySide : uint8
{
	OverTheWicket  UMETA(DisplayName = "Over the Wicket"),
	RoundTheWicket UMETA(DisplayName = "Round the Wicket")
};

/**
 * Length band — where the ball pitches, measured from the striker's stumps back
 * toward the bowler. This is the primary "length" control. The generator solves
 * the release elevation so the ball actually bounces in the chosen band.
 */
UENUM(BlueprintType)
enum class ECricketLength : uint8
{
	FullToss     UMETA(DisplayName = "Full Toss"),      // does not pitch before the batter
	Yorker       UMETA(DisplayName = "Yorker"),         // at the base of the stumps / boots
	Full         UMETA(DisplayName = "Full / Half-Volley"),
	GoodLength   UMETA(DisplayName = "Good Length"),
	BackOfLength UMETA(DisplayName = "Back of a Length"),
	Short        UMETA(DisplayName = "Short"),
	Bouncer      UMETA(DisplayName = "Bouncer")         // pitched short enough to rise to chest/head
};

/**
 * Aim line — the lateral channel the bowler aims at, measured at the striker's
 * stumps. Swing/spin then deviate the ball off this line (emergent). +Y is the
 * off side for a right-hander; mirrored for a left-hander.
 */
UENUM(BlueprintType)
enum class ECricketLine : uint8
{
	WideOutsideOff UMETA(DisplayName = "Wide Outside Off"),
	OutsideOff     UMETA(DisplayName = "Outside Off (4th/5th stump)"),
	OffStump       UMETA(DisplayName = "Off Stump"),
	Middle         UMETA(DisplayName = "Middle Stump"),
	LegStump       UMETA(DisplayName = "Leg Stump"),
	DownLeg        UMETA(DisplayName = "Down Leg")
};

/**
 * Movement archetype — how the ball behaves in the air and off the pitch. This
 * selects the physical mechanism (seam cant, spin axis, seam stability, surface
 * regime); it is orthogonal to length and line (you can bowl an in-swinging
 * yorker or a back-of-a-length off-break). The generator infers the bowling
 * family (pace vs spin) from this.
 */
UENUM(BlueprintType)
enum class ECricketMovement : uint8
{
	SeamUp       UMETA(DisplayName = "Seam Up (straight)"),  // held seam, gyroscopic stability, no deliberate swing
	Outswing     UMETA(DisplayName = "Outswing"),            // away from a RH bat: deviates +Y
	Inswing      UMETA(DisplayName = "Inswing"),             // into a RH bat: deviates -Y
	ReverseSwing UMETA(DisplayName = "Reverse Swing"),       // rough ball + pace: the regime flips the side force
	WobbleSeam   UMETA(DisplayName = "Wobble Seam"),         // scrambled seam: precesses, late inconsistent movement
	OffBreak     UMETA(DisplayName = "Off Break"),           // finger spin: turns -Y off the pitch (into RH bat)
	LegBreak     UMETA(DisplayName = "Leg Break")            // wrist spin: turns +Y off the pitch (away from RH bat)
};

/** Coarse family the movement belongs to (drives default pace, RPM and seam handling). */
UENUM(BlueprintType)
enum class ECricketBowlingStyle : uint8
{
	Pace    UMETA(DisplayName = "Pace / Seam"),
	Swing   UMETA(DisplayName = "Swing"),
	OffSpin UMETA(DisplayName = "Off Spin"),
	LegSpin UMETA(DisplayName = "Leg Spin")
};

/**
 * Wrist/hand position at release — a physical descriptor recorded with every
 * delivery (one of the eight required release parameters). It summarises how the
 * seam is presented and the spin axis tilted; the generator sets it from the
 * movement archetype.
 */
UENUM(BlueprintType)
enum class ECricketWristPosition : uint8
{
	BehindSeamUp  UMETA(DisplayName = "Behind the Ball, Seam Up"),   // classic pace/seam
	CantedOut     UMETA(DisplayName = "Canted for Outswing"),
	CantedIn      UMETA(DisplayName = "Canted for Inswing"),
	Scrambled     UMETA(DisplayName = "Scrambled (Wobble)"),
	FingerSpin    UMETA(DisplayName = "Finger Spin (Off)"),
	WristSpin     UMETA(DisplayName = "Wrist Spin (Leg)")
};

/** Which aerodynamic swing regime a delivery is predicted to be in (diagnostic). */
UENUM(BlueprintType)
enum class ECricketSwingRegime : uint8
{
	None         UMETA(DisplayName = "None"),
	Conventional UMETA(DisplayName = "Conventional"),
	Reverse      UMETA(DisplayName = "Reverse")
};

// ---------------------------------------------------------------------------
// Field geometry (bowling-scoped; the pure aero model is free of field layout)
// ---------------------------------------------------------------------------

namespace CricketField
{
	/** Stump-to-stump pitch length (m). MCC Law 6: 22 yards. */
	inline constexpr double PitchLengthM = 20.12;

	/** Horizontal distance from the bowler's release point to the striker's stumps (m).
	 *  Popping crease to popping crease is 17.68 m; a quick releases ~1 m past the
	 *  bowling crease, so ~17.7 m of carry is representative. Tunable per action. */
	inline constexpr double DefaultReleaseToStumpsM = 17.7;

	/** Stump height above the ground (m). MCC Law 8: 28 in. */
	inline constexpr double StumpHeightM = 0.711;

	/** Overall width across the three stumps (m). MCC Law 8: 9 in. */
	inline constexpr double StumpsWidthM = 0.2286;

	/** Half a stump's spacing — off/leg stump centre offset from middle (m). */
	inline constexpr double StumpHalfSpacingM = StumpsWidthM * 0.5; // ~0.114 m

	/** Default release height for a tall pace bowler (m). */
	inline constexpr double DefaultPaceReleaseHeightM = 2.1;

	/** Default release height for a spinner (m). */
	inline constexpr double DefaultSpinReleaseHeightM = 1.95;
}

// ---------------------------------------------------------------------------
// Intent — the human/AI control input
// ---------------------------------------------------------------------------

/**
 * FCricketBowlingIntent — what the bowler is trying to do. The five MVP control
 * axes (line, length, pace, swing amount, spin amount) plus the movement
 * archetype and a couple of fine offsets. This is the sole input to the
 * generator besides the bowler's action and the world context.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBowlingIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent")
	ECricketBowlingArm Arm = ECricketBowlingArm::RightArm;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent")
	ECricketDeliverySide Side = ECricketDeliverySide::OverTheWicket;

	/** Air/pitch behaviour. Decides the physical mechanism (seam/spin/regime). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent")
	ECricketMovement Movement = ECricketMovement::SeamUp;

	/** Length band — where it pitches (solved into a release elevation). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent")
	ECricketLength Length = ECricketLength::GoodLength;

	/** Aim line — channel at the stumps (mapped to a release azimuth). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent")
	ECricketLine Line = ECricketLine::OffStump;

	/** Fine length trim added to the band's target (m). + = fuller (nearer batter). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent")
	double LengthFineM = 0.0;

	/** Fine line trim added to the aim (m). + = wider to the off side (RH). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent")
	double LineFineM = 0.0;

	/** Pace dial in [0,1] mapped across the action's min..max pace. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Pace01 = 0.8;

	/** Swing intensity in [0,1] — scales the seam angle toward the optimal swing angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SwingAmount = 0.6;

	/** Spin intensity in [0,1] — scales RPM across the action's max for the chosen spin. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Intent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SpinAmount = 0.7;
};

/**
 * FCricketDeliveryPreset — a named bundle that fills an intent's movement, length
 * and dials in one keystroke. The MVP set covers the eight required deliveries
 * plus the four fast lengths. Presets only seed the intent; the player can still
 * fine-tune every axis afterwards.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketDeliveryPreset
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Preset")
	FName DisplayName = TEXT("Good Length");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Preset")
	ECricketMovement Movement = ECricketMovement::SeamUp;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Preset")
	ECricketLength Length = ECricketLength::GoodLength;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Preset")
	ECricketLine Line = ECricketLine::OffStump;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Pace01 = 0.85;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SwingAmount = 0.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SpinAmount = 0.0;

	/** Apply this preset onto an intent (preserving arm/side). */
	void ApplyTo(FCricketBowlingIntent& Intent) const
	{
		Intent.Movement    = Movement;
		Intent.Length      = Length;
		Intent.Line        = Line;
		Intent.Pace01      = Pace01;
		Intent.SwingAmount = SwingAmount;
		Intent.SpinAmount  = SpinAmount;
	}
};

// ---------------------------------------------------------------------------
// Bowler action — the physical capabilities of a bowler
// ---------------------------------------------------------------------------

/**
 * FCricketBowlingAction — a bowler's repeatable physical action: how high and
 * wide they release, their arm slot, their pace range and the spin they can
 * impart. These are the limits the generator works within; the intent dials
 * select a point inside them. Authored as data (UCricketBowlingActionAsset).
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBowlingAction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action")
	FName BowlerName = TEXT("Express Quick");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action")
	ECricketBowlingArm Arm = ECricketBowlingArm::RightArm;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action")
	ECricketBowlingStyle PrimaryStyle = ECricketBowlingStyle::Pace;

	/** Release height above the ground (m). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action", meta = (ClampMin = "1.2", ClampMax = "2.6"))
	double ReleaseHeightM = CricketField::DefaultPaceReleaseHeightM;

	/** Lateral release offset from the stumps line (m). Positive toward the bowling arm's side. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action")
	double ReleaseWidthM = 0.35;

	/** Horizontal carry from release to the striker's stumps (m). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action", meta = (ClampMin = "10.0", ClampMax = "20.12"))
	double ReleaseToStumpsM = CricketField::DefaultReleaseToStumpsM;

	/** Arm slot measured from vertical (deg). 0 = high/straight; ~45 = round-arm. Tilts seam cant and spin axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	double ArmSlotDeg = 18.0;

	/** Slowest stock pace this bowler delivers (km/h). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action", meta = (ClampMin = "40.0", ClampMax = "160.0"))
	double MinPaceKmh = 125.0;

	/** Fastest pace this bowler delivers (km/h). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action", meta = (ClampMin = "40.0", ClampMax = "165.0"))
	double MaxPaceKmh = 145.0;

	/** Maximum revolutions this bowler can impart on a spinning delivery (rpm). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action", meta = (ClampMin = "0.0", ClampMax = "3500.0"))
	double MaxSpinRPM = 2400.0;

	/** Backspin imparted on a stock seam-up / swing delivery to hold the seam (rpm). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action", meta = (ClampMin = "0.0", ClampMax = "2500.0"))
	double StockBackspinRPM = 1300.0;

	/** Seam stability of a well-held seam in [0,1] (1 = rock steady; wobble overrides lower). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double HeldSeamStability = 0.95;

	/** Named deliveries this bowler offers (for the control scheme's preset slots). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Action")
	TArray<FCricketDeliveryPreset> Presets;
};

// ---------------------------------------------------------------------------
// Release parameters — the generator's output (the physical delivery)
// ---------------------------------------------------------------------------

/**
 * FCricketReleaseParameters — the complete, physically-meaningful description of
 * one delivery at the instant of release. Contains the eight release parameters
 * the brief requires plus the exact quantities the physics core consumes. All SI;
 * positions in metres in world axes. The controller maps these onto
 * UCricketBallPhysicsComponent::ReleaseEx (converting m->cm for position only).
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketReleaseParameters
{
	GENERATED_BODY()

	// --- The eight required release parameters -----------------------------

	/** 1. Release velocity — magnitude (m/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	double ReleaseSpeedMS = 38.0;

	/** Release velocity as a full world vector (m/s) = direction * speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	FVector ReleaseVelocityMS = FVector(38.0, 0.0, 0.0);

	/** 2. Release position (m, world axes). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	FVector ReleasePositionM = FVector::ZeroVector;

	/** 3a. Release angle — elevation above horizontal (deg). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	double ReleaseElevationDeg = 0.0;

	/** 3b. Release angle — azimuth from straight-down-the-pitch (deg, + toward +Y/off). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	double ReleaseAzimuthDeg = 0.0;

	/** 4. Seam orientation — unit seam-plane normal. */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	FVector SeamNormal = FVector(0.0, 1.0, 0.0);

	/** 5. Spin axis — unit angular-velocity direction. */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	FVector SpinAxis = FVector(0.0, -1.0, 0.0);

	/** 6. Spin rate (rpm). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	double SpinRateRPM = 0.0;

	/** 7. Wrist position. */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	ECricketWristPosition WristPosition = ECricketWristPosition::BehindSeamUp;

	/** 8. Ball condition (shine/roughness/seam) applied to the integrator's surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	FCricketBallSurface BallCondition;

	// --- Derived quantities the model consumes -----------------------------

	/** Angular velocity (rad/s) = SpinAxis * RpmToRadS(SpinRateRPM). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	FVector AngularVelocityRadS = FVector::ZeroVector;

	/** Seam stability in [0,1] carried into FCricketBallState (low => wobble). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double SeamStability = 0.95;

	/** Per-delivery aerodynamic coefficients (wobble rate/amplitude, swing transition). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	FCricketAeroCoefficients Coefficients;

	/** Archetype hint for tooling/validation (does not affect the physics). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Release")
	ECricketDeliveryArchetype Archetype = ECricketDeliveryArchetype::SeamUp;
};

/**
 * FCricketDeliveryDiagnostics — predicted, physics-derived analysis of a
 * generated delivery, for the debug overlay and the automation tests. Produced
 * by integrating the release parameters through the very same model the live
 * ball uses, so the prediction equals the actual flight.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketDeliveryDiagnostics
{
	GENERATED_BODY()

	/** First predicted bounce point (m, world). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Diagnostics")
	FVector PredictedPitchPointM = FVector::ZeroVector;

	/** Predicted length: distance of the pitch point from the striker's stumps (m). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Diagnostics")
	double PredictedLengthM = 0.0;

	/** Predicted lateral line at the pitch point (m, signed; + = off side for RH). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Diagnostics")
	double PredictedLineAtPitchM = 0.0;

	/**
	 * Free-flight lateral swing (m): how far the ball would deviate sideways by
	 * the pitch point if it were aimed straight down the pitch. This is the honest
	 * "amount of swing" (sign: + = toward +Y/off). Independent of the aim azimuth.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Diagnostics")
	double FreeFlightSwingM = 0.0;

	/** Whether the length aim-solve converged within tolerance. */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Diagnostics")
	bool bAimConverged = false;

	/** Residual length error after the aim solve (m). */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Diagnostics")
	double AimResidualM = 0.0;

	/** Predicted swing regime at release speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Bowling|Diagnostics")
	ECricketSwingRegime Regime = ECricketSwingRegime::None;
};

/**
 * FCricketDeliveryContext — the world conditions the generator solves within.
 * Supplied by the controller (which owns the cm<->m boundary and the ball's
 * current condition). All SI; positions in metres in world axes.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketDeliveryContext
{
	GENERATED_BODY()

	/** Release point (m, world). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Context")
	FVector ReleasePositionM = FVector::ZeroVector;

	/** Striker's stumps base (m, world) — the line/length reference. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Context")
	FVector StrikerStumpsM = FVector(CricketField::DefaultReleaseToStumpsM, 0.0, 0.0);

	/**
	 * World height of the pitch plane the ball bounces on (m). The aim solver and
	 * the predicted overlays resolve the bounce at this Z, so it MUST equal the
	 * world height of the actual pitch/floor collision the live ball sweeps against
	 * (the controller sets it from the striker's ground level). Defaults to 0.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Context")
	double GroundPlaneZM = 0.0;

	/** Atmosphere used for the aim solve and carried to the ball. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Context")
	FCricketEnvironment Environment;

	/**
	 * Reserved. The current aim solve stops at the FIRST bounce point and so needs
	 * only the pre-bounce flight, not a pitch surface — this field is not read yet.
	 * The live ball's bounces use the controller's PitchSurface. Kept for a future
	 * multi-bounce aim solve.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Context")
	FCricketSurfacePatch AimPitchSurface;

	/** Current ball condition (ages over the innings; reverse needs roughness). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Context")
	FCricketBallSurface BallCondition;

	/** Deterministic seed for human release scatter (same seed => same delivery). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Context")
	int32 Seed = 0;

	/** Human inaccuracy in [0,1]: 0 = a machine, ~1 = a club bowler. Scatters inputs only. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Bowling|Context", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double HumanScatter = 0.0;
};
