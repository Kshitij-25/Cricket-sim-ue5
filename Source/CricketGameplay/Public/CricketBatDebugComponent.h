#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketBatTypes.h"
#include "CricketBatDebugComponent.generated.h"

class UCricketBallPhysicsComponent;

/**
 * UCricketBatDebugComponent
 *
 * Developer visualization for bat-ball contact. Attach alongside a
 * UCricketBallPhysicsComponent. It listens for OnBatImpact and, for the last
 * impact, draws:
 *
 *   - the bat face (sweet-spot zone + edge zones, colour-coded)
 *   - the bat angle (face normal) and swing path (bat velocity)
 *   - the contact point
 *   - the exit velocity vector + launch angle
 *   - the predicted post-impact ball path (via the trajectory predictor)
 *   - an on-screen readout (region, exit speed, launch/deflection, edge factor,
 *     spin transfer, energy transfer)
 *
 * Read-only; never affects the simulation. Gated by cricket.Debug.Bat.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketBatDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBatDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Seconds the impact visualization persists after a hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	float DisplaySeconds = 4.0f;

	/** Seconds of post-impact flight to predict and draw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	float PredictionSeconds = 2.5f;

	/** Bat profile assumed when drawing the bat face zones for the last impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	FCricketBatProfile DebugBatProfile;

private:
	UFUNCTION()
	void HandleBatImpact(FCricketBatImpactReport Report);

	void DrawBatFace(const FCricketBatState& Bat) const;
	void DrawImpact() const;
	void DrawReadout() const;

	UPROPERTY()
	TObjectPtr<UCricketBallPhysicsComponent> Ball;

	// Snapshot of the last impact for persistent drawing.
	FCricketBatState LastBat;
	FVector LastContactCm = FVector::ZeroVector;
	FCricketBatImpactReport LastReport;
	TArray<FVector> PredictedPathCm;
	TArray<FVector> PredictedBouncesCm;
	float TimeSinceImpact = 1e9f;
	bool bHasImpact = false;
};
