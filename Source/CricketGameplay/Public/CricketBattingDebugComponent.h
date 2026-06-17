#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketBattingTypes.h"
#include "CricketBattingDebugComponent.generated.h"

class UCricketBattingComponent;

/**
 * UCricketBattingDebugComponent
 *
 * Developer visualization for the batting motion system. Attach alongside a
 * UCricketBattingComponent. Read-only; never affects the simulation. Gated by
 * cricket.Debug.Batting. It draws, live each tick and on the last contact:
 *
 *   - the bat path (sweet-spot trail through the swing), coloured by bat speed
 *   - the current bat speed (arrow + readout)
 *   - the swing plane (the face plane the bat sweeps through at the contact zone)
 *   - the foot positions (front/back/neutral) and the stance origin
 *   - the impact location (region-coloured) and contact timing (Early/Perfect/Late)
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketBattingDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBattingDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Seconds the last-contact visualization persists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	float ContactDisplaySeconds = 4.0f;

private:
	UFUNCTION()
	void HandleShotPlayed(FCricketBatImpactReport Report, FCricketTimingResult Timing);

	void DrawSwingTrail() const;
	void DrawBatAndPlane() const;
	void DrawFeet() const;
	void DrawContact() const;
	void DrawReadout() const;

	UPROPERTY()
	TObjectPtr<UCricketBattingComponent> Batting;

	float TimeSinceContact = 1e9f;
	bool bHasContact = false;
};
