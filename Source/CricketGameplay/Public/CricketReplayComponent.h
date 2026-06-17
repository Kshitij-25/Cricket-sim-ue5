#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketReplayTypes.h"
#include "CricketReplayComponent.generated.h"

class ACricketBall;
class UCricketBallPhysicsComponent;
struct FCricketBounceReport;
struct FCricketBatImpactReport;

/** An actor to record (its transform + animation state) into the replay. */
USTRUCT()
struct FCricketRecordedActor
{
	GENERATED_BODY()
	UPROPERTY() TWeakObjectPtr<AActor> Actor;
	UPROPERTY() int32 Id = 0;
};

/**
 * UCricketReplayComponent — the Replay Manager: Recorder + Playback System + the
 * physics-visualization it enables.
 *
 *   RECORD: each delivery is captured as a capped ring of frames (ball state +
 *   player transforms + animation state) plus sparse events (release/bounce/impact/
 *   catch/throw). It records the sim's RESULTS — it never re-simulates.
 *
 *   PLAY:  drives the ball + recorded actors from the stored clip via the pure
 *   FCricketReplayPlayer cursor, with slow motion, pause and frame stepping. The
 *   ball physics is frozen during playback so nothing is re-computed.
 *
 *   VISUALIZE: from the recorded path/events it draws the ball path, bounce points,
 *   the impact location, and the measured swing/spin deviation — the realism of the
 *   simulation made visible. Gated by cricket.Debug.Replay.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketReplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketReplayComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// --- Setup ---
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay")
	void SetBall(ACricketBall* InBall);

	/** Register a player/actor whose transform + anim state should be recorded. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay")
	void RegisterActor(AActor* Actor, int32 Id);

	/** Mark a timeline event the component can't infer itself (catch/throw/wicket). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay")
	void MarkEvent(ECricketReplayEventType Type, const FVector& WorldCm);

	// --- Playback control ---
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay") void StartReplay();
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay") void StopReplay();
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay") void TogglePause();
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay") void StepFrames(int32 N);
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay") void AdjustRate(double Delta);
	UFUNCTION(BlueprintCallable, Category = "Cricket|Replay") void SetRate(double Rate);

	UFUNCTION(BlueprintPure, Category = "Cricket|Replay") bool IsReplaying() const { return bReplaying; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Replay") bool IsRecording() const { return bRecording; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Replay") double GetPlaybackRate() const { return Player.Rate; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Replay") bool IsPaused() const { return Player.bPaused; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Replay") double GetNormalizedTime() const { return Player.NormalizedTime(); }
	UFUNCTION(BlueprintPure, Category = "Cricket|Replay") int32 GetFrameCount() const { return Clip.NumFrames(); }

	/** Current replayed ball location (cm) — what the replay cameras follow. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Replay") FVector GetReplayBallCm() const { return ReplayBallCm; }

	const FCricketReplayClip& GetClip() const { return Clip; }

	/** Draw the physics-visualization overlays for the recorded clip. */
	void DrawPhysicsOverlays() const;

	// --- Config ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Replay") double SampleHz = 60.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Replay") bool bAutoRecordLiveBall = true;

private:
	UFUNCTION() void HandleBounce(FCricketBounceReport Report);
	UFUNCTION() void HandleBatImpact(FCricketBatImpactReport Report);

	void CaptureFrame();
	UCricketBallPhysicsComponent* BallPhysics() const;
	int32 BounceFrameIndex() const;

	UPROPERTY() TWeakObjectPtr<ACricketBall> Ball;
	UPROPERTY() TArray<FCricketRecordedActor> Actors;

	FCricketReplayClip Clip;
	FCricketReplayPlayer Player;

	bool bRecording = false;
	bool bReplaying = false;
	bool bPrevBallLive = false;
	double SampleAccumulator = 0.0;
	double RecordClock = 0.0;
	FVector ReplayBallCm = FVector::ZeroVector;
};
