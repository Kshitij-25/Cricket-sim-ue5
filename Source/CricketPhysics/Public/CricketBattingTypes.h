#pragma once

#include "CoreMinimal.h"
#include "CricketBatTypes.h"
#include "CricketBattingTypes.generated.h"

/**
 * CricketBattingTypes — data models for the BATTING MOTION layer.
 *
 * This is the input/intent side of the bat: it decides how the bat MOVES through
 * space over time (backlift -> downswing -> follow-through), and where the player
 * commits to meet the ball (footwork). It never decides an outcome. The existing
 * FCricketBatCollision still resolves every contact; the only thing this layer
 * does is put the bat in the right place, at the right speed, at the right time —
 * so that middling vs edging EMERGES from whether the swing met the ball, exactly
 * as on a real pitch.
 *
 * Axis convention matches the ball physics (CricketPhysicsConstants.h):
 *   +X = down the pitch toward the striker (so the bowler is at -X of the striker)
 *   +Y = off side for a right-hander, +Z = up.  Left-handers mirror in Y.
 * All quantities are SI (metres, m/s, rad/s, seconds).
 */

/** Where the striker's weight has gone — sets reach and the natural contact zone. */
UENUM(BlueprintType)
enum class ECricketFootwork : uint8
{
	Neutral    UMETA(DisplayName = "Neutral Stance"),
	FrontFoot  UMETA(DisplayName = "Front Foot"),   // press forward: more reach, contact further down the pitch
	BackFoot   UMETA(DisplayName = "Back Foot")     // rock back: contact later and higher, room for the short ball
};

/** The phase the bat is in along a single stroke. Purely descriptive (debug/anim). */
UENUM(BlueprintType)
enum class ECricketSwingPhase : uint8
{
	Idle          UMETA(DisplayName = "Idle / Guard"),
	Backlift      UMETA(DisplayName = "Backlift"),       // bat raised, waiting
	Downswing     UMETA(DisplayName = "Downswing"),      // the timing-critical descent to contact
	Contact       UMETA(DisplayName = "Contact Window"), // sweet spot passing through the contact zone
	FollowThrough UMETA(DisplayName = "Follow Through"),
	Recovery      UMETA(DisplayName = "Recovery")        // returning to guard
};

/**
 * Timing verdict, derived from WHEN the swing met the ball relative to the ideal
 * (sweet spot at the contact zone at peak bat speed). It is a READOUT of what the
 * physics already did — it does not feed back into the outcome.
 */
UENUM(BlueprintType)
enum class ECricketTimingQuality : uint8
{
	TooEarly UMETA(DisplayName = "Too Early"),   // bat well ahead of the ball
	Early    UMETA(DisplayName = "Early"),
	Perfect  UMETA(DisplayName = "Perfect"),
	Late     UMETA(DisplayName = "Late"),
	TooLate  UMETA(DisplayName = "Too Late")     // bat well behind the ball
};

/**
 * FCricketBattingInput — the player/AI INTENT for the current stroke. This is the
 * whole control surface: what shot, which foot, which way, how hard, which hand.
 * Timing is NOT in here — timing is when the swing is triggered relative to the
 * ball, and that is resolved by the motion model against the live delivery.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBattingInput
{
	GENERATED_BODY()

	/** Which of the four MVP strokes to play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting") ECricketShotType ShotType = ECricketShotType::StraightDrive;

	/** Footwork — sets reach and the natural contact zone for the stroke. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting") ECricketFootwork Footwork = ECricketFootwork::Neutral;

	/** Striker handedness. Left-handers mirror everything in Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting") bool bRightHanded = true;

	/**
	 * Fine aim, degrees, rotating the bat face (and thus the exit direction) about
	 * vertical. + opens the face toward the off side (RH). This is shot DIRECTION
	 * control; it never changes whether contact is clean.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting", meta = (ClampMin = "-45.0", ClampMax = "45.0"))
	double AimYawDeg = 0.0;

	/** Power scale [0..1.5] on the stroke's nominal bat speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Batting", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	double PowerScale = 1.0;
};

/**
 * FCricketSwingProfile — the kinematic template for one stroke, after footwork and
 * handedness have been folded in. Offsets are the sweet-spot position relative to
 * the striker's stance origin, in SI world axes. The motion model interpolates the
 * sweet spot along backlift -> contact -> follow-through over the phase durations.
 *
 * The single most important field is ContactOffsetM: it is where the sweet spot
 * will be, moving at peak speed, at exactly DownswingTimeSec into the stroke. If
 * the ball is there then, it is middled. If the player's footwork or timing put
 * the sweet spot somewhere else, the ball meets a different part of the blade and
 * the mishit is the geometric consequence.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketSwingProfile
{
	GENERATED_BODY()

	// --- Time domain (seconds) ---
	/** Backlift raise duration (cosmetic/anim; the bat is "ready" once elapsed). */
	UPROPERTY(BlueprintReadWrite, Category = "Swing") double BackliftTimeSec = 0.18;
	/** Backlift apex -> contact. THE timing-critical window. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing") double DownswingTimeSec = 0.16;
	/** Contact -> end of follow-through. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing") double FollowThroughTimeSec = 0.22;

	// --- Sweet-spot path, relative to the stance origin (m, RH world axes) ---
	UPROPERTY(BlueprintReadWrite, Category = "Swing") FVector BackliftOffsetM = FVector(0.15, 0.0, 1.30);
	UPROPERTY(BlueprintReadWrite, Category = "Swing") FVector ContactOffsetM = FVector(-0.60, 0.0, 0.70);
	UPROPERTY(BlueprintReadWrite, Category = "Swing") FVector FollowThroughOffsetM = FVector(-0.70, 0.0, 1.35);

	// --- Orientation & speed ---
	/** Intended exit direction = bat face normal at contact (RH, before AimYaw). */
	UPROPERTY(BlueprintReadWrite, Category = "Swing") FVector FaceNormalAim = FVector(-1.0, 0.0, 0.18);
	/** In-plane swing direction; defines the blade's LongAxis (toe->handle). */
	UPROPERTY(BlueprintReadWrite, Category = "Swing") FVector ArcDir = FVector(0.0, 0.0, 1.0);
	/** Nominal sweet-spot speed at contact (m/s), before PowerScale. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing") double PeakBatSpeedMS = 26.0;
	/** Bat speed as a fraction of peak at the start of the downswing. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing") double StartSpeedFraction = 0.25;
	/** Soft, controlled stroke (defensive): the contact zone is small and slow. */
	UPROPERTY(BlueprintReadWrite, Category = "Swing") bool bDefensive = false;
};

/**
 * FCricketTimingResult — the verdict on a contact's timing. Pure read-out.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketTimingResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Timing") ECricketTimingQuality Quality = ECricketTimingQuality::Perfect;

	/**
	 * Signed timing error (s). >0 = the swing was LATE (bat behind the ball, met it
	 * before the sweet spot arrived); <0 = EARLY (bat ahead, into the follow-through).
	 * This matches FCricketShotIntent::TimingErrorSec so the two layers agree.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Timing") double TimingErrorSec = 0.0;

	/** Swing-clock time at which contact actually occurred (s into the stroke). */
	UPROPERTY(BlueprintReadOnly, Category = "Timing") double ContactSwingTimeSec = 0.0;

	/** 1 at perfect timing, falling to 0 at the edge of the window. For UI/debug only. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing") double Normalized = 1.0;
};

/**
 * FCricketContactSolution — the full result of the motion model meeting the ball:
 * the bat's exact state at the moment of contact, the world contact point, and the
 * timing verdict. This is the bridge handed to FCricketBatCollision::Resolve.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketContactSolution
{
	GENERATED_BODY()

	/** True if the moving bat actually met the ball within the blade this interval. */
	UPROPERTY(BlueprintReadOnly, Category = "Contact") bool bHit = false;

	/** Bat kinematics sampled at the instant of contact (SI world axes). */
	UPROPERTY(BlueprintReadOnly, Category = "Contact") FCricketBatState BatAtContact;

	/** World contact point (m) — the ball centre at the crossing instant. */
	UPROPERTY(BlueprintReadOnly, Category = "Contact") FVector ContactPointM = FVector::ZeroVector;

	/** Closing speed along the face normal at contact (m/s), for diagnostics. */
	UPROPERTY(BlueprintReadOnly, Category = "Contact") double ClosingSpeedMS = 0.0;

	/** Phase the bat was in at contact. */
	UPROPERTY(BlueprintReadOnly, Category = "Contact") ECricketSwingPhase Phase = ECricketSwingPhase::Contact;

	/** Timing verdict for this contact. */
	UPROPERTY(BlueprintReadOnly, Category = "Contact") FCricketTimingResult Timing;
};
