#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketBattingTypes.h"
#include "CricketBatTypes.h"
#include "CricketBattingComponent.generated.h"

class ACricketBall;
class UCricketBallPhysicsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCricketShotPlayed,
	FCricketBatImpactReport, Report, FCricketTimingResult, Timing);

/**
 * UCricketBattingComponent — the Batting Controller.
 *
 * The gameplay-side bridge between human/AI INTENT and the physics core, and the
 * batting counterpart to UCricketBowlingComponent. It owns the current batting
 * intent (shot, footwork, aim, power, handedness) and a bat profile, and it
 * MOVES the bat through space over time:
 *
 *   1. On TriggerSwing()/Defend() it starts a stroke; the swing clock then runs.
 *   2. Each tick (after the ball has flown) it samples the bat's full kinematic
 *      state from FCricketSwingModel (backlift -> downswing -> follow-through),
 *      drives the optional bat visual (the Animation Integration Layer), and
 *   3. detects contact GEOMETRICALLY between the moving bat and the live ball over
 *      the tick interval. On contact it hands the bat state + contact point to the
 *      ball's UCricketBallPhysicsComponent::ApplyBatImpact — i.e. the EXISTING
 *      collision solver decides the outcome. This layer never scripts a result.
 *
 * Because contact is detected where the swing actually met the ball, middling vs
 * edging — and the Early/Perfect/Late verdict — emerge from the player's timing
 * and footwork, not from any injected error.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketBattingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBattingComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// --- Setup --------------------------------------------------------------

	/** The ball this bat plays at. If null at BeginPlay, the first ACricketBall is used. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting")
	void SetTargetBall(ACricketBall* InBall);

	/** Optional scene component representing the bat; driven each tick from the swing. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting")
	void SetBatVisual(USceneComponent* InBatVisual) { BatVisual = InBatVisual; }

	// --- Intent control (the whole batting control surface) -----------------

	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting|Control")
	void SelectShot(ECricketShotType Shot) { CurrentInput.ShotType = Shot; }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting|Control")
	void SetFootwork(ECricketFootwork Foot) { CurrentInput.Footwork = Foot; }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting|Control")
	void SetHanded(bool bRightHanded) { CurrentInput.bRightHanded = bRightHanded; }

	/** Fine aim in degrees (+ opens the face to off for a RH bat). Shot DIRECTION. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting|Control")
	void SetAimYawDeg(double Deg) { CurrentInput.AimYawDeg = FMath::Clamp(Deg, -45.0, 45.0); }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting|Control")
	void AdjustAimYawDeg(double Delta) { SetAimYawDeg(CurrentInput.AimYawDeg + Delta); }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting|Control")
	void SetPower(double Power) { CurrentInput.PowerScale = FMath::Clamp(Power, 0.0, 1.5); }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting|Control")
	void AdjustPower(double Delta) { SetPower(CurrentInput.PowerScale + Delta); }

	// --- Strokes ------------------------------------------------------------

	/** Begin the currently-selected stroke now. Its timing is judged against the ball. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting")
	void TriggerSwing();

	/** Play a specific shot now (selects it, then triggers). Convenience for input. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting")
	void PlayShotNow(ECricketShotType Shot, ECricketFootwork Foot);

	/** Present a defensive block now (soft, controlled; front foot by default). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Batting")
	void Defend();

	/** True while a stroke is in progress. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Batting")
	bool IsSwinging() const { return bSwingActive; }

	// --- Contact window (animation-timing readout for debug/tooling) --------

	/**
	 * The swing-clock window within which the moving bat can geometrically meet
	 * the ball and register a contact — centred on the profile's ideal contact
	 * instant (DownswingTimeSec), ± the swing model's loose timing window. This is
	 * the same window FCricketSwingModel::ClassifyTiming uses to grade Early/Late
	 * vs Too Early/Too Late; exposed here so debug tooling can show it as a real,
	 * inspectable "is the contact window open right now" state rather than an
	 * implicit consequence of the geometry.
	 */
	UFUNCTION(BlueprintPure, Category = "Cricket|Batting")
	double GetContactWindowOpenSec() const;

	UFUNCTION(BlueprintPure, Category = "Cricket|Batting")
	double GetContactWindowCloseSec() const;

	/** True while the swing clock is inside the contact window right now. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Batting")
	bool IsContactWindowOpen() const;

	// --- Read-back for debug / tooling --------------------------------------

	const FCricketBattingInput& GetInput() const { return CurrentInput; }
	const FCricketSwingProfile& GetActiveProfile() const { return ActiveProfile; }
	const FCricketBatProfile& GetBatProfile() const { return BatProfile; }
	const FCricketBatState& GetCurrentBatState() const { return CurrentBat; }
	double GetSwingClockSec() const { return SwingClock; }
	double GetCurrentBatSpeedMS() const { return CurrentBatSpeedMS; }
	ECricketSwingPhase GetSwingPhase() const { return CurrentPhase; }
	const FCricketBatImpactReport& GetLastReport() const { return LastReport; }
	const FCricketTimingResult& GetLastTiming() const { return LastTiming; }
	FVector GetLastContactCm() const { return LastContactCm; }
	bool HasPlayed() const { return bHasPlayed; }

	/** Striker's guard point in world SI metres (the stance origin for the swing). */
	FVector GetStanceOriginM() const;

	/** Recent sweet-spot world positions (cm) — the bat path, for the debug overlay. */
	const TArray<FVector>& GetSwingTrailCm() const { return SwingTrailCm; }

	UCricketBallPhysicsComponent* GetTargetBallPhysics() const;

	/** Fired when a stroke makes contact (carries the resolved impact + timing). */
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Batting")
	FOnCricketShotPlayed OnShotPlayed;

	// --- Authoring ----------------------------------------------------------

	/** The active batting intent (also editable for quick setups / AI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Batting")
	FCricketBattingInput CurrentInput;

	/** Physical bat constants used by the collision solver. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Batting")
	FCricketBatProfile BatProfile;

	/** Sub-steps per tick for contact detection (robust at coarse frame rates). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Batting", meta = (ClampMin = "1", ClampMax = "32"))
	int32 ContactSubsteps = 8;

	/** Local offset (cm) from the owner to the striker's guard point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Batting")
	FVector StanceOffsetCm = FVector::ZeroVector;

private:
	/** End the current stroke and return to guard. */
	void EndSwing();

	UPROPERTY()
	TWeakObjectPtr<ACricketBall> TargetBall;

	UPROPERTY()
	TWeakObjectPtr<USceneComponent> BatVisual;

	// Active stroke state.
	FCricketSwingProfile ActiveProfile;
	bool bSwingActive = false;
	bool bContactConsumed = false;
	double SwingClock = 0.0;        // swing-time at the START of the current tick (s)

	// Sampled-this-tick bat state (for anim + debug).
	FCricketBatState CurrentBat;
	ECricketSwingPhase CurrentPhase = ECricketSwingPhase::Idle;
	double CurrentBatSpeedMS = 0.0;

	// Last resolved contact.
	FCricketBatImpactReport LastReport;
	FCricketTimingResult LastTiming;
	FVector LastContactCm = FVector::ZeroVector;
	bool bHasPlayed = false;

	// Ball tracking for the swept contact test.
	FVector PrevBallPosM = FVector::ZeroVector;
	bool bHasPrevBall = false;

	TArray<FVector> SwingTrailCm;
};
