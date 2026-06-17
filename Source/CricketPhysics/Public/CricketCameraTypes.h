#pragma once

#include "CoreMinimal.h"
#include "CricketCameraTypes.generated.h"

/**
 * CricketCameraTypes — data models for the camera framework.
 *
 * Camera poses are computed by pure functions (FCricketCameraModel) from the
 * positions of the things being filmed (the ball, the pitch ends, the bowler, the
 * active fielder) plus per-mode tuning. The camera only ever LOOKS at the sim; it
 * never moves anything in it. World positions are UE cm.
 */

/** The selectable camera modes — gameplay + debug. */
UENUM(BlueprintType)
enum class ECricketCameraMode : uint8
{
	Batting          UMETA(DisplayName = "Batting"),
	Bowling          UMETA(DisplayName = "Bowling"),
	Fielding         UMETA(DisplayName = "Fielding"),
	Spectator        UMETA(DisplayName = "Spectator (broadcast)"),
	Free             UMETA(DisplayName = "Free"),
	Orbit            UMETA(DisplayName = "Orbit"),
	BallFollow       UMETA(DisplayName = "Ball Follow"),
	PhysicsInspection UMETA(DisplayName = "Physics Inspection")
};

/** A camera pose to apply to a UCameraComponent. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketCameraPose
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Camera") FVector LocationCm = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Camera") FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadOnly, Category = "Camera") double FOVDeg = 70.0;
};

/** Per-mode tuning. Batting distance/height are the adjustable framing the brief asks for. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketCameraConfig
{
	GENERATED_BODY()

	/** Batting/bowling chase distance (cm) behind the player. Adjustable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double DistanceCm = 650.0;
	/** Batting/bowling camera height (cm). Adjustable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double HeightCm = 200.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double FOVDeg = 70.0;

	/** Bowling camera sits lower for release/seam readability. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double BowlingHeightCm = 230.0;
	/** Fielding follow distance/height (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double FieldingDistanceCm = 700.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double FieldingHeightCm = 350.0;
	/** Spectator (side-on broadcast) offset (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double SpectatorSideCm = 4200.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double SpectatorHeightCm = 2600.0;
	/** Orbit radius (cm) and physics-inspection framing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double OrbitRadiusCm = 500.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double InspectionDistanceCm = 250.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") double InspectionFOVDeg = 45.0;
};

/** The world positions a camera frames, plus the free/orbit control inputs. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketCameraSubjects
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Camera") FVector BallCm = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") FVector BallVelocityMS = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") bool bBallInFlight = false;

	/** The two pitch ends. The pitch axis (batter->bowler) frames batting/bowling. */
	UPROPERTY(BlueprintReadWrite, Category = "Camera") FVector BatterStumpsCm = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") FVector BowlerStumpsCm = FVector(2000, 0, 0);

	UPROPERTY(BlueprintReadWrite, Category = "Camera") FVector ActiveFielderCm = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") bool bHasActiveFielder = false;

	// Manual controls (free / orbit).
	UPROPERTY(BlueprintReadWrite, Category = "Camera") FVector FreeLocationCm = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") double FreeYawDeg = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") double FreePitchDeg = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") double OrbitYawDeg = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") double OrbitPitchDeg = -20.0;
	UPROPERTY(BlueprintReadWrite, Category = "Camera") FVector OrbitPivotCm = FVector::ZeroVector;
};
