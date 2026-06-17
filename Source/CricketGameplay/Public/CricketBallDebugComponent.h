#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketPitchInteraction.h"
#include "CricketBallDebugComponent.generated.h"

class UCricketBallPhysicsComponent;

/**
 * UCricketBallDebugComponent
 *
 * Developer visualization for the ball physics. Attach alongside a
 * UCricketBallPhysicsComponent (ACricketBall already carries the physics one).
 * Draws, every frame, the things you need to SEE the simulation while tuning:
 *
 *   - velocity vector (+ speed in km/h)
 *   - spin axis (+ spin rate in RPM)
 *   - seam plane & seam normal
 *   - drag / Magnus / swing force vectors (colour-coded)
 *   - the ACTUAL flight path (a trail of where the ball has been)
 *   - the PREDICTED flight path (integrated ahead through the same model)
 *   - bounce points (actual, and the predicted first bounce)
 *   - an on-screen readout (speed, RPM, seam angle, Reynolds, swing regime, forces)
 *
 * Every overlay has a console variable (cricket.Debug.*) so you can toggle them
 * live; defaults come from UCricketPhysicsSettings. This component does NOTHING
 * to the simulation — it only reads state. Safe to leave attached; gated off in
 * shipping by the master switch.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketBallDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBallDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** How many seconds of flight to predict for the predicted-path overlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	float PredictionSeconds = 2.0f;

	/** Surface assumed by the prediction's bounce resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	FCricketSurfacePatch PredictionPitch;

	/** Max points retained in the actual-trajectory trail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	int32 MaxTrailPoints = 600;

private:
	UFUNCTION()
	void HandleBounce(FCricketBounceReport Report);

	void DrawVelocity() const;
	void DrawSpin() const;
	void DrawSeam() const;
	void DrawForces() const;
	void DrawActualTrajectory() const;
	void DrawPredictedTrajectory() const;
	void DrawBouncePoints() const;
	void DrawReadout() const;

	UPROPERTY()
	TObjectPtr<UCricketBallPhysicsComponent> Ball;

	/** Past world positions (cm) for the actual-path trail. */
	TArray<FVector> Trail;

	/** World positions (cm) where the live ball actually bounced. */
	TArray<FVector> ActualBounces;
};
