#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CricketBall.generated.h"

class UCricketBallPhysicsComponent;
class UCricketBallDebugComponent;
class UCricketBatDebugComponent;
class UCricketPitchDebugComponent;
class UStaticMeshComponent;

/**
 * ACricketBall — the protagonist. A thin actor: a visual sphere mesh driven by
 * the physics component. It deliberately holds no simulation state of its own;
 * everything lives in the deterministic core via UCricketBallPhysicsComponent.
 */
UCLASS()
class CRICKETGAMEPLAY_API ACricketBall : public AActor
{
	GENERATED_BODY()

public:
	ACricketBall();

	UCricketBallPhysicsComponent* GetBallPhysics() const { return BallPhysics; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket")
	TObjectPtr<UCricketBallPhysicsComponent> BallPhysics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket")
	TObjectPtr<UCricketBallDebugComponent> BallDebug;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket")
	TObjectPtr<UCricketBatDebugComponent> BatDebug;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket")
	TObjectPtr<UCricketPitchDebugComponent> PitchDebug;
};
