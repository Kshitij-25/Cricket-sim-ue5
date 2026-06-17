#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketAnimationTypes.h"
#include "CricketCharacterAnimComponent.generated.h"

class UCricketBattingComponent;
class UCricketFielderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketAnimNotify, ECricketAnimNotify, Notify);

/** A recorded notify, for the debug overlay. */
USTRUCT(BlueprintType)
struct FCricketAnimNotifyLog
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Anim") ECricketAnimNotify Type = ECricketAnimNotify::FootPlant;
	UPROPERTY(BlueprintReadOnly, Category = "Anim") float WorldTime = 0.f;
};

/**
 * UCricketCharacterAnimComponent — the AnimController: the C++ layer a cricket
 * character's Anim Blueprint reads from. It is the embodiment of the project's
 * animation philosophy:
 *
 *   - It DERIVES believable visual state from what the sim is doing: locomotion
 *     from the pawn's movement; the batting/fielding action states by FOLLOWING the
 *     existing UCricketBattingComponent / UCricketFielderComponent (physics is the
 *     source of truth, animation visualizes it).
 *   - It TIMES one physics event: the bowling run-up montage's BallRelease notify
 *     fires OnAnimNotify(BallRelease), which the bowling rig binds to BowlNow().
 *     The animation decides WHEN the ball is released; the flight is pure physics.
 *
 * Everything a real UAnimInstance needs (gait + blend, the action states, the
 * release pose it exposes, the live bat data) is available via getters here, so an
 * Anim Blueprint is a thin presentation layer over this deterministic controller.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketCharacterAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketCharacterAnimComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// --- Bowling action (the one timed sequence that gates a physics event) ---

	/** Begin the run-up; the BallRelease notify fires at the action's release time. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Anim")
	void StartBowlingAction();

	UFUNCTION(BlueprintPure, Category = "Cricket|Anim")
	bool IsBowlingActionPlaying() const { return BowlPlayer.bPlaying; }

	// --- Motion data the Anim Blueprint reads ---

	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") FCricketLocomotionSample GetLocomotion() const { return Locomotion; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") ECricketLocomotionState GetLocomotionState() const { return Locomotion.State; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") ECricketBowlingAnimState GetBowlingState() const { return BowlingState; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") ECricketBattingAnimState GetBattingState() const { return BattingState; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") ECricketFieldingAnimState GetFieldingState() const { return FieldingState; }

	/** Release pose this action exposes: world point (cm), wrist axis, action time. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") FVector GetReleaseOffsetM() const { return BowlingTimeline.ReleaseOffsetM; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") FVector GetWristAxis() const { return BowlingTimeline.WristAxis; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") double GetBowlActionTimeSec() const { return BowlPlayer.Time; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") double GetReleaseTimeSec() const { return BowlingTimeline.ReleaseTimeSec(); }

	/** Live bat data (face normal, swing speed) from the batting sim, if present. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") double GetBatSpeedMS() const;
	UFUNCTION(BlueprintPure, Category = "Cricket|Anim") FVector GetBatFaceNormal() const;

	UCricketBattingComponent* GetBatting() const { return Batting.Get(); }
	UCricketFielderComponent* GetFielder() const { return Fielder.Get(); }
	const TArray<FCricketAnimNotifyLog>& GetRecentNotifies() const { return RecentNotifies; }

	/** Fires for every animation notify (BallRelease, BatImpact, CatchAttempt, ...). */
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Anim")
	FOnCricketAnimNotify OnAnimNotify;

	// --- Authoring ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Anim") FCricketBowlingActionTimeline BowlingTimeline;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Anim") FCricketLocomotionConfig LocomotionConfig;

private:
	void UpdateLocomotion(double Dt);
	void EmitNotify(ECricketAnimNotify Type);

	UFUNCTION() void HandleShotPlayed(FCricketBatImpactReport Report, FCricketTimingResult Timing);
	UFUNCTION() void HandleFielderState(ECricketFielderState NewState);
	UFUNCTION() void HandleFielderThrew(FVector TargetWorldCm, FCricketThrowSolution Solution);

	UPROPERTY() TWeakObjectPtr<UCricketBattingComponent> Batting;
	UPROPERTY() TWeakObjectPtr<UCricketFielderComponent> Fielder;

	FCricketMontagePlayer BowlPlayer;
	FCricketLocomotionSample Locomotion;
	ECricketBowlingAnimState BowlingState = ECricketBowlingAnimState::Idle;
	ECricketBattingAnimState BattingState = ECricketBattingAnimState::Guard;
	ECricketFieldingAnimState FieldingState = ECricketFieldingAnimState::Idle;

	// Locomotion derivation state.
	FVector PrevLocationCm = FVector::ZeroVector;
	double PrevYawDeg = 0.0;
	double PrevSpeedMS = 0.0;
	bool bHasPrev = false;

	TArray<FCricketAnimNotifyLog> RecentNotifies;
};
