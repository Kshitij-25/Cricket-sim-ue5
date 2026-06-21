#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketAnimDebugComponent.generated.h"

class UCricketCharacterAnimComponent;

/**
 * UCricketAnimDebugComponent
 *
 * Developer visualization for the animation layer. Attach alongside a
 * UCricketCharacterAnimComponent. Read-only; gated by cricket.Debug.Anim. Draws:
 *
 *   - the current animation state (locomotion / bowling / batting / fielding)
 *   - state transitions (the live state label above the character)
 *   - notify timing (a rolling list of recent notifies with their times; a flash
 *     when one fires)
 *   - the bat path (the swing trail, when a batter)
 *   - the release timing (action time vs scheduled release, when bowling)
 *   - the movement speed
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketAnimDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketAnimDebugComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** On-screen message key base, so several characters don't overwrite each other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Debug")
	int32 MessageKeyBase = 7000;

private:
	void DrawStateLabel() const;
	void DrawReadout() const;
	void DrawBatPath() const;
	void DrawFieldingReadout() const;

	UPROPERTY()
	TObjectPtr<UCricketCharacterAnimComponent> Anim;
};
