#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CricketFielder.generated.h"

class UCricketFielderComponent;
class UCricketFielderDebugComponent;
class UCricketCharacterAnimComponent;
class UCricketAnimDebugComponent;

/**
 * ACricketFielder — a single fielder pawn.
 *
 * Deliberately minimal and asset-free: a root, the Fielding Controller
 * (UCricketFielderComponent) that runs the state machine and moves this pawn, a
 * debug overlay, and the AnimController (UCricketCharacterAnimComponent) that
 * follows the fielder's state machine to derive Run/Catch/Pickup/Throw anim state
 * and fire the CatchAttempt/PickupContact/ThrowRelease notifies exactly when those
 * physics events happen. The body is drawn by the debug overlay, so it works
 * dropped into any level with no mesh. The coordinator (ACricketFieldingRig) wires
 * its ball, home position, and throw targets.
 */
UCLASS()
class CRICKETGAMEPLAY_API ACricketFielder : public APawn
{
	GENERATED_BODY()

public:
	ACricketFielder();

	UCricketFielderComponent* GetFielder() const { return Fielder; }
	UCricketCharacterAnimComponent* GetAnim() const { return Anim; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Fielder")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Fielder")
	TObjectPtr<UCricketFielderComponent> Fielder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Fielder")
	TObjectPtr<UCricketFielderDebugComponent> FielderDebug;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Fielder")
	TObjectPtr<UCricketCharacterAnimComponent> Anim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Fielder")
	TObjectPtr<UCricketAnimDebugComponent> AnimDebug;
};
