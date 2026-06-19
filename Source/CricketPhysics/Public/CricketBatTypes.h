#pragma once

#include "CoreMinimal.h"
#include "CricketBatTypes.generated.h"

/**
 * Region of the bat face that the ball struck, along the blade. Determined
 * geometrically from the actual contact point — never randomly.
 */
UENUM(BlueprintType)
enum class ECricketContactRegion : uint8
{
	Middle      UMETA(DisplayName = "Middle / Sweet Spot"),
	TopEdge     UMETA(DisplayName = "Top Edge"),     // struck high toward the shoulder
	BottomEdge  UMETA(DisplayName = "Bottom Edge"),  // struck low on the blade
	Toe         UMETA(DisplayName = "Toe"),          // very end of the blade
	ThickEdge   UMETA(DisplayName = "Thick Edge"),   // off to the side, moderate
	ThinEdge    UMETA(DisplayName = "Thin Edge")     // off to the side, near the corner
};

/** Which side edge of the bat (across the face) the ball caught, if any. */
UENUM(BlueprintType)
enum class ECricketContactSide : uint8
{
	Centre       UMETA(DisplayName = "Centre"),
	InsideEdge   UMETA(DisplayName = "Inside Edge"),
	OutsideEdge  UMETA(DisplayName = "Outside Edge")
};

/** The four MVP shots. Each maps to deterministic bat kinematics. */
UENUM(BlueprintType)
enum class ECricketShotType : uint8
{
	DefensiveBlock UMETA(DisplayName = "Defensive Block"),
	StraightDrive  UMETA(DisplayName = "Straight Drive"),
	CoverDrive     UMETA(DisplayName = "Cover Drive"),
	PullShot       UMETA(DisplayName = "Pull Shot")
};

/**
 * FCricketBatProfile — physical constants of a bat. Mass distribution is modelled
 * as an effective (node) mass that peaks at the sweet spot / centre of percussion
 * and falls toward the edges and toe, where strikes lose energy to vibration and
 * the bat "gives". Restitution falls the same way. This is what makes a middled
 * drive fizz and a toe-end jar and die — purely from where contact happens.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBatProfile
{
	GENERATED_BODY()

	/** Total bat mass (kg). Typical 1.1–1.35 kg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double TotalMassKg = 1.2;

	/** Effective (node) mass at the sweet spot (kg) — the mass the ball "feels". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double EffectiveMassSweetSpotKg = 0.95;

	/** Effective mass far from the sweet spot, at the edges/toe (kg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double EffectiveMassEdgeKg = 0.30;

	/** Blade length (m), toe to shoulder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double BladeLengthM = 0.56;

	/** Blade width (m), across the face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double BladeWidthM = 0.108;

	/** Sweet-spot height up the blade from the toe (m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double SweetSpotFromToeM = 0.18;

	/** Radius (m) of the full-quality middle zone around the sweet spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double SweetSpotRadiusM = 0.05;

	/** Length scale (m) over which quality/mass/restitution falls off from the sweet spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double QualityFalloffM = 0.13;

	/** Coefficient of restitution at the sweet spot (willow on leather ~0.45–0.55). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double RestitutionSweetSpot = 0.50;

	/** Coefficient of restitution at the edges/toe (energy lost to vibration). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double RestitutionEdge = 0.18;

	/** Surface friction between bat face and ball (drives spin transfer/deflection). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") double Friction = 0.35;

	/**
	 * Blade-curvature factor (0..1). The bat face is not a flat plane: toward the
	 * edges and toe the surface curves away, so the *local* contact normal tilts
	 * off-face proportional to how far the contact is from the spine/sweet line.
	 * This is what makes an edge squirt sideways and bleed pace (an oblique hit),
	 * rather than just softening a still-straight drive. 0 = perfectly flat face.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	double EdgeCurvature = 1.1;
};

/**
 * FCricketBatState — the bat's kinematic state at the instant of contact, in the
 * same SI world axes as the ball. The bat is a rigid body referenced to its
 * sweet spot: the velocity of any face point P is
 *     v(P) = LinearVelocity + AngularVelocity x (P - SweetSpotLocation).
 *
 * Orientation is an orthonormal frame: FaceNormal (outward, toward the ball),
 * LongAxis (toe -> handle), and WidthAxis (across the face). The shot generator
 * fills this in; the collision solver reads it.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBatState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") FVector SweetSpotLocation = FVector::ZeroVector; // m
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") FVector FaceNormal = FVector(-1, 0, 0);          // outward
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") FVector LongAxis = FVector(0, 0, 1);             // toe->handle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") FVector WidthAxis = FVector(0, 1, 0);            // across face
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") FVector LinearVelocity = FVector::ZeroVector;    // m/s of sweet spot
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat") FVector AngularVelocity = FVector::ZeroVector;   // rad/s

	/** Re-orthonormalise the frame (FaceNormal authoritative, then LongAxis, then WidthAxis). */
	void Orthonormalize();

	/** Velocity of a point P (m) on the bat. */
	FVector VelocityAt(const FVector& P) const
	{
		return LinearVelocity + FVector::CrossProduct(AngularVelocity, P - SweetSpotLocation);
	}
};

/**
 * FCricketBatImpactReport — the full, deterministic outcome of one bat-ball
 * contact. Every field is explained by the inputs; "edge probability" here is a
 * geometric certainty (EdgeFactor), not a dice roll.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBatImpactReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Impact") bool bMadeContact = false;

	// --- Contact analysis ---
	UPROPERTY(BlueprintReadOnly, Category = "Impact") ECricketContactRegion Region = ECricketContactRegion::Middle;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") ECricketContactSide Side = ECricketContactSide::Centre;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double AlongBladeM = 0.0;   // signed, + toward handle
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double AcrossFaceM = 0.0;   // signed, + outside edge
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double DistanceFromSweetSpotM = 0.0;
	/** 0 = dead centre, 1 = corner/edge. Deterministic "edge probability". */
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double EdgeFactor = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") bool bIsEdge = false;
	/** 1 at the sweet spot, falling toward edges/toe — the "timing" quality. */
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double Quality = 1.0;

	// --- Physics used ---
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double EffectiveMassKg = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double RestitutionUsed = 0.0;

	// --- Outcomes ---
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double IncomingSpeedMS = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double ExitSpeedMS = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double LaunchAngleDeg = 0.0;     // above horizontal
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double DeflectionAngleDeg = 0.0; // off the face normal (horizontal)
	UPROPERTY(BlueprintReadOnly, Category = "Impact") FVector OutgoingVelocity = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") FVector OutgoingSpin = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double SpinTransferRadS = 0.0;   // |delta omega|

	// --- Energy ---
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double EnergyInJ = 0.0;          // ball + effective bat KE in
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double EnergyOutBallJ = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Impact") double EnergyTransferFraction = 0.0;
};

/**
 * FCricketShotIntent — the player/AI intent fed to the shot generator. Timing
 * and line errors are EXPLICIT inputs (not random); a mistimed shot is one with
 * a non-zero TimingErrorSec, and the resulting mishit is fully explained by the
 * displaced contact point.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketShotIntent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") ECricketShotType ShotType = ECricketShotType::StraightDrive;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") bool bRightHanded = true;

	/** Timing error (s): >0 late (bat behind the ball), <0 early (bat ahead). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") double TimingErrorSec = 0.0;

	/** Lateral line error across the face (m): >0 toward the outside edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot") double LineErrorM = 0.0;

	/** Power scale [0..1.5] on the shot's nominal bat speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shot", meta = (ClampMin = "0.0", ClampMax = "1.5")) double PowerScale = 1.0;
};
