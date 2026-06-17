#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CricketPlayerPawn.generated.h"

class UCameraComponent;
class UBoxComponent;
class UCricketBattingComponent;
class UCricketBattingDebugComponent;
class UCricketCameraDirectorComponent;
class UCricketReplayComponent;
class UCricketPlayerInputComponent;
class UCricketInputDebugComponent;
class UCricketBowlingComponent;
class ACricketBall;

/**
 * ACricketPlayerPawn — the Enhanced Input showcase: a batter at the striker's end
 * facing auto-fed deliveries, driven entirely through the Cricket-07-style control
 * scheme (Enhanced Input), with the dynamic cameras and the replay system on the
 * shared layer. Demonstrates the whole control framework end-to-end.
 *
 * Controls (Batting layer)
 *   D / W ........ front foot / back foot (hold)
 *   S ............ defensive
 *   Shift ........ lofted modifier
 *   Up/Down/Left/Right/Q/E ... shot direction
 *   Space / LMB .. play the shot (timing)
 * Shared layer
 *   C ............ cycle camera   B ... ball-follow   F ... free cam
 *   V ............ replay the last delivery (then P pause, [ ] speed, , . step)
 */
UCLASS()
class CRICKETGAMEPLAY_API ACricketPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	ACricketPlayerPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Pawn")
	TSubclassOf<ACricketBall> BallClass;

	/** Seconds between auto-fed deliveries (so a ball is always coming). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Pawn")
	float FeedInterval = 3.5f;

	/** Distance (cm) to the bowler's end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Pawn")
	double PitchLengthCm = 2000.0;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<UBoxComponent> PitchCollision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<UCricketBattingComponent> Batting;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<UCricketBattingDebugComponent> BattingDebug;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<UCricketCameraDirectorComponent> Director;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<UCricketReplayComponent> Replay;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<UCricketPlayerInputComponent> Input;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Pawn") TObjectPtr<UCricketInputDebugComponent> InputDebug;

private:
	void FeedDelivery();

	UPROPERTY() TObjectPtr<ACricketBall> Ball;
	UPROPERTY() TObjectPtr<AActor> FeederAnchor;
	UPROPERTY() TObjectPtr<UCricketBowlingComponent> Feeder;

	FVector StrikerStumpsCm = FVector::ZeroVector;
	FVector BowlerStumpsCm = FVector::ZeroVector;
	float FeedTimer = 0.f;
};
