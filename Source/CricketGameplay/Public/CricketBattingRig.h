#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CricketBattingRig.generated.h"

class UCricketBattingComponent;
class UCricketBattingDebugComponent;
class UCricketBowlingComponent;
class UCricketCharacterAnimComponent;
class UCricketAnimDebugComponent;
class UCameraComponent;
class UBoxComponent;
class ACricketBall;

/**
 * ACricketBattingRig — the batting Training/Test Environment.
 *
 * A self-possessing pawn at the striker's end. It owns the Batting Controller
 * (UCricketBattingComponent) and its debug overlay, and — so you can actually
 * face a ball — a FEEDER built from the real UCricketBowlingComponent on an anchor
 * at the bowler's end. Pressing Feed bowls a genuine delivery (line/length/pace/
 * swing/spin through the existing bowling + pitch pipeline); you then read it and
 * play one of the four strokes. Whether you middle it is decided by your timing
 * and footwork meeting the ball — never by the key you pressed.
 *
 * Asset-free and input-asset-free, exactly like ACricketBowlingRig: drop it into
 * any level on macOS and it works with polled keyboard+mouse, no Enhanced Input
 * wiring required. (A shipping build would migrate the SCHEME to Input Actions.)
 *
 * Controls
 *   Space / LMB ........ play the selected stroke (this is the TIMING input)
 *   1 / 2 / 3 / 4 ...... shot: Block / Straight Drive / Cover Drive / Pull
 *   Up / Down .......... footwork: front foot / back foot
 *   N .................. neutral stance
 *   Left / Right / Mouse aim: trim the stroke toward leg / off
 *   Q / E .............. power  -/+
 *   F .................. feed a new delivery (bowls a ball at you)
 *   [ / ] .............. feed length: fuller / shorter
 *   Tab ................ cycle feeder (quick / swing / off-spin / leg-spin)
 *   R .................. fresh ball
 */
UCLASS()
class CRICKETGAMEPLAY_API ACricketBattingRig : public APawn
{
	GENERATED_BODY()

public:
	ACricketBattingRig();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UCricketBattingComponent* GetBatting() const { return Batting; }

	/** Class of ball to spawn as the delivery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	TSubclassOf<ACricketBall> BallClass;

	/** Distance (cm) from the striker to the feeder/bowler end along the pitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	double PitchLengthCm = 2000.0;

	/** Power step per keypress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	double PowerStep = 0.1;

	/** Aim trim per keypress (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	double AimStepDeg = 4.0;

	/** Mouse sensitivity for aim (deg per pixel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	double MouseAimSensitivity = 0.05;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCameraComponent> Camera;

	/** WorldStatic slab down the pitch so the fed ball actually bounces. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UBoxComponent> PitchCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketBattingComponent> Batting;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketBattingDebugComponent> BattingDebug;

	/** Follows the batting sim to derive anim state and fire the BatImpact notify exactly on contact. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketCharacterAnimComponent> Anim;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketAnimDebugComponent> AnimDebug;

private:
	void PollInput(float DeltaSeconds);
	void Feed();
	void CycleFeeder(int32 Dir);
	void DrawControlsHelp() const;

	UPROPERTY()
	TObjectPtr<ACricketBall> Ball;

	/** Anchor actor at the bowler's end that owns the feeder bowling component. */
	UPROPERTY()
	TObjectPtr<AActor> FeederAnchor;

	UPROPERTY()
	TObjectPtr<UCricketBowlingComponent> Feeder;

	int32 FeederIndex = 0;
};
