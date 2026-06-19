#pragma once

#include "CoreMinimal.h"
#include "CricketMatchTypes.h"     // ECricketDismissal
#include "CricketPresentationTypes.generated.h"

/**
 * CricketPresentationTypes — the vocabulary of the broadcast PRESENTATION LAYER.
 *
 * Presentation is a CONSEQUENCE of the simulation, never a cause. The flow is one-way
 * and mirrors the audio layer's design exactly:
 *
 *   match/physics result ──► FCricketEventClassifier ──► FCricketPresentationEvent
 *        ──► broadcast/replay/crowd/score directors ──► camera + replay + atmosphere
 *
 *   - A MATCH SNAPSHOT (FCricketMatchSnapshot) is a read-only copy of the engine's
 *     scoreboard at an instant. The classifier diffs before/after to find moments.
 *   - A PRESENTATION EVENT (FCricketPresentationEvent) names a moment worth showing
 *     (a six, a wicket, a fifty, the match-winning hit) with a severity that drives
 *     how big the camera/replay/crowd response should be. It is DERIVED from the real
 *     scoreboard — it can never change one.
 *   - The directors turn events into a camera selection, a replay plan, a crowd
 *     atmosphere target and on-screen score beats.
 *
 * Every struct here is plain data so the entire decision layer is unit-testable with
 * no UWorld, no cameras, no RHI — the same contract the UI and audio layers honour.
 */

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------

/** The kinds of moment the presentation layer reacts to. */
UENUM(BlueprintType)
enum class ECricketPresentationEventType : uint8
{
	None              UMETA(DisplayName = "None"),
	Boundary          UMETA(DisplayName = "Boundary (Four)"),
	Six               UMETA(DisplayName = "Six"),
	Wicket            UMETA(DisplayName = "Wicket"),
	Milestone         UMETA(DisplayName = "Milestone"),
	MatchResult       UMETA(DisplayName = "Match Result"),

	// Broadcast-flow beats (driven by the match state machine, not a single ball).
	MatchIntro        UMETA(DisplayName = "Match Intro"),
	TeamIntro         UMETA(DisplayName = "Team Intro"),
	Toss              UMETA(DisplayName = "Toss"),
	OverTransition    UMETA(DisplayName = "Over Transition"),
	InningsTransition UMETA(DisplayName = "Innings Transition")
};

/** How big the moment is — scales camera cuts, replays, crowd lift and on-screen UI. */
UENUM(BlueprintType)
enum class ECricketPresentationSeverity : uint8
{
	Low      UMETA(DisplayName = "Low"),       // a single, a dot — ambient only
	Medium   UMETA(DisplayName = "Medium"),    // a four, a fifty
	High     UMETA(DisplayName = "High"),      // a six, a wicket, a century
	Defining UMETA(DisplayName = "Defining")   // the match-winning / match-sealing moment
};

/** Milestone categories the classifier recognises from before/after scoreboards. */
UENUM(BlueprintType)
enum class ECricketMilestoneType : uint8
{
	None             UMETA(DisplayName = "None"),
	BatterFifty      UMETA(DisplayName = "Batter Fifty"),
	BatterCentury    UMETA(DisplayName = "Batter Century"),
	BowlerThreeFor   UMETA(DisplayName = "Bowler Three-For"),
	BowlerFiveFor    UMETA(DisplayName = "Bowler Five-For"),
	TeamFifty        UMETA(DisplayName = "Team Fifty"),
	TeamHundred      UMETA(DisplayName = "Team Hundred"),
	TeamHundredFifty UMETA(DisplayName = "Team 150"),
	TeamTwoHundred   UMETA(DisplayName = "Team 200")
};

/**
 * The broadcast camera angles the director can call for. These map onto the existing
 * gameplay camera modes (ECricketCameraMode) — the presentation layer only PICKS an
 * angle, the camera director still computes the pose from the live subjects.
 */
UENUM(BlueprintType)
enum class ECricketBroadcastCamera : uint8
{
	MainBroadcast UMETA(DisplayName = "Main Broadcast"), // side-on spectator
	Bowling       UMETA(DisplayName = "Bowling"),        // behind the bowler
	Batting       UMETA(DisplayName = "Batting"),        // behind the batter
	Boundary      UMETA(DisplayName = "Boundary"),       // follows the ball to the rope
	Stump         UMETA(DisplayName = "Stump"),          // low ball-follow at the stumps
	Replay        UMETA(DisplayName = "Replay")          // orbiting replay framing
};

/** Top-level state of the presentation director. */
UENUM(BlueprintType)
enum class ECricketPresentationState : uint8
{
	Idle              UMETA(DisplayName = "Idle"),
	LivePlay          UMETA(DisplayName = "Live Play"),
	EventBeat         UMETA(DisplayName = "Event Beat"),         // holding on a just-happened moment
	Replay            UMETA(DisplayName = "Replay"),             // an automatic replay is running
	BroadcastSequence UMETA(DisplayName = "Broadcast Sequence")  // intro/toss/transition/result
};

/** Player-reaction intents the presentation surfaces for the animation/AI layers. */
UENUM(BlueprintType)
enum class ECricketReactionType : uint8
{
	None               UMETA(DisplayName = "None"),
	WicketCelebration  UMETA(DisplayName = "Wicket Celebration"),
	BoundaryCelebration UMETA(DisplayName = "Boundary Celebration"),
	MilestoneCelebration UMETA(DisplayName = "Milestone Celebration"),
	Frustration        UMETA(DisplayName = "Frustration"),
	VictoryCelebration UMETA(DisplayName = "Victory Celebration")
};

/** Crowd atmosphere descriptor — the mood band the tension/excitement arc is in. */
UENUM(BlueprintType)
enum class ECricketCrowdMood : uint8
{
	Calm     UMETA(DisplayName = "Calm"),
	Building UMETA(DisplayName = "Building"),
	Loud     UMETA(DisplayName = "Loud"),
	Electric UMETA(DisplayName = "Electric"),
	Tense    UMETA(DisplayName = "Tense (close finish)")
};

/** Which broadcast package a flow sequence belongs to. */
UENUM(BlueprintType)
enum class ECricketBroadcastSegment : uint8
{
	MatchIntro        UMETA(DisplayName = "Match Intro"),
	TeamIntro         UMETA(DisplayName = "Team Intro"),
	Toss              UMETA(DisplayName = "Toss"),
	OverTransition    UMETA(DisplayName = "Over Transition"),
	InningsTransition UMETA(DisplayName = "Innings Transition"),
	MatchResult       UMETA(DisplayName = "Match Result")
};

// ---------------------------------------------------------------------------
// Core data structures
// ---------------------------------------------------------------------------

/**
 * FCricketPresentationEvent — one classified moment. The universal currency that the
 * broadcast/replay/crowd/score directors all consume. Pure data, derived from the
 * scoreboard; it is never fed back into the engine.
 */
USTRUCT(BlueprintType)
struct CRICKETPRESENTATION_API FCricketPresentationEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") ECricketPresentationEventType Type = ECricketPresentationEventType::None;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") ECricketPresentationSeverity Severity = ECricketPresentationSeverity::Low;

	/** Valid when Type == Milestone. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") ECricketMilestoneType Milestone = ECricketMilestoneType::None;
	/** Valid when Type == Wicket. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") ECricketDismissal Dismissal = ECricketDismissal::NotOut;

	/** The headline player (batter for runs/milestones, the dismissed batter for wickets). */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString PrimaryPlayer;
	/** A secondary actor (the bowler who took the wicket, the partner, …). */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString SecondaryPlayer;

	/** Pre-formatted broadcast caption, e.g. "SIX!", "WICKET — bowled", "FIFTY • Kohli". */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString Headline;

	/** Excitement impulse this moment injects into the crowd atmosphere [0,1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") float CrowdImpulse = 0.0f;

	/** True if the replay director should consider an automatic replay. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") bool bReplayCandidate = false;

	/** True for the match-deciding moment (sealing six/boundary, last wicket). */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") bool bMatchDefining = false;

	bool IsValid() const { return Type != ECricketPresentationEventType::None; }
};

/**
 * FCricketReactionIntent — a passive request for a player reaction, surfaced by the
 * presentation layer for the animation/AI systems to consume. The presentation layer
 * does not play the animation itself; it only says "this player would celebrate now".
 */
USTRUCT(BlueprintType)
struct CRICKETPRESENTATION_API FCricketReactionIntent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") ECricketReactionType Type = ECricketReactionType::None;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString Player;
	/** [0,1] — how strong the reaction should be. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") float Intensity = 0.0f;

	bool IsValid() const { return Type != ECricketReactionType::None; }
};

/**
 * FCricketReplayPlan — the replay director's decision for a moment: whether to replay,
 * how slow, from which angles and for how long. Consumed by the Presentation Manager,
 * which drives the existing UCricketReplayComponent (it never re-simulates).
 */
USTRUCT(BlueprintType)
struct CRICKETPRESENTATION_API FCricketReplayPlan
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") bool bShouldReplay = false;
	/** Playback rate for the highlight (1 = real time, <1 = slow motion). */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") float SlowMoRate = 1.0f;
	/** The camera angles to cut through during the replay, in order. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") TArray<ECricketBroadcastCamera> Angles;
	/** Wall-clock seconds to hold each angle. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") float SecondsPerAngle = 1.5f;

	int32 NumAngles() const { return Angles.Num(); }
	float TotalSeconds() const { return SecondsPerAngle * Angles.Num(); }
};

/** One step of a broadcast-package sequence (intro/toss/transition/result). */
USTRUCT(BlueprintType)
struct CRICKETPRESENTATION_API FCricketBroadcastStep
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") FString Caption;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") ECricketBroadcastCamera Camera = ECricketBroadcastCamera::MainBroadcast;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") float DurationSeconds = 3.0f;

	FCricketBroadcastStep() = default;
	FCricketBroadcastStep(const FString& InCaption, ECricketBroadcastCamera InCamera, float InDuration)
		: Caption(InCaption), Camera(InCamera), DurationSeconds(InDuration) {}
};

/**
 * FCricketBroadcastSequence — an ordered list of broadcast steps with a runtime cursor.
 * The match-flow model builds it; the Presentation Manager advances it each tick and
 * applies the current step's camera + caption.
 */
USTRUCT(BlueprintType)
struct CRICKETPRESENTATION_API FCricketBroadcastSequence
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Presentation") ECricketBroadcastSegment Segment = ECricketBroadcastSegment::MatchIntro;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") TArray<FCricketBroadcastStep> Steps;

	// Runtime cursor.
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") int32 CurrentStep = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") float StepElapsed = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Presentation") bool bActive = false;

	int32 NumSteps() const { return Steps.Num(); }
	bool IsValid() const { return Steps.Num() > 0; }

	float TotalDuration() const
	{
		float T = 0.0f;
		for (const FCricketBroadcastStep& S : Steps) { T += S.DurationSeconds; }
		return T;
	}

	void Begin() { CurrentStep = 0; StepElapsed = 0.0f; bActive = Steps.Num() > 0; }

	/** Advance the cursor by DeltaSeconds; returns true while the sequence is still running. */
	bool Advance(float DeltaSeconds)
	{
		if (!bActive || Steps.Num() == 0) { bActive = false; return false; }
		StepElapsed += DeltaSeconds;
		while (CurrentStep < Steps.Num() && StepElapsed >= Steps[CurrentStep].DurationSeconds)
		{
			StepElapsed -= Steps[CurrentStep].DurationSeconds;
			++CurrentStep;
		}
		if (CurrentStep >= Steps.Num()) { bActive = false; CurrentStep = Steps.Num() - 1; }
		return bActive;
	}

	const FCricketBroadcastStep* ActiveStep() const
	{
		return Steps.IsValidIndex(CurrentStep) ? &Steps[CurrentStep] : nullptr;
	}
};
