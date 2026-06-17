#pragma once

#include "CoreMinimal.h"
#include "CricketReplayTypes.generated.h"

/**
 * CricketReplayTypes — data models + playback cursor for the replay system.
 *
 * PHILOSOPHY: a replay RECORDS the simulation's results (ball state, player
 * transforms, animation state, discrete physics events) and PLAYS THEM BACK. It
 * never re-simulates or scripts anything — what you watch in a replay is exactly
 * what the physics produced. Storage is a fixed-rate, capped ring of compact
 * frames plus a sparse list of events, so a long innings stays bounded.
 *
 * All ball quantities are SI (metres); actor transforms are UE world (cm), matching
 * the rest of the project's boundary convention.
 */

/** Discrete physics/gameplay moments worth marking on the timeline. */
UENUM(BlueprintType)
enum class ECricketReplayEventType : uint8
{
	Release   UMETA(DisplayName = "Release"),
	Bounce    UMETA(DisplayName = "Bounce"),
	BatImpact UMETA(DisplayName = "Bat Impact"),
	Catch     UMETA(DisplayName = "Catch"),
	Throw     UMETA(DisplayName = "Throw"),
	Boundary  UMETA(DisplayName = "Boundary"),
	Wicket    UMETA(DisplayName = "Wicket")
};

/** Compact ball snapshot (SI metres). The protagonist, sampled every frame. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBallSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Replay") FVector PositionM = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") FVector VelocityMS = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") FVector AngularVelocityRadS = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") FVector SeamNormal = FVector(0, 1, 0);
	UPROPERTY(BlueprintReadOnly, Category = "Replay") bool bInFlight = false;
};

/** A player/actor pose + animation state at a frame (UE world cm). */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketActorSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Replay") int32 ActorId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") FVector LocationCm = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") FRotator Rotation = FRotator::ZeroRotator;
	/** Generic animation state id (cast from the system's anim-state enum). */
	UPROPERTY(BlueprintReadOnly, Category = "Replay") uint8 AnimStateId = 0;
};

/** One recorded instant. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketReplayFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Replay") double Time = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") FCricketBallSnapshot Ball;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") TArray<FCricketActorSnapshot> Actors;
};

/** A timeline marker (with the world location it happened at, in metres). */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketReplayEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Replay") double Time = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") ECricketReplayEventType Type = ECricketReplayEventType::Bounce;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") FVector LocationM = FVector::ZeroVector;
};

/**
 * FCricketReplayClip — a complete recording: frames (dense, capped ring) + events
 * (sparse). Interpolates a frame at any time so playback is smooth at any rate.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketReplayClip
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Replay") TArray<FCricketReplayFrame> Frames;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") TArray<FCricketReplayEvent> Events;
	/** Cap on stored frames (oldest evicted past this). Bounds memory for a long innings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Replay") int32 MaxFrames = 7200; // ~2 min @ 60 Hz

	void Reset() { Frames.Reset(); Events.Reset(); }

	double StartTime() const { return Frames.Num() > 0 ? Frames[0].Time : 0.0; }
	double EndTime() const { return Frames.Num() > 0 ? Frames.Last().Time : 0.0; }
	double Duration() const { return EndTime() - StartTime(); }
	int32 NumFrames() const { return Frames.Num(); }

	/** Append a frame, evicting the oldest if over MaxFrames (the ring). */
	void AddFrame(const FCricketReplayFrame& F)
	{
		Frames.Add(F);
		while (MaxFrames > 0 && Frames.Num() > MaxFrames)
		{
			Frames.RemoveAt(0, 1, EAllowShrinking::No);
		}
	}

	void AddEvent(const FCricketReplayEvent& E) { Events.Add(E); }

	/** Interpolated frame at time T (clamped to the recorded range). */
	FCricketReplayFrame SampleAtTime(double T) const;

	/** Index of the frame at or just before T (or INDEX_NONE if empty). */
	int32 FrameIndexAtTime(double T) const;

	/** All recorded ball positions (m), for drawing the path. */
	void GetBallPath(TArray<FVector>& OutPathM) const;

	/** All event locations (m) of a given type (e.g. bounce points). */
	void GetEventLocations(ECricketReplayEventType Type, TArray<FVector>& OutM) const;
};

/**
 * FCricketReplayPlayer — the playback cursor. Drives a clip forward at a variable
 * rate (slow motion), supports pause and frame-stepping. Pure state; the gameplay
 * playback component reads SampleAtTime(GetCursorTime()) each frame.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketReplayPlayer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Replay") double CursorTime = 0.0;
	UPROPERTY(BlueprintReadWrite, Category = "Replay") double Rate = 1.0;  // 1=real time, <1=slow-mo
	UPROPERTY(BlueprintReadOnly, Category = "Replay") bool bPaused = false;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") bool bPlaying = false;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") double RangeStart = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Replay") double RangeEnd = 0.0;

	void Start(const FCricketReplayClip& Clip)
	{
		RangeStart = Clip.StartTime();
		RangeEnd = Clip.EndTime();
		CursorTime = RangeStart;
		bPlaying = true;
		bPaused = false;
		if (Rate <= 0.0) { Rate = 1.0; }
	}

	void Pause() { bPaused = true; }
	void Resume() { bPaused = false; }
	void TogglePause() { bPaused = !bPaused; }
	void SetRate(double InRate) { Rate = FMath::Max(InRate, 0.01); }

	/** Advance the cursor by RealDt of wall-clock, scaled by Rate. Clamps to the range. */
	void Advance(double RealDt)
	{
		if (!bPlaying || bPaused) { return; }
		CursorTime = FMath::Clamp(CursorTime + RealDt * Rate, RangeStart, RangeEnd);
		if (CursorTime >= RangeEnd) { bPaused = true; } // hold on the last frame
	}

	/** Jump the cursor by N frames (for frame-stepping; auto-pauses). */
	void StepFrames(const FCricketReplayClip& Clip, int32 N)
	{
		bPaused = true;
		const int32 Cur = Clip.FrameIndexAtTime(CursorTime);
		const int32 Target = FMath::Clamp(Cur + N, 0, FMath::Max(Clip.NumFrames() - 1, 0));
		if (Clip.Frames.IsValidIndex(Target)) { CursorTime = Clip.Frames[Target].Time; }
	}

	void SeekNormalized(double Alpha01)
	{
		CursorTime = FMath::Lerp(RangeStart, RangeEnd, FMath::Clamp(Alpha01, 0.0, 1.0));
	}

	double NormalizedTime() const
	{
		const double D = RangeEnd - RangeStart;
		return D > KINDA_SMALL_NUMBER ? (CursorTime - RangeStart) / D : 0.0;
	}
};
