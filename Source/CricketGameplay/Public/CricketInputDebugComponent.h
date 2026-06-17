#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketInputDebugComponent.generated.h"

class UCricketPlayerInputComponent;

/**
 * UCricketInputDebugComponent — visualizes the control layer: the active input
 * context and the live intents (shot / footwork / lofted / direction, bowling,
 * running, fielding). Read-only; gated by cricket.Debug.Input.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketInputDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketInputDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY() TObjectPtr<UCricketPlayerInputComponent> Input;
};
