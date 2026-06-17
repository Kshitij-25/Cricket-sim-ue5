#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketBowlingTypes.h"
#include "CricketBowlingComponent.generated.h"

class ACricketBall;
class UCricketBallPhysicsComponent;
class UCricketBowlingActionAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCricketDelivery,
	FCricketReleaseParameters, Params, FCricketDeliveryDiagnostics, Diagnostics);

/**
 * UCricketBowlingComponent — the Bowling Controller.
 *
 * The gameplay-side bridge between human/AI INTENT and the physics core. It owns
 * the current bowling intent, the active bowler action and the ball condition,
 * and on BowlNow() it:
 *   1. builds the SI delivery context from the owning actor's transform,
 *   2. runs FCricketDeliveryGenerator to produce the physical release parameters,
 *   3. writes the delivery's ball condition / coefficients / environment onto the
 *      target ball's physics component, and
 *   4. releases the ball via ReleaseEx (m -> cm only at this boundary).
 *
 * It scripts nothing about the flight: it is purely a generator of physical
 * release conditions, exactly as the brief requires. The five MVP control axes
 * (line, length, pace, swing amount, spin amount) plus movement and preset
 * selection are exposed as BlueprintCallable setters for the input layer to call.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketBowlingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBowlingComponent();

	virtual void BeginPlay() override;

	// --- Setup --------------------------------------------------------------

	/** The ball this controller bowls. If null at BeginPlay, the first ACricketBall in the world is used. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling")
	void SetTargetBall(ACricketBall* InBall);

	/** Adopt a bowler action asset (copies its action + presets). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling")
	void SetActionAsset(UCricketBowlingActionAsset* InAsset);

	/** Set the bowler action directly (overrides any asset). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling")
	void SetAction(const FCricketBowlingAction& InAction);

	// --- The five MVP control axes (+ movement) -----------------------------

	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SetLine(ECricketLine Line) { CurrentIntent.Line = Line; }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SetLength(ECricketLength Length) { CurrentIntent.Length = Length; }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SetMovement(ECricketMovement Movement) { CurrentIntent.Movement = Movement; }

	/** Pace dial in [0,1] across the action's pace range. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SetPace01(double Pace01) { CurrentIntent.Pace01 = FMath::Clamp(Pace01, 0.0, 1.0); }

	/** Swing intensity in [0,1]. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SetSwingAmount(double Amount) { CurrentIntent.SwingAmount = FMath::Clamp(Amount, 0.0, 1.0); }

	/** Spin intensity in [0,1]. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SetSpinAmount(double Amount) { CurrentIntent.SpinAmount = FMath::Clamp(Amount, 0.0, 1.0); }

	/** Fine line trim (m, + = wider to off for RH). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SetLineFineM(double Trim) { CurrentIntent.LineFineM = Trim; }

	/** Fine length trim (m, + = fuller). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SetLengthFineM(double Trim) { CurrentIntent.LengthFineM = Trim; }

	/** Nudge pace/swing/spin by a delta (clamped). Handy for analog/scroll input. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void AdjustPace(double Delta) { SetPace01(CurrentIntent.Pace01 + Delta); }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void AdjustSwing(double Delta) { SetSwingAmount(CurrentIntent.SwingAmount + Delta); }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void AdjustSpin(double Delta) { SetSpinAmount(CurrentIntent.SpinAmount + Delta); }

	/** Step the length band shorter (+1) or fuller (-1). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void StepLength(int32 Dir);

	/** Step the aim line toward leg (-1) or off (+1). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void StepLine(int32 Dir);

	/** Apply one of the active action's named presets (clamped index). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling|Control")
	void SelectPreset(int32 Index);

	/** Number of presets the active action offers. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Bowling|Control")
	int32 NumPresets() const { return Action.Presets.Num(); }

	// --- Bowl ---------------------------------------------------------------

	/** Generate and bowl the current delivery. Returns the physical release parameters. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling")
	FCricketReleaseParameters BowlNow();

	/** Age the ball by a wear increment in [0,1] (raises roughness, dulls shine/seam — enables reverse). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling")
	void AgeBall(double WearDelta);

	/** Restore a brand-new ball condition. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling")
	void ResetBall();

	// --- Read-back for the debug overlay & tooling --------------------------

	const FCricketBowlingIntent& GetIntent() const { return CurrentIntent; }
	const FCricketBowlingAction& GetAction() const { return Action; }
	const FCricketReleaseParameters& GetLastReleaseParams() const { return LastReleaseParams; }
	const FCricketDeliveryDiagnostics& GetLastDiagnostics() const { return LastDiagnostics; }
	UCricketBallPhysicsComponent* GetTargetBallPhysics() const;

	/** World release point (cm) for the current action/owner transform. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Bowling")
	FVector GetReleaseWorldCm() const;

	UFUNCTION(BlueprintPure, Category = "Cricket|Bowling")
	FVector GetStrikerStumpsWorldCm() const { return StrikerStumpsWorldCm; }

	/** World height (m) of the pitch plane the ball bounces on — the striker's ground level. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Bowling")
	double GetGroundPlaneZM() const;

	UFUNCTION(BlueprintCallable, Category = "Cricket|Bowling")
	void SetStrikerStumpsWorldCm(const FVector& InCm) { StrikerStumpsWorldCm = InCm; bStrikerExplicit = true; }

	/** Fired after a delivery is bowled. */
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Bowling")
	FOnCricketDelivery OnDelivery;

	// --- Authoring ----------------------------------------------------------

	/** The active intent (also editable in the details panel for quick setups). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling")
	FCricketBowlingIntent CurrentIntent;

	/** The active bowler action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling")
	FCricketBowlingAction Action;

	/** Optional action asset adopted at BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling")
	TObjectPtr<UCricketBowlingActionAsset> ActionAsset;

	/** Current ball condition (ages over the innings; drives reverse swing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling")
	FCricketBallSurface BallCondition;

	/** Atmosphere for the aim solve and the ball. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling")
	FCricketEnvironment Environment;

	/** Pitch surface used for the aim solve and applied to the live ball's bounces. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling")
	FCricketSurfacePatch PitchSurface;

	/** Human release inaccuracy in [0,1] (scatters inputs only; 0 = a bowling machine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double HumanScatter = 0.0;

	/** Seed for the next delivery's scatter; auto-incremented each ball for variety. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Bowling")
	int32 DeliverySeed = 1;

private:
	FVector ComputeReleaseWorldCm() const;

	UPROPERTY()
	TWeakObjectPtr<ACricketBall> TargetBall;

	FCricketReleaseParameters LastReleaseParams;
	FCricketDeliveryDiagnostics LastDiagnostics;

	/** Target reference (cm). Defaulted from the owner transform + action unless set explicitly. */
	FVector StrikerStumpsWorldCm = FVector::ZeroVector;
	bool bStrikerExplicit = false;
};
