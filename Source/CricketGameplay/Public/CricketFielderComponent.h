#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketFieldingTypes.h"
#include "CricketFielderComponent.generated.h"

class ACricketBall;
class UCricketBallPhysicsComponent;

/** The fielder's decision state. Drives movement and catch/pickup/throw execution. */
UENUM(BlueprintType)
enum class ECricketFielderState : uint8
{
	Idle                UMETA(DisplayName = "Idle"),
	Tracking            UMETA(DisplayName = "Tracking"),
	MovingToIntercept   UMETA(DisplayName = "Moving To Intercept"),
	Catching            UMETA(DisplayName = "Catching"),
	PickingUp           UMETA(DisplayName = "Picking Up"),
	Throwing            UMETA(DisplayName = "Throwing"),
	ReturningToPosition UMETA(DisplayName = "Returning To Position")
};

/** Where a fielder prefers to throw once it has the ball. */
UENUM(BlueprintType)
enum class ECricketThrowTarget : uint8
{
	Stumps          UMETA(DisplayName = "Stumps (run-out)"),
	Keeper          UMETA(DisplayName = "Wicketkeeper"),
	NearestFielder  UMETA(DisplayName = "Nearest Fielder")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketFielderStateChanged, ECricketFielderState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCricketFielderThrew, FVector, TargetWorldCm, FCricketThrowSolution, Solution);

/**
 * UCricketFielderComponent — the Fielding Controller + Fielder State Machine.
 *
 * Attached to a fielder pawn. Each tick it REACTS to the live ball: it asks the
 * reusable FCricketFieldingPredictor for the ball's real forecast and its own
 * earliest intercept, then runs the state machine
 *   Idle -> Tracking -> MovingToIntercept -> (Catching | PickingUp) -> Throwing
 *        -> ReturningToPosition -> Idle
 * moving the pawn kinematically and, on contact, catching/collecting the ball and
 * throwing it (stumps / keeper / nearest fielder) via a ballistic aim.
 *
 * It never moves the ball except by the legitimate physics API (Freeze while held,
 * Release on the throw). Outcomes — whether a chance is reachable, a catch is on,
 * a run-out hits — follow from the prediction geometry, not a script or a dice roll.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketFielderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketFielderComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// --- Setup --------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Cricket|Fielding")
	void SetTargetBall(ACricketBall* InBall);

	/** Home position (world cm) this fielder returns to when idle. Defaults to spawn. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Fielding")
	void SetHomePosition(const FVector& WorldCm) { HomeWorldCm = WorldCm; bHomeSet = true; }

	/** Throw destinations (world cm). Teammates are used for the NearestFielder option. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Fielding")
	void SetThrowContext(const FVector& StumpsCm, const FVector& KeeperCm) { StumpsWorldCm = StumpsCm; KeeperWorldCm = KeeperCm; }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Fielding")
	void SetTeammates(const TArray<FVector>& TeammateWorldCm) { Teammates = TeammateWorldCm; }

	/** Designate this fielder as the one chasing the ball (set by the coordinator). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Fielding")
	void SetActiveChaser(bool bActive);

	// --- Reasoning (reused by the coordinator to pick the best chaser) -------

	/** This fielder's capabilities posed as an intercept query at its current spot. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Fielding")
	FCricketInterceptQuery MakeQuery() const;

	/** Predict the ball and solve this fielder's intercept against it (or invalid if no ball). */
	FCricketInterceptResult EvaluateIntercept(FCricketBallPrediction& OutPrediction) const;

	// --- Read-back for debug / coordination ---------------------------------

	ECricketFielderState GetState() const { return State; }
	bool IsActiveChaser() const { return bIsActiveChaser; }
	const FCricketBallPrediction& GetPrediction() const { return Prediction; }
	const FCricketInterceptResult& GetIntercept() const { return Intercept; }
	bool HasBall() const { return bHoldingBall; }
	FVector GetMoveTargetCm() const { return MoveTargetCm; }
	const TArray<FVector>& GetThrowPathCm() const { return ThrowPathCm; }
	FVector GetHandWorldCm() const;

	UPROPERTY(BlueprintAssignable, Category = "Cricket|Fielding")
	FOnCricketFielderStateChanged OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Cricket|Fielding")
	FOnCricketFielderThrew OnThrew;

	// --- Capabilities (authoring) -------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double MaxSpeedMS = 7.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double ReactionTimeSec = 0.22;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double ReachRadiusM = 1.6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double CatchReachHeightM = 2.6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double GroundFieldHeightM = 0.7;
	/** 3D distance (m) to the ball at which a catch is attempted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double CatchRadiusM = 1.3;
	/** Horizontal distance (m) to a grounded ball at which it is collected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double PickupRadiusM = 0.8;
	/** Time (s) to gather a ground ball before throwing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double GatherTimeSec = 0.3;
	/** Time (s) to complete a catch (secured) before throwing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double CatchHoldSec = 0.35;
	/** Release speed (m/s) of a throw. A hard flat throw for a run-out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double ThrowSpeedMS = 28.0;
	/** Running speed (m/s) when returning to position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double ReturnSpeedMS = 5.0;
	/** Height (m) the ball is held/released at above the fielder's feet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") double HandHeightM = 1.4;
	/** Where to throw once the ball is collected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Fielding") ECricketThrowTarget ThrowPreference = ECricketThrowTarget::Keeper;

private:
	void SetState(ECricketFielderState NewState);
	bool MoveOwnerToward(const FVector& TargetCm, double SpeedMS, double DeltaSeconds); // returns reached
	void HoldBallAtHand();
	void ExecuteThrow();
	FVector ResolveThrowTargetCm() const;
	UCricketBallPhysicsComponent* GetBallPhysics() const;

	UPROPERTY() TWeakObjectPtr<ACricketBall> TargetBall;

	ECricketFielderState State = ECricketFielderState::Idle;
	double StateTime = 0.0;          // seconds spent in the current state
	bool bIsActiveChaser = false;
	bool bHoldingBall = false;

	FVector HomeWorldCm = FVector::ZeroVector;
	bool bHomeSet = false;
	FVector MoveTargetCm = FVector::ZeroVector;

	FVector StumpsWorldCm = FVector::ZeroVector;
	FVector KeeperWorldCm = FVector::ZeroVector;
	TArray<FVector> Teammates;

	// Cached reasoning for debug + transitions.
	FCricketBallPrediction Prediction;
	FCricketInterceptResult Intercept;
	TArray<FVector> ThrowPathCm;
};
