#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CricketStadiumTypes.h"
#include "CricketStadium.generated.h"

class ACricketBall;
class UCricketBallPhysicsComponent;
struct FCricketBounceReport;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCricketBoundary, ECricketBoundaryResult, Result, FVector, LocationCm);

/**
 * ACricketStadium — the Stadium Manager + Match Environment Controller.
 *
 * The stadium is a SIMULATION ENVIRONMENT, not a visual asset: it owns the ground
 * geometry (built from this actor's transform + dimensions), the boundary rules,
 * the canonical fielding positions, and the atmospheric conditions. It integrates
 * with the existing systems:
 *   - Ball physics: pushes the venue atmosphere (wind/humidity/pressure) onto the
 *     ball integrator — a real, wired effect on flight — and watches the live ball.
 *   - Boundary system: detects fours/sixes from the ACTUAL ball path (bounced inside
 *     then over = four; cleared on the full = six) and validates catches vs the rope.
 *   - Match engine: exposes the boundary result so a match driver can score it.
 *
 * Drop it into a level (its transform places + orients the ground); it finds the
 * ball and overlays the venue. Debug-drawn, gated by cricket.Debug.Stadium.
 */
UCLASS()
class CRICKETGAMEPLAY_API ACricketStadium : public AActor
{
	GENERATED_BODY()

public:
	ACricketStadium();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// --- Setup ---
	UFUNCTION(BlueprintCallable, Category = "Cricket|Stadium")
	void SetBall(ACricketBall* InBall);

	/** The ground geometry, built from this actor's transform + the configured sizes. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Stadium")
	FCricketGroundDimensions GetDimensions() const;

	// --- Field Position System ---
	UFUNCTION(BlueprintPure, Category = "Cricket|Stadium")
	FVector GetFieldPositionWorldCm(ECricketFieldPosition Position) const;

	UFUNCTION(BlueprintPure, Category = "Cricket|Stadium")
	FVector GetStrikerStumpsCm() const;
	UFUNCTION(BlueprintPure, Category = "Cricket|Stadium")
	FVector GetBowlerStumpsCm() const;

	// --- Boundary System ---
	UFUNCTION(BlueprintPure, Category = "Cricket|Stadium")
	bool IsInsideBoundaryCm(const FVector& WorldCm) const;

	/** Validate a catch at WorldCm: caught (inside the rope) or a six (over it). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Stadium")
	ECricketBoundaryResult ValidateBoundaryCatchCm(const FVector& WorldCm) const;

	// --- Match Environment Controller ---
	UFUNCTION(BlueprintCallable, Category = "Cricket|Stadium")
	void ApplyEnvironmentToBall();

	UFUNCTION(BlueprintCallable, Category = "Cricket|Stadium")
	void SetTimeOfDay(ECricketTimeOfDay TimeOfDay);

	UFUNCTION(BlueprintCallable, Category = "Cricket|Stadium")
	void SetWindMS(const FVector& WindMS);

	UFUNCTION(BlueprintPure, Category = "Cricket|Stadium")
	ECricketTimeOfDay GetTimeOfDay() const { return Environment.TimeOfDay; }

	/** Fired when the live ball reaches the rope (four/six). */
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Stadium")
	FOnCricketBoundary OnBoundaryEvent;

	// --- Authoring ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Stadium", meta = (ClampMin = "40.0"))
	double StraightBoundaryM = 75.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Stadium", meta = (ClampMin = "40.0"))
	double SquareBoundaryM = 68.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Stadium")
	double RopeHeightM = 0.15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Stadium")
	bool bRightHandedBatter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Stadium")
	FCricketVenueEnvironment Environment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Stadium")
	FCricketFieldPlacement Field;

private:
	UFUNCTION() void HandleBounce(FCricketBounceReport Report);

	UCricketBallPhysicsComponent* BallPhysics() const;
	void UpdateBoundaryDetection();
	void DrawStadium() const;

	UPROPERTY(VisibleAnywhere, Category = "Cricket|Stadium")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY() TWeakObjectPtr<ACricketBall> Ball;

	// Per-delivery boundary state.
	bool bPrevBallLive = false;
	bool bBouncedInside = false;
	bool bBoundaryAwarded = false;
	FVector FirstBounceCm = FVector::ZeroVector;
	bool bHasFirstBounce = false;

	// Heatmap / shot distribution (accumulated across deliveries).
	struct FShotRecord { FVector LandingCm; FVector CrossingCm; ECricketBoundaryResult Result; };
	TArray<FShotRecord> Shots;
};
