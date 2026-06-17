#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketFielderDebugComponent.generated.h"

class UCricketFielderComponent;

/**
 * UCricketFielderDebugComponent
 *
 * Developer visualization for a fielder. Attach alongside a
 * UCricketFielderComponent. Read-only; gated by cricket.Debug.Fielding. Draws:
 *
 *   - the predicted ball path + landing point (the active chaser only)
 *   - the predicted catch/intercept point (colour-coded catch vs ground vs none)
 *   - the interception path (fielder -> where it is running)
 *   - the throw path (after a throw)
 *   - the fielder's decision state as a label above it
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketFielderDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketFielderDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Seconds the throw path persists after a throw. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	float ThrowDisplaySeconds = 3.0f;

private:
	void DrawPrediction() const;
	void DrawInterceptAndState() const;
	void DrawThrow() const;

	UPROPERTY()
	TObjectPtr<UCricketFielderComponent> Fielder;
};
