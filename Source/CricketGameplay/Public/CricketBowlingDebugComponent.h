#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketBowlingTypes.h"
#include "CricketPitchInteraction.h"
#include "CricketBowlingDebugComponent.generated.h"

class UCricketBowlingComponent;
class UCricketBallPhysicsComponent;

/**
 * UCricketBowlingDebugComponent
 *
 * Developer visualization for the bowling system. Attach alongside a
 * UCricketBowlingComponent. Every overlay the brief calls for is drawn here and
 * gated by a cricket.Bowl.Debug.* console variable so it can be toggled live:
 *
 *   - release point + the wrist/seam orientation at release
 *   - spin axis and spin rate (RPM)
 *   - the aim line and the SWING PREDICTION (straight reference vs the curved
 *     predicted path through the same model — the gap IS the swing)
 *   - the actual flight trail
 *   - predicted and actual bounce (pitch) points
 *   - a persistent PITCH MAP: length-zone bands, the stumps, and every ball's
 *     landing spot colour-coded by length
 *   - an on-screen readout of the delivery's parameters and predicted outcome
 *
 * It only reads state; it never affects the simulation.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketBowlingDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBowlingDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Seconds of flight to predict for the swing-prediction overlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling|Debug")
	float PredictionSeconds = 1.4f;

	/** Max points retained in the actual-flight trail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling|Debug")
	int32 MaxTrailPoints = 800;

	/** Max landing markers retained on the pitch map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling|Debug")
	int32 MaxPitchMarks = 60;

private:
	UFUNCTION()
	void HandleDelivery(FCricketReleaseParameters Params, FCricketDeliveryDiagnostics Diagnostics);

	UFUNCTION()
	void HandleBounce(FCricketBounceReport Report);

	void RebindBall();

	void DrawReleaseAndSeam() const;
	void DrawSpinAxis() const;
	void DrawAimAndSwing() const;
	void DrawActualTrajectory() const;
	void DrawBouncePoints() const;
	void DrawPitchMap() const;
	void DrawReadout() const;

	/** One landing mark on the pitch map. */
	struct FPitchMark
	{
		FVector LocationCm = FVector::ZeroVector;
		double  LengthM = 0.0;
	};

	UPROPERTY()
	TObjectPtr<UCricketBowlingComponent> Bowl;

	UPROPERTY()
	TObjectPtr<UCricketBallPhysicsComponent> BallPhys;

	TArray<FVector> Trail;          // actual flight path (cm)
	TArray<FPitchMark> PitchMarks;  // accumulated landings (cm)

	// Cached at each delivery so the swing-prediction overlay is stable.
	TArray<FVector> PredictedPathCm;
	FVector PredictedPitchCm = FVector::ZeroVector;
	bool bHasPrediction = false;
	int32 BallsBowled = 0;
};
