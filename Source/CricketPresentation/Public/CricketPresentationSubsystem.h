#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CricketPresentationTypes.h"
#include "CricketMatchSnapshot.h"
#include "CricketBroadcastDirector.h"
#include "CricketReplayDirector.h"
#include "CricketCrowdPresentationModel.h"
#include "CricketScorePresentationModel.h"
#include "CricketPresentationSubsystem.generated.h"

class UCricketMatchEngine;
class UCricketReplayComponent;
class UCricketCameraDirectorComponent;
class UCricketBallPhysicsComponent;
class UCricketAudioSubsystem;
struct FCricketDeliveryOutcome;
enum class ECricketMatchState : uint8;

/** Fired for every classified moment, so UI/analytics can subscribe passively. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketPresentationEvent, FCricketPresentationEvent, Event);
/** Fired when a player reaction would play, for the animation/AI layers to consume. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketReactionRequested, FCricketReactionIntent, Intent);

/** A row in the debug overlay's recent-events log. */
USTRUCT()
struct FCricketPresentationDebugEntry
{
	GENERATED_BODY()

	UPROPERTY() ECricketPresentationEventType Type = ECricketPresentationEventType::None;
	UPROPERTY() ECricketPresentationSeverity Severity = ECricketPresentationSeverity::Low;
	UPROPERTY() FString Headline;
	UPROPERTY() bool bTriggeredReplay = false;
	UPROPERTY() double TimeSeconds = 0.0;
};

/**
 * UCricketPresentationSubsystem — the PRESENTATION MANAGER.
 *
 * A world subsystem (auto-created, no level placement) that is the single hub of the
 * broadcast presentation layer, built to the same passive contract as the audio
 * manager. It:
 *   - DISCOVERS the gameplay sources (Match Engine, Replay, Camera Director, Ball
 *     physics, Audio) and BINDS to the engine's match/ball/over delegates;
 *   - CLASSIFIES each ball into FCricketPresentationEvents (FCricketEventClassifier);
 *   - DIRECTS the broadcast off those events — selects cameras (FCricketBroadcastDirector),
 *     rolls automatic replays (FCricketReplayDirector → the existing ReplayComponent),
 *     swells the crowd atmosphere arc (FCricketCrowdPresentationModel) and produces
 *     the score graphics (FCricketScorePresentationModel);
 *   - SEQUENCES the match-flow packages (intro / team / toss / over / innings / result
 *     via FCricketMatchFlowModel);
 *   - SURFACES player-reaction intents for the animation/AI layers (a passive output);
 *   - records recent events for the debug overlay (cvar cricket.Presentation.Debug).
 *
 * It holds NO gameplay state and writes to NO simulation system: every engine access
 * is a const read through FCricketMatchSnapshot, and the only things it commands are
 * presentation-only — the camera director's mode and the replay component's playback,
 * neither of which can change a scored outcome. With no camera/replay present it still
 * runs fully and consistently (the overlay just reports them as missing).
 */
UCLASS()
class CRICKETPRESENTATION_API UCricketPresentationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Subsystem lifecycle ---------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// --- Tickable --------------------------------------------------------------
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	// --- Public read-back (debug / UI / tests) ---------------------------------
	UFUNCTION(BlueprintPure, Category = "Cricket|Presentation") ECricketPresentationState GetPresentationState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Presentation") ECricketBroadcastCamera GetActiveCamera() const { return Broadcast.Current; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Presentation") FCricketPresentationEvent GetActiveEvent() const { return ActiveEvent; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Presentation") float GetCrowdAtmosphere() const { return Crowd.Atmosphere; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Presentation") ECricketCrowdMood GetCrowdMood() const { return Crowd.Mood(); }
	UFUNCTION(BlueprintPure, Category = "Cricket|Presentation") FString GetActiveCaption() const { return ActiveCaption; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Presentation") bool IsRunningSequence() const { return State == ECricketPresentationState::BroadcastSequence; }

	// --- Configuration (designer-tunable, presentation-only) -------------------
	/** Master switch — when false the layer is inert (nothing is classified or driven). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Presentation") bool bEnabled = true;
	/** Drive the live camera director's mode from the broadcast director. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Presentation") bool bDriveCamera = true;
	/** Roll automatic replays for qualifying moments. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Presentation") bool bAutoReplays = true;
	/** Seconds to hold live on a moment before cutting to its replay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Presentation") float EventBeatSeconds = 1.5f;

	// --- Passive outputs -------------------------------------------------------
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Presentation") FOnCricketPresentationEvent OnPresentationEvent;
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Presentation") FOnCricketReactionRequested OnReactionRequested;

	// --- Direct ingest (used by tests / scripted beats; no engine required) ----
	/** Run a pre-built event through the directors exactly as the live path would. */
	void IngestEvent(const FCricketPresentationEvent& Event);

private:
	// --- Discovery + binding ---------------------------------------------------
	void ResolveAndBindSources();

	// --- Bound engine handlers -------------------------------------------------
	// NOTE: over transitions are driven from HandleBallApplied (via the score model's
	// over-complete return), because the engine broadcasts OnOverComplete BEFORE
	// OnBallApplied within ApplyDelivery — so the fresh over summary only exists once
	// the ball has been folded in. Binding a separate OnOverComplete handler would see
	// the previous over's tally.
	UFUNCTION() void HandleMatchStateChanged(ECricketMatchState NewState);
	UFUNCTION() void HandleBallApplied(FCricketDeliveryOutcome Outcome);

	// --- Per-tick stages -------------------------------------------------------
	void UpdateLiveCamera(float DeltaSeconds);
	void UpdateEventBeat(float DeltaSeconds);
	void UpdateReplay(float DeltaSeconds);
	void UpdateSequence(float DeltaSeconds);

	// --- Orchestration helpers -------------------------------------------------
	void ProcessHeadlineEvent(const FCricketPresentationEvent& Event);    // drives the camera beat / replay
	void ProcessSecondaryEvent(const FCricketPresentationEvent& Event);   // crowd / reaction / graphics only
	void BeginSequence(const FCricketBroadcastSequence& Seq);
	void BeginReplay(const FCricketReplayPlan& Plan);
	void EndReplay();
	void FinishBeatOrReplay();                                            // return to live, draining any pending sequence
	void DriveCameraMode(ECricketBroadcastCamera Camera, float BlendSeconds = 0.6f);
	void ForceCamera(ECricketBroadcastCamera Camera, float BlendSeconds = 0.6f);
	void EmitReaction(const FCricketPresentationEvent& Event);
	void RecordDebug(const FCricketPresentationEvent& Event, bool bTriggeredReplay);
	void DrawDebug() const;

	bool BallInFlight() const;

	// --- Decision cores (pure, owned) ------------------------------------------
	FCricketBroadcastDirector Broadcast;
	FCricketReplayDirector ReplayDir;
	FCricketCrowdPresentationModel Crowd;
	FCricketScorePresentationModel Score;

	// --- Live state ------------------------------------------------------------
	ECricketPresentationState State = ECricketPresentationState::Idle;
	FCricketMatchSnapshot LastSnapshot;            // the "before" for the next ball
	FCricketPresentationEvent ActiveEvent;         // the moment currently being presented
	FString ActiveCaption;                         // current on-screen caption

	// Event-beat / replay timers.
	float BeatElapsed = 0.0f;
	FCricketReplayPlan ActiveReplayPlan;
	int32 ReplayAngleIndex = 0;
	float ReplayAngleElapsed = 0.0f;

	// Active broadcast sequence (intro/toss/transition/result), plus one that is queued
	// to play as soon as a live event beat / replay finishes.
	FCricketBroadcastSequence ActiveSequence;
	FCricketBroadcastSequence PendingSequence;
	bool bHasPendingSequence = false;

	// The camera mode last pushed to the director (so we only re-drive on a real cut).
	ECricketBroadcastCamera AppliedCamera = ECricketBroadcastCamera::MainBroadcast;
	bool bHasAppliedCamera = false;

	// --- Sources (weak; rediscovered if they go away) --------------------------
	UPROPERTY() TWeakObjectPtr<UCricketMatchEngine> MatchEngine;
	UPROPERTY() TWeakObjectPtr<UCricketReplayComponent> Replay;
	UPROPERTY() TWeakObjectPtr<UCricketCameraDirectorComponent> CameraDirector;
	UPROPERTY() TWeakObjectPtr<UCricketBallPhysicsComponent> BallPhysics;
	UPROPERTY() TWeakObjectPtr<UCricketAudioSubsystem> Audio;

	bool bBoundMatch = false;

	// Debug ring.
	TArray<FCricketPresentationDebugEntry> RecentEvents;
	double NowSeconds = 0.0;
};
