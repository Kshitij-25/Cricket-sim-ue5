#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CricketScoringTypes.h"
#include "CricketOutcomeInterpreter.h"
#include "CricketMatchRunner.generated.h"

class UCricketMatchEngine;

/**
 * ACricketMatchRunner — the Match Engine Training/Test Environment + debug HUD.
 *
 * A self-possessing pawn that owns a UCricketMatchEngine, sets up India vs
 * Australia, and drives a full T20 ball-by-ball: it generates each ball's physical
 * result, runs it through FCricketOutcomeInterpreter (the consume-physics seam),
 * and feeds the FCricketDeliveryOutcome to the engine — which applies the laws.
 * The HUD visualizes the live match: state, score, over, batters, bowler, wickets,
 * and run rate. Asset-free; drop it into any level on macOS and press Play.
 *
 * NOTE on the ball generator: producing each ball's FCricketBallResult from a full
 * bowl->bat->field physics rollout needs batting/running AI that is explicitly out
 * of scope for this milestone. The generator here is a deterministic, seedable
 * stand-in so a complete match plays end-to-end; the interpreter + engine path it
 * feeds is the real one, ready to take results straight from the physics systems.
 *
 * Controls
 *   Space ...... bowl the next ball
 *   Enter ...... toggle auto-play
 *   Up / Down .. auto-play faster / slower
 *   R .......... restart the match (re-toss)
 */
UCLASS()
class CRICKETSIM_API ACricketMatchRunner : public APawn
{
	GENERATED_BODY()

public:
	ACricketMatchRunner();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UCricketMatchEngine* GetEngine() const { return Engine; }

	/** Overs per innings for this exhibition match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Match")
	int32 OversPerInnings = 20;

	/** Seconds between balls in auto-play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Match")
	float BallInterval = 0.4f;

	/** Seed for the deterministic ball generator (same seed => same match). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Match")
	int32 Seed = 12345;

private:
	void SetupMatch();
	void StepBall();
	void EnsureBowler();
	FCricketBallResult GenerateResult();
	void PollInput();
	void DrawHUD() const;

	UPROPERTY() TObjectPtr<UCricketMatchEngine> Engine;

	FCricketSquad India;
	FCricketSquad Australia;

	bool bAutoPlay = true;
	float BallTimer = 0.f;
	int32 DeliveryCounter = 0;  // advances the deterministic generator
	int32 BowlerRotation = 0;
};
