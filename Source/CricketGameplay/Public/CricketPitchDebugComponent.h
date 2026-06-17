#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketPitchInteraction.h"
#include "CricketPitchDebugComponent.generated.h"

class UCricketBallPhysicsComponent;

/**
 * UCricketPitchDebugComponent
 *
 * Developer visualization for the PITCH simulation specifically (the ball-flight
 * overlay lives in UCricketBallDebugComponent). Attach alongside a
 * UCricketBallPhysicsComponent. For every bounce it draws, persistently:
 *
 *   - the bounce POINT,
 *   - the bounce ANGLE (incoming red arrow into the spot, outgoing green arrow),
 *   - the FRICTION used and SURFACE properties at the spot (floating readout),
 *   - the TURN (spin) and SEAM-deviation lateral kicks (cyan / orange arrows),
 *   - PREDICTED vs ACTUAL bounce: a hollow marker where the model predicted the
 *     ball would land, and a line to where it actually landed (the error).
 *
 * Each overlay has a console variable (cricket.Debug.Pitch.*) and a project-
 * settings default. Reads state only — it never affects the simulation.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketPitchDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketPitchDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Seconds of flight to look ahead when computing the predicted bounce. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	float PredictionSeconds = 3.0f;

	/** How many recent bounces to keep drawing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	int32 MaxBouncesShown = 8;

	/** Lateral arrow length scale (cm per m/s of turn/seam deviation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	float LateralArrowScale = 30.f;

private:
	UFUNCTION()
	void HandleBounce(FCricketBounceReport Report);

	/** One recorded bounce, with everything needed to draw it after the fact. */
	struct FBounceViz
	{
		FVector ContactCm = FVector::ZeroVector;
		FVector IncomingDir = FVector::ZeroVector;  // unit, world
		FVector OutgoingDir = FVector::ZeroVector;  // unit, world
		FVector PredictedContactCm = FVector::ZeroVector;
		bool    bHasPrediction = false;
		FCricketBounceReport Report;
	};

	void DrawBounce(const FBounceViz& B) const;
	void UpdatePredictedBounce();

	UPROPERTY()
	TObjectPtr<UCricketBallPhysicsComponent> Ball;

	TArray<FBounceViz> Bounces;

	/** Velocity (m/s) at the end of the previous frame — the pre-bounce velocity. */
	FVector PrevVelocityMS = FVector::ZeroVector;

	/** Most recent predicted first-bounce point (cm) while the ball is in flight. */
	FVector PredictedContactCm = FVector::ZeroVector;
	bool    bHasPredictedContact = false;
};
