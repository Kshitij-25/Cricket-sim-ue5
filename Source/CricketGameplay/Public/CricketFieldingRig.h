#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CricketFieldingTypes.h"
#include "CricketFieldingRig.generated.h"

class ACricketFielder;
class ACricketBall;
class UCameraComponent;
class UBoxComponent;
class UCricketCameraDirectorComponent;
class UCricketReplayComponent;
enum class ECricketFielderState : uint8;

/**
 * ACricketFieldingRig — the fielding Training/Test Environment + lightweight
 * Coordinator.
 *
 * A self-possessing pawn that sets up a ball, a ring of ACricketFielder pawns, a
 * keeper, and the stumps, then each frame:
 *   - asks every fielder (via the reusable predictor) for its earliest intercept
 *     of the LIVE ball and designates the best one as the active chaser (simple
 *     coordination — NOT strategy), and
 *   - feeds the fielders their throw targets (stumps / keeper / teammates).
 * Press a number to play a shot (ground ball, lofted, skyer, boundary-bound, or a
 * straight push for a run-out) and watch the fielders react to the real physics.
 *
 * Asset-free and input-asset-free like the other rigs — drop it into any level on
 * macOS and press Play.
 *
 * Controls
 *   1 .. 5 ........ launch a shot: ground / lofted / high catch / boundary / run-out
 *   Space ......... replay the last shot
 *   Left / Right .. aim the shot (rotate the launch direction)
 *   T ............. toggle run-out mode (fielders throw at the stumps)
 *   R ............. reset the ball and send fielders home
 */
UCLASS()
class CRICKETGAMEPLAY_API ACricketFieldingRig : public APawn
{
	GENERATED_BODY()

public:
	ACricketFieldingRig();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	TSubclassOf<ACricketBall> BallClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	TSubclassOf<ACricketFielder> FielderClass;

	/** Launch aim step per keypress (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Rig")
	double AimStepDeg = 6.0;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UBoxComponent> GroundCollision;

	/** Drives the gameplay/replay cameras; the rig feeds it the live subjects. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketCameraDirectorComponent> Director;

	/** Records each delivery and plays it back (slow-mo/pause/step + overlays). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Rig")
	TObjectPtr<UCricketReplayComponent> Replay;

private:
	void SpawnField();
	void Coordinate();
	void LaunchPreset(int32 Index);
	void DetectDirectHit();
	void DrawFieldAndHelp() const;
	void PollInput();
	void PollCameraReplayInput();
	void UpdateCamera(float DeltaSeconds);
	void CycleReplayCamera();

	UFUNCTION()
	void HandleThrew(FVector TargetWorldCm, FCricketThrowSolution Solution);

	UFUNCTION()
	void HandleFielderState(ECricketFielderState NewState);

	UPROPERTY() TObjectPtr<ACricketBall> Ball;
	UPROPERTY() TArray<TObjectPtr<ACricketFielder>> Fielders;
	UPROPERTY() TObjectPtr<ACricketFielder> Keeper;

	FVector StrikerStumpsCm = FVector::ZeroVector;     // run-out target (keeper end)
	FVector BowlerStumpsCm = FVector::ZeroVector;
	FVector KeeperCm = FVector::ZeroVector;

	bool bRunOutMode = false;
	int32 LastPreset = 1;
	double LaunchYawDeg = 0.0;

	// Direct-hit monitoring (set when a throw is aimed at the stumps).
	bool bMonitorStumps = false;
	double ClosestToStumpsM = 1e9;
	bool bDirectHit = false;
	float DirectHitMsgTimer = 0.f;

	// Camera / replay state.
	UPROPERTY() TObjectPtr<ACricketFielder> CamFielder; // current chaser, for the fielding cam
	int32 ReplayCamIndex = 0;
};
