#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CricketBowlingTypes.h"
#include "CricketAnimationTypes.h"
#include "CricketBowlingRig.generated.h"

class UCricketBowlingComponent;
class UCricketBowlingDebugComponent;
class UCricketCharacterAnimComponent;
class UCricketAnimDebugComponent;
class UCameraComponent;
class UBoxComponent;
class ACricketBall;

/**
 * ACricketBowlingRig — the bowling Training/Test Environment.
 *
 * A self-possessing pawn that stands at the bowler's end, spawns the target
 * ACricketBall, owns the Bowling Controller (UCricketBowlingComponent) and the
 * bowling debug overlay, and exposes a simple keyboard+mouse control scheme for
 * the five MVP axes plus delivery presets and ball ageing.
 *
 * Input is polled from the possessing PlayerController each tick — deliberately
 * asset-free so the rig works the moment it is dropped into any level on macOS,
 * with no Input Action / Mapping Context wiring required. (A shipping build would
 * migrate these to Enhanced Input assets; the control SCHEME is identical.)
 *
 * Controls
 *   Space / LMB ........ bowl the current delivery
 *   Up / Down .......... length: fuller / shorter
 *   Left / Right ....... line:   toward leg / toward off
 *   Mouse / [ ] ........ fine aim (mouse) · pace down/up (brackets)
 *   Wheel .............. pace down / up
 *   Q / E .............. swing amount  -/+
 *   Z / C .............. spin amount   -/+
 *   M .................. cycle movement archetype
 *   Tab ................ cycle bowler (quick / swing / off-spin / leg-spin)
 *   1..8 ............... select the bowler's preset deliveries
 *   - (minus) .......... scuff the ball (ages it; enables reverse)
 *   R .................. fresh ball
 */
UCLASS()
class CRICKETGAMEPLAY_API ACricketBowlingRig : public APawn
{
	GENERATED_BODY()

public:
	ACricketBowlingRig();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UCricketBowlingComponent* GetBowling() const { return Bowling; }

	/** Class of ball to spawn as the delivery target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	TSubclassOf<ACricketBall> BallClass;

	/** Pace step per keypress / wheel notch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	double PaceStep = 0.05;

	/** Swing/spin step per keypress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	double AmountStep = 0.1;

	/** Mouse sensitivity for fine aim (m of trim per pixel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	double MouseAimSensitivity = 0.004;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCameraComponent> Camera;

	/** WorldStatic collision floor along the pitch so the live ball actually bounces. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UBoxComponent> PitchCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketBowlingComponent> Bowling;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketBowlingDebugComponent> BowlingDebug;

	/** Drives the run-up animation; its BallRelease notify times the actual bowl. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketCharacterAnimComponent> Anim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketAnimDebugComponent> AnimDebug;

private:
	void PollInput(float DeltaSeconds);
	void CycleMovement(int32 Dir);
	void CycleBowler(int32 Dir);
	void DrawControlsHelp() const;

	/** Bound to the anim controller: on BallRelease, release the ball into physics. */
	UFUNCTION()
	void HandleAnimNotify(ECricketAnimNotify Notify);

	UPROPERTY()
	TObjectPtr<ACricketBall> Ball;

	/** Built-in bowler actions cycled with Tab. */
	TArray<FCricketBowlingAction> Bowlers;
	int32 BowlerIndex = 0;
};
