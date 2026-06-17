#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketInputTypes.h"
#include "CricketPlayerInputComponent.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputComponent;
class APlayerController;
struct FInputActionValue;

class UCricketBattingComponent;
class UCricketBowlingComponent;
class UCricketFielderComponent;
class UCricketCameraDirectorComponent;
class UCricketReplayComponent;

/**
 * UCricketPlayerInputComponent — the player control hub. It builds the Enhanced
 * Input setup IN C++ (Input Actions + per-layer Mapping Contexts; gamepad is just
 * extra MapKey entries later), runs the Input State Manager (swaps the active
 * mapping context per layer), and contains the per-domain controllers (Batting,
 * Bowling, Running, Fielding, Camera, Replay) as handler regions that translate
 * actions into INTENT via the pure FCricketInputModel and apply it to the existing
 * gameplay components. It never decides an outcome.
 *
 * Architecture mapping (the brief's items 1–9):
 *   1. Mapping Contexts   -> IMC_Match/Batting/Bowling/Fielding/Replay (BuildInput)
 *   2. Input Actions      -> the IA_* members (BuildInput)
 *   3. Input State Manager-> SetContext + the EnhancedInput subsystem swap
 *   4–9. Controllers      -> the Batting/Bowling/Running/Fielding/Camera/Replay
 *                            handler regions, each calling the pure model.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketPlayerInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketPlayerInputComponent();

	/** Build the actions/contexts and bind them on the pawn's input component. */
	void SetupInput(UEnhancedInputComponent* InputComp, APlayerController* PC);

	/** Swap the active input layer (Input State Manager). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Input")
	void SetContext(ECricketInputContext NewContext);

	UFUNCTION(BlueprintPure, Category = "Cricket|Input")
	ECricketInputContext GetContext() const { return ActiveContext; }

	// Target components the controllers drive (set by the owning pawn).
	void SetTargets(UCricketBattingComponent* Bat, UCricketBowlingComponent* Bowl,
		UCricketFielderComponent* Field, UCricketCameraDirectorComponent* Cam, UCricketReplayComponent* Rep);

	// --- Read-back for debug ---
	const FCricketBattingControlState& GetBattingState() const { return BattingState; }
	const FCricketBattingShotIntent& GetLastShotIntent() const { return LastShotIntent; }
	const FCricketBowlingControlState& GetBowlingState() const { return BowlingState; }
	ECricketRunCall GetLastRunCall() const { return LastRunCall; }
	ECricketFieldAction GetLastFieldAction() const { return LastFieldAction; }
	bool HasPlayedShot() const { return bHasPlayedShot; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Input")
	bool bRightHandedBatter = true;

private:
	UInputAction* MakeAction(const FName& Name, bool bAxis = false);
	void BuildInput();

	// --- Handlers (the per-domain controllers) ---
	void OnFootFront(const FInputActionValue& V); void OnFootFrontEnd(const FInputActionValue& V);
	void OnFootBack(const FInputActionValue& V);  void OnFootBackEnd(const FInputActionValue& V);
	void OnDefensive(const FInputActionValue& V); void OnDefensiveEnd(const FInputActionValue& V);
	void OnLofted(const FInputActionValue& V);    void OnLoftedEnd(const FInputActionValue& V);
	void OnDir(ECricketShotDirection Dir);
	void OnDirStraight(const FInputActionValue& V) { OnDir(ECricketShotDirection::Straight); }
	void OnDirOff(const FInputActionValue& V)      { OnDir(ECricketShotDirection::OffSide); }
	void OnDirLeg(const FInputActionValue& V)      { OnDir(ECricketShotDirection::LegSide); }
	void OnDirFine(const FInputActionValue& V)     { OnDir(ECricketShotDirection::FineLeg); }
	void OnDirCover(const FInputActionValue& V)    { OnDir(ECricketShotDirection::CoverRegion); }
	void OnDirMid(const FInputActionValue& V)      { OnDir(ECricketShotDirection::MidwicketRegion); }
	void OnPlayShot(const FInputActionValue& V);

	void OnDeliveryStock(const FInputActionValue& V); void OnDeliveryVar(const FInputActionValue& V); void OnDeliveryAggr(const FInputActionValue& V);
	void OnLineLeft(const FInputActionValue& V);  void OnLineRight(const FInputActionValue& V);
	void OnLengthUp(const FInputActionValue& V);  void OnLengthDown(const FInputActionValue& V);
	void OnSwingMod(const FInputActionValue& V); void OnSwingModEnd(const FInputActionValue& V);
	void OnSpinMod(const FInputActionValue& V);  void OnSpinModEnd(const FInputActionValue& V);
	void OnBowl(const FInputActionValue& V);

	void OnRunTake(const FInputActionValue& V); void OnRunSendBack(const FInputActionValue& V); void OnRunDive(const FInputActionValue& V);

	void OnFieldCatch(const FInputActionValue& V); void OnFieldThrow(const FInputActionValue& V); void OnFieldDive(const FInputActionValue& V); void OnFieldRelay(const FInputActionValue& V);

	void OnCamCycle(const FInputActionValue& V); void OnCamBallFollow(const FInputActionValue& V); void OnCamFree(const FInputActionValue& V);

	void OnReplayToggle(const FInputActionValue& V); void OnReplayPause(const FInputActionValue& V);
	void OnReplaySlowDown(const FInputActionValue& V); void OnReplaySlowUp(const FInputActionValue& V);
	void OnReplayStepBack(const FInputActionValue& V); void OnReplayStepFwd(const FInputActionValue& V);

	// --- Enhanced Input assets (built in C++) ---
	UPROPERTY() TObjectPtr<UInputMappingContext> IMC_Match;
	UPROPERTY() TObjectPtr<UInputMappingContext> IMC_Batting;
	UPROPERTY() TObjectPtr<UInputMappingContext> IMC_Bowling;
	UPROPERTY() TObjectPtr<UInputMappingContext> IMC_Fielding;
	UPROPERTY() TObjectPtr<UInputMappingContext> IMC_Replay;

	UPROPERTY() TObjectPtr<UInputAction> IA_FootFront;
	UPROPERTY() TObjectPtr<UInputAction> IA_FootBack;
	UPROPERTY() TObjectPtr<UInputAction> IA_Defensive;
	UPROPERTY() TObjectPtr<UInputAction> IA_Lofted;
	UPROPERTY() TObjectPtr<UInputAction> IA_DirStraight;
	UPROPERTY() TObjectPtr<UInputAction> IA_DirOff;
	UPROPERTY() TObjectPtr<UInputAction> IA_DirLeg;
	UPROPERTY() TObjectPtr<UInputAction> IA_DirFine;
	UPROPERTY() TObjectPtr<UInputAction> IA_DirCover;
	UPROPERTY() TObjectPtr<UInputAction> IA_DirMid;
	UPROPERTY() TObjectPtr<UInputAction> IA_PlayShot;

	UPROPERTY() TObjectPtr<UInputAction> IA_DeliveryStock;
	UPROPERTY() TObjectPtr<UInputAction> IA_DeliveryVar;
	UPROPERTY() TObjectPtr<UInputAction> IA_DeliveryAggr;
	UPROPERTY() TObjectPtr<UInputAction> IA_LineLeft;
	UPROPERTY() TObjectPtr<UInputAction> IA_LineRight;
	UPROPERTY() TObjectPtr<UInputAction> IA_LengthUp;
	UPROPERTY() TObjectPtr<UInputAction> IA_LengthDown;
	UPROPERTY() TObjectPtr<UInputAction> IA_SwingMod;
	UPROPERTY() TObjectPtr<UInputAction> IA_SpinMod;
	UPROPERTY() TObjectPtr<UInputAction> IA_Bowl;

	UPROPERTY() TObjectPtr<UInputAction> IA_RunTake;
	UPROPERTY() TObjectPtr<UInputAction> IA_RunSendBack;
	UPROPERTY() TObjectPtr<UInputAction> IA_RunDive;

	UPROPERTY() TObjectPtr<UInputAction> IA_FieldCatch;
	UPROPERTY() TObjectPtr<UInputAction> IA_FieldThrow;
	UPROPERTY() TObjectPtr<UInputAction> IA_FieldDive;
	UPROPERTY() TObjectPtr<UInputAction> IA_FieldRelay;

	UPROPERTY() TObjectPtr<UInputAction> IA_CamCycle;
	UPROPERTY() TObjectPtr<UInputAction> IA_CamBallFollow;
	UPROPERTY() TObjectPtr<UInputAction> IA_CamFree;

	UPROPERTY() TObjectPtr<UInputAction> IA_ReplayToggle;
	UPROPERTY() TObjectPtr<UInputAction> IA_ReplayPause;
	UPROPERTY() TObjectPtr<UInputAction> IA_ReplaySlowDown;
	UPROPERTY() TObjectPtr<UInputAction> IA_ReplaySlowUp;
	UPROPERTY() TObjectPtr<UInputAction> IA_ReplayStepBack;
	UPROPERTY() TObjectPtr<UInputAction> IA_ReplayStepFwd;

	// --- Targets ---
	UPROPERTY() TWeakObjectPtr<UCricketBattingComponent> Batting;
	UPROPERTY() TWeakObjectPtr<UCricketBowlingComponent> Bowling;
	UPROPERTY() TWeakObjectPtr<UCricketFielderComponent> Fielder;
	UPROPERTY() TWeakObjectPtr<UCricketCameraDirectorComponent> Camera;
	UPROPERTY() TWeakObjectPtr<UCricketReplayComponent> Replay;
	UPROPERTY() TWeakObjectPtr<APlayerController> Controller;

	// --- Live control state (for the controllers + debug) ---
	ECricketInputContext ActiveContext = ECricketInputContext::None;
	FCricketBattingControlState BattingState;
	FCricketBattingShotIntent LastShotIntent;
	FCricketBowlingControlState BowlingState;
	ECricketRunCall LastRunCall = ECricketRunCall::None;
	ECricketFieldAction LastFieldAction = ECricketFieldAction::None;
	bool bHasPlayedShot = false;
};
