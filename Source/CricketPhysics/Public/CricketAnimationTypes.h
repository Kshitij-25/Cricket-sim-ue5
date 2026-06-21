#pragma once

#include "CoreMinimal.h"
#include "CricketAnimationTypes.generated.h"

/**
 * CricketAnimationTypes — the data models for the animation layer.
 *
 * PHILOSOPHY: Input -> Animation -> Physics -> Result. Animation is a CONSUMER and
 * a TIMING SOURCE, never an authority on outcomes. It does two jobs:
 *   1. Derives a believable visual STATE from what the gameplay sim is doing
 *      (locomotion from speed; bowling/batting/fielding states from the existing
 *      components), and
 *   2. fires NOTIFIES at the right moment in an action so a physics event happens
 *      on time — most importantly the BALL RELEASE notify, which times when the
 *      bowling system releases the ball. The flight that follows is pure physics.
 *
 * The montage/notify model below is the reusable spine: every cricket action (a
 * bowling action, a catch, a throw, a pickup, a swing) is a montage of timed
 * phases carrying notify events. It is plain SI-time data, headless-testable.
 */

// ----------------------------- State machines --------------------------------

/** Locomotion state machine (drives the movement blendspace). */
UENUM(BlueprintType)
enum class ECricketLocomotionState : uint8
{
	Idle   UMETA(DisplayName = "Idle"),
	Walk   UMETA(DisplayName = "Walk"),
	Jog    UMETA(DisplayName = "Jog"),
	Sprint UMETA(DisplayName = "Sprint"),
	Turn   UMETA(DisplayName = "Turn"),
	Stop   UMETA(DisplayName = "Stop")
};

/** Bowling action state machine. */
UENUM(BlueprintType)
enum class ECricketBowlingAnimState : uint8
{
	Idle           UMETA(DisplayName = "Idle"),
	RunUp          UMETA(DisplayName = "Run-Up"),
	Gather         UMETA(DisplayName = "Gather"),
	DeliveryStride UMETA(DisplayName = "Delivery Stride"),
	Release        UMETA(DisplayName = "Release"),
	FollowThrough  UMETA(DisplayName = "Follow Through"),
	Recover        UMETA(DisplayName = "Recover")
};

/** Batting action state machine (mirrors the swing model's phases). */
UENUM(BlueprintType)
enum class ECricketBattingAnimState : uint8
{
	Guard         UMETA(DisplayName = "Guard"),
	Backlift      UMETA(DisplayName = "Backlift"),
	Downswing     UMETA(DisplayName = "Downswing"),
	Impact        UMETA(DisplayName = "Impact"),
	FollowThrough UMETA(DisplayName = "Follow Through"),
	Recover       UMETA(DisplayName = "Recover")
};

/** Fielding action state machine. */
UENUM(BlueprintType)
enum class ECricketFieldingAnimState : uint8
{
	Idle             UMETA(DisplayName = "Idle"),
	Run              UMETA(DisplayName = "Run"),
	GroundStop       UMETA(DisplayName = "Ground Stop"),
	Pickup           UMETA(DisplayName = "Pickup"),
	Catch            UMETA(DisplayName = "Catch"),
	Throw            UMETA(DisplayName = "Throw"),
	ReturnToPosition UMETA(DisplayName = "Return To Position")
};

/** The gameplay-facing animation notifies — the hooks that drive/observe physics. */
UENUM(BlueprintType)
enum class ECricketAnimNotify : uint8
{
	BallRelease   UMETA(DisplayName = "Ball Release"),   // release the ball into physics
	BatImpact     UMETA(DisplayName = "Bat Impact"),     // the swing met the ball
	CatchAttempt  UMETA(DisplayName = "Catch Attempt"),  // hands close on a catch
	ThrowRelease  UMETA(DisplayName = "Throw Release"),  // ball leaves the hand on a throw
	PickupContact UMETA(DisplayName = "Pickup Contact"), // hand reaches a grounded ball
	FootPlant     UMETA(DisplayName = "Foot Plant")      // a footfall (sfx/locomotion sync)
};

// --------------------------- Montage / notify model --------------------------

/** One phase of an action montage (a state held for a duration). */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketAnimPhase
{
	GENERATED_BODY()

	/** The state's enum value, stored generically (cast to the system's enum). */
	UPROPERTY(BlueprintReadWrite, Category = "Anim") int32 StateId = 0;
	/** Duration of this phase (s). */
	UPROPERTY(BlueprintReadWrite, Category = "Anim") double Duration = 0.1;
};

/** A notify scheduled at a time from the montage start. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketAnimNotifyDef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Anim") ECricketAnimNotify Type = ECricketAnimNotify::FootPlant;
	UPROPERTY(BlueprintReadWrite, Category = "Anim") double Time = 0.0;
};

/**
 * FCricketActionMontage — a timed sequence of phases carrying notify events. The
 * unit of every cricket action animation. Pure data; no engine dependency.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketActionMontage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Anim") TArray<FCricketAnimPhase> Phases;
	UPROPERTY(BlueprintReadWrite, Category = "Anim") TArray<FCricketAnimNotifyDef> Notifies;

	double TotalDuration() const
	{
		double T = 0.0;
		for (const FCricketAnimPhase& P : Phases) { T += P.Duration; }
		return T;
	}

	/** State id active at time T (clamped to the last phase). */
	int32 StateAtTime(double T) const
	{
		double Acc = 0.0;
		for (const FCricketAnimPhase& P : Phases)
		{
			Acc += P.Duration;
			if (T < Acc) { return P.StateId; }
		}
		return Phases.Num() > 0 ? Phases.Last().StateId : 0;
	}

	/** [0,1] progress through this phase at time T. */
	double PhaseAlphaAtTime(double T) const
	{
		double Acc = 0.0;
		for (const FCricketAnimPhase& P : Phases)
		{
			if (T < Acc + P.Duration)
			{
				return P.Duration > KINDA_SMALL_NUMBER ? (T - Acc) / P.Duration : 1.0;
			}
			Acc += P.Duration;
		}
		return 1.0;
	}
};

/**
 * FCricketMontagePlayer — plays a montage forward, emitting each notify exactly
 * once as its time is crossed. This is the deterministic notify engine the whole
 * animation timing system rests on (and what the timing tests exercise).
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketMontagePlayer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Anim") FCricketActionMontage Montage;
	UPROPERTY(BlueprintReadOnly, Category = "Anim") double Time = 0.0;
	UPROPERTY(BlueprintReadOnly, Category = "Anim") bool bPlaying = false;
	UPROPERTY() TArray<bool> NotifyFired;

	void Start(const FCricketActionMontage& InMontage)
	{
		Montage = InMontage;
		Time = 0.0;
		bPlaying = true;
		NotifyFired.Init(false, Montage.Notifies.Num());
	}

	void Stop() { bPlaying = false; }

	/** Advance by Dt; append any notifies whose scheduled time was reached. */
	void Advance(double Dt, TArray<ECricketAnimNotify>& OutFired)
	{
		if (!bPlaying) { return; }
		Time += FMath::Max(Dt, 0.0);
		for (int32 i = 0; i < Montage.Notifies.Num(); ++i)
		{
			if (!NotifyFired[i] && Montage.Notifies[i].Time <= Time)
			{
				NotifyFired[i] = true;
				OutFired.Add(Montage.Notifies[i].Type);
			}
		}
		if (Time >= Montage.TotalDuration()) { bPlaying = false; }
	}

	int32 CurrentStateId() const { return Montage.StateAtTime(Time); }
	double NormalizedTime() const { const double D = Montage.TotalDuration(); return D > 0 ? FMath::Clamp(Time / D, 0.0, 1.0) : 0.0; }
};

// ------------------------------- Locomotion ----------------------------------

/** Speed thresholds (m/s) that gate the locomotion state machine. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketLocomotionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion") double StopSpeed = 0.15;   // below = Idle
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion") double WalkSpeed = 1.6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion") double JogSpeed = 4.2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion") double SprintSpeed = 6.5;
	/** Turn rate (deg/s) above which, while slow, the state is Turn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion") double TurnRateDeg = 140.0;
	/** Deceleration (m/s^2) magnitude above which a moving->slowing char is Stop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Locomotion") double StopDecelMS2 = 8.0;
};

/** The locomotion result a movement blendspace consumes. */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketLocomotionSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") ECricketLocomotionState State = ECricketLocomotionState::Idle;
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") double SpeedMS = 0.0;
	/** Normalized gait blend within the locomotion space (0 idle .. 1 sprint). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") double GaitBlend = 0.0;
};

// ----------------------------- Bowling action --------------------------------

/**
 * FCricketBowlingActionTimeline — the timing of a bowling action, and the release
 * pose it EXPOSES (position, timing, wrist). The animation owns this timeline; the
 * release notify at ReleaseTime tells the bowling system WHEN to release. What the
 * ball then does is pure physics.
 */
USTRUCT(BlueprintType)
struct CRICKETPHYSICS_API FCricketBowlingActionTimeline
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling") double RunUpTimeSec = 1.6;
	/** The back-foot gather/plant after the run-up — weight loads before the jump into the delivery stride. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling") double GatherTimeSec = 0.12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling") double DeliveryStrideTimeSec = 0.45;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling") double FollowThroughTimeSec = 0.8;
	/** Time into the delivery stride at which the ball leaves the hand (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling") double ReleaseInStrideSec = 0.28;

	/** Release point offset (m) relative to the body — high and slightly forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling") FVector ReleaseOffsetM = FVector(0.4, 0.0, 2.1);
	/** Wrist/seam axis at release (unit) — the exposed wrist orientation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling") FVector WristAxis = FVector(0.0, 1.0, 0.0);

	/** Absolute time from action start at which the ball is released. */
	double ReleaseTimeSec() const { return RunUpTimeSec + GatherTimeSec + ReleaseInStrideSec; }
	double TotalDurationSec() const { return RunUpTimeSec + GatherTimeSec + DeliveryStrideTimeSec + FollowThroughTimeSec; }
};
