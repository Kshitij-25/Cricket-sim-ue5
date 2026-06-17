#pragma once

#include "CoreMinimal.h"
#include "CricketBattingTypes.h"
#include "CricketInputTypes.generated.h"

/**
 * CricketInputTypes — data models for the player control layer.
 *
 * PHILOSOPHY: controls generate INTENT only. The mapping from keys to intent (the
 * Cricket-07-style scheme) is pure data + logic so it is headless-testable and
 * independent of Enhanced Input wiring (which is just "what key feeds which flag").
 * The resolved intent is then applied to the existing gameplay components, which —
 * with the physics — decide the outcome. Nothing here determines a result.
 */

/** The input layers (Enhanced Input mapping contexts), one active set at a time. */
UENUM(BlueprintType)
enum class ECricketInputContext : uint8
{
	None     UMETA(DisplayName = "None"),
	Match    UMETA(DisplayName = "Match (shared)"),
	Batting  UMETA(DisplayName = "Batting"),
	Bowling  UMETA(DisplayName = "Bowling"),
	Fielding UMETA(DisplayName = "Fielding"),
	Replay   UMETA(DisplayName = "Replay")
};

/** Shot direction the player is steering toward (Cricket 07 directional input). */
UENUM(BlueprintType)
enum class ECricketShotDirection : uint8
{
	Straight        UMETA(DisplayName = "Straight"),
	OffSide         UMETA(DisplayName = "Off Side"),
	LegSide         UMETA(DisplayName = "Leg Side"),
	FineLeg         UMETA(DisplayName = "Fine Leg"),
	CoverRegion     UMETA(DisplayName = "Cover Region"),
	MidwicketRegion UMETA(DisplayName = "Midwicket Region")
};

/** The seven MVP strokes the control scheme can express. */
UENUM(BlueprintType)
enum class ECricketC07Shot : uint8
{
	Defensive    UMETA(DisplayName = "Defensive"),
	StraightDrive UMETA(DisplayName = "Straight Drive"),
	CoverDrive   UMETA(DisplayName = "Cover Drive"),
	PullShot     UMETA(DisplayName = "Pull Shot"),
	CutShot      UMETA(DisplayName = "Cut Shot"),
	FlickShot    UMETA(DisplayName = "Flick Shot"),
	LoftedDrive  UMETA(DisplayName = "Lofted Drive")
};

/** Bowling delivery choice (D / S / W). */
UENUM(BlueprintType)
enum class ECricketDeliveryChoice : uint8
{
	Stock     UMETA(DisplayName = "Stock"),
	Variation UMETA(DisplayName = "Variation"),
	Aggressive UMETA(DisplayName = "Aggressive (bouncer/yorker)")
};

/** Running call between the wickets (D / A / W). Future: two-player / AI partner. */
UENUM(BlueprintType)
enum class ECricketRunCall : uint8
{
	None     UMETA(DisplayName = "None"),
	Take     UMETA(DisplayName = "Take the Run"),
	SendBack UMETA(DisplayName = "Send Back"),
	Dive     UMETA(DisplayName = "Dive")
};

/** Fielding action the player requests of the controlled fielder. */
UENUM(BlueprintType)
enum class ECricketFieldAction : uint8
{
	None       UMETA(DisplayName = "None"),
	Move       UMETA(DisplayName = "Move"),
	Catch      UMETA(DisplayName = "Catch"),
	Throw      UMETA(DisplayName = "Throw"),
	Dive       UMETA(DisplayName = "Dive"),
	RelayThrow UMETA(DisplayName = "Relay Throw")
};

/**
 * FCricketBattingControlState — the held control state at the instant the shot is
 * played: footwork keys (D front / W back), the defensive key (S), the lofted
 * modifier (Shift), and the steered direction. ResolveBattingShot turns this into a
 * shot intent.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBattingControlState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input") bool bFrontFoot = false; // D held
	UPROPERTY(BlueprintReadWrite, Category = "Input") bool bBackFoot = false;  // W held
	UPROPERTY(BlueprintReadWrite, Category = "Input") bool bDefensive = false; // S held
	UPROPERTY(BlueprintReadWrite, Category = "Input") bool bLofted = false;    // Shift held
	UPROPERTY(BlueprintReadWrite, Category = "Input") ECricketShotDirection Direction = ECricketShotDirection::Straight;
};

/**
 * FCricketBattingShotIntent — the resolved intent: which stroke, on which foot,
 * lofted or not, steered where, plus the aim/power the physics layer consumes. This
 * is the bridge to the existing FCricketBattingInput.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBattingShotIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Input") ECricketC07Shot Shot = ECricketC07Shot::Defensive;
	UPROPERTY(BlueprintReadOnly, Category = "Input") ECricketFootwork Footwork = ECricketFootwork::Neutral;
	UPROPERTY(BlueprintReadOnly, Category = "Input") bool bLofted = false;
	UPROPERTY(BlueprintReadOnly, Category = "Input") ECricketShotDirection Direction = ECricketShotDirection::Straight;
	UPROPERTY(BlueprintReadOnly, Category = "Input") double AimYawDeg = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Input") double PowerScale = 1.0;
};

/** Held bowling control state; ResolveDelivery turns it into intent deltas. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBowlingControlState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Input") ECricketDeliveryChoice Delivery = ECricketDeliveryChoice::Stock;
	UPROPERTY(BlueprintReadWrite, Category = "Input") int32 LineStep = 0;    // Left/Right arrows: -1 leg .. +1 off
	UPROPERTY(BlueprintReadWrite, Category = "Input") int32 LengthStep = 0;  // Up/Down arrows: +1 shorter .. -1 fuller
	UPROPERTY(BlueprintReadWrite, Category = "Input") bool bSwingMod = false; // swing modifier held
	UPROPERTY(BlueprintReadWrite, Category = "Input") bool bSpinMod = false;  // spin modifier held
};

/** Resolved bowling intent deltas applied to the bowling component. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBowlingControlIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Input") int32 LineStepDir = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Input") int32 LengthStepDir = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Input") double SwingAmount = 0.0; // [0,1]
	UPROPERTY(BlueprintReadOnly, Category = "Input") double SpinAmount = 0.0;  // [0,1]
	UPROPERTY(BlueprintReadOnly, Category = "Input") double Pace01 = 0.8;      // pace dial
};
