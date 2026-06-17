#include "CricketPlayerInputComponent.h"
#include "CricketInputModel.h"
#include "CricketBattingComponent.h"
#include "CricketBowlingComponent.h"
#include "CricketFielderComponent.h"
#include "CricketCameraDirectorComponent.h"
#include "CricketReplayComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

UCricketPlayerInputComponent::UCricketPlayerInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCricketPlayerInputComponent::SetTargets(UCricketBattingComponent* Bat, UCricketBowlingComponent* Bowl,
	UCricketFielderComponent* Field, UCricketCameraDirectorComponent* Cam, UCricketReplayComponent* Rep)
{
	Batting = Bat; Bowling = Bowl; Fielder = Field; Camera = Cam; Replay = Rep;
}

UInputAction* UCricketPlayerInputComponent::MakeAction(const FName& Name, bool bAxis)
{
	UInputAction* IA = NewObject<UInputAction>(this, Name);
	IA->ValueType = bAxis ? EInputActionValueType::Axis1D : EInputActionValueType::Boolean;
	return IA;
}

void UCricketPlayerInputComponent::BuildInput()
{
	// --- Input Actions ---
	IA_FootFront = MakeAction(TEXT("IA_FootFront"));   IA_FootBack = MakeAction(TEXT("IA_FootBack"));
	IA_Defensive = MakeAction(TEXT("IA_Defensive"));   IA_Lofted = MakeAction(TEXT("IA_Lofted"));
	IA_DirStraight = MakeAction(TEXT("IA_DirStraight")); IA_DirOff = MakeAction(TEXT("IA_DirOff"));
	IA_DirLeg = MakeAction(TEXT("IA_DirLeg"));          IA_DirFine = MakeAction(TEXT("IA_DirFine"));
	IA_DirCover = MakeAction(TEXT("IA_DirCover"));      IA_DirMid = MakeAction(TEXT("IA_DirMid"));
	IA_PlayShot = MakeAction(TEXT("IA_PlayShot"));

	IA_DeliveryStock = MakeAction(TEXT("IA_DeliveryStock")); IA_DeliveryVar = MakeAction(TEXT("IA_DeliveryVar")); IA_DeliveryAggr = MakeAction(TEXT("IA_DeliveryAggr"));
	IA_LineLeft = MakeAction(TEXT("IA_LineLeft")); IA_LineRight = MakeAction(TEXT("IA_LineRight"));
	IA_LengthUp = MakeAction(TEXT("IA_LengthUp")); IA_LengthDown = MakeAction(TEXT("IA_LengthDown"));
	IA_SwingMod = MakeAction(TEXT("IA_SwingMod")); IA_SpinMod = MakeAction(TEXT("IA_SpinMod")); IA_Bowl = MakeAction(TEXT("IA_Bowl"));

	IA_RunTake = MakeAction(TEXT("IA_RunTake")); IA_RunSendBack = MakeAction(TEXT("IA_RunSendBack")); IA_RunDive = MakeAction(TEXT("IA_RunDive"));

	IA_FieldCatch = MakeAction(TEXT("IA_FieldCatch")); IA_FieldThrow = MakeAction(TEXT("IA_FieldThrow")); IA_FieldDive = MakeAction(TEXT("IA_FieldDive")); IA_FieldRelay = MakeAction(TEXT("IA_FieldRelay"));

	IA_CamCycle = MakeAction(TEXT("IA_CamCycle")); IA_CamBallFollow = MakeAction(TEXT("IA_CamBallFollow")); IA_CamFree = MakeAction(TEXT("IA_CamFree"));

	IA_ReplayToggle = MakeAction(TEXT("IA_ReplayToggle")); IA_ReplayPause = MakeAction(TEXT("IA_ReplayPause"));
	IA_ReplaySlowDown = MakeAction(TEXT("IA_ReplaySlowDown")); IA_ReplaySlowUp = MakeAction(TEXT("IA_ReplaySlowUp"));
	IA_ReplayStepBack = MakeAction(TEXT("IA_ReplayStepBack")); IA_ReplayStepFwd = MakeAction(TEXT("IA_ReplayStepFwd"));

	// --- Mapping Contexts (the input layers). Gamepad = extra MapKey lines later. ---
	IMC_Match = NewObject<UInputMappingContext>(this, TEXT("IMC_Match"));
	IMC_Batting = NewObject<UInputMappingContext>(this, TEXT("IMC_Batting"));
	IMC_Bowling = NewObject<UInputMappingContext>(this, TEXT("IMC_Bowling"));
	IMC_Fielding = NewObject<UInputMappingContext>(this, TEXT("IMC_Fielding"));
	IMC_Replay = NewObject<UInputMappingContext>(this, TEXT("IMC_Replay"));

	// Batting layer — the Cricket-07 scheme.
	IMC_Batting->MapKey(IA_FootFront, EKeys::D);
	IMC_Batting->MapKey(IA_FootBack, EKeys::W);
	IMC_Batting->MapKey(IA_Defensive, EKeys::S);
	IMC_Batting->MapKey(IA_Lofted, EKeys::LeftShift);
	IMC_Batting->MapKey(IA_DirStraight, EKeys::Up);
	IMC_Batting->MapKey(IA_DirOff, EKeys::Right);
	IMC_Batting->MapKey(IA_DirLeg, EKeys::Left);
	IMC_Batting->MapKey(IA_DirFine, EKeys::Down);
	IMC_Batting->MapKey(IA_DirCover, EKeys::E);
	IMC_Batting->MapKey(IA_DirMid, EKeys::Q);
	IMC_Batting->MapKey(IA_PlayShot, EKeys::SpaceBar);
	IMC_Batting->MapKey(IA_PlayShot, EKeys::LeftMouseButton);

	// Bowling layer.
	IMC_Bowling->MapKey(IA_DeliveryStock, EKeys::D);
	IMC_Bowling->MapKey(IA_DeliveryVar, EKeys::S);
	IMC_Bowling->MapKey(IA_DeliveryAggr, EKeys::W);
	IMC_Bowling->MapKey(IA_LineLeft, EKeys::Left);
	IMC_Bowling->MapKey(IA_LineRight, EKeys::Right);
	IMC_Bowling->MapKey(IA_LengthUp, EKeys::Up);
	IMC_Bowling->MapKey(IA_LengthDown, EKeys::Down);
	IMC_Bowling->MapKey(IA_SwingMod, EKeys::Q);
	IMC_Bowling->MapKey(IA_SpinMod, EKeys::E);
	IMC_Bowling->MapKey(IA_Bowl, EKeys::SpaceBar);

	// Fielding layer.
	IMC_Fielding->MapKey(IA_FieldCatch, EKeys::SpaceBar);
	IMC_Fielding->MapKey(IA_FieldThrow, EKeys::E);
	IMC_Fielding->MapKey(IA_FieldDive, EKeys::W);
	IMC_Fielding->MapKey(IA_FieldRelay, EKeys::R);

	// Replay playback layer.
	IMC_Replay->MapKey(IA_ReplayPause, EKeys::P);
	IMC_Replay->MapKey(IA_ReplaySlowDown, EKeys::LeftBracket);
	IMC_Replay->MapKey(IA_ReplaySlowUp, EKeys::RightBracket);
	IMC_Replay->MapKey(IA_ReplayStepBack, EKeys::Comma);
	IMC_Replay->MapKey(IA_ReplayStepFwd, EKeys::Period);

	// Match (shared base) layer — running, cameras, replay-enter; always active.
	IMC_Match->MapKey(IA_RunTake, EKeys::D);
	IMC_Match->MapKey(IA_RunSendBack, EKeys::A);
	IMC_Match->MapKey(IA_RunDive, EKeys::W);
	IMC_Match->MapKey(IA_CamCycle, EKeys::C);
	IMC_Match->MapKey(IA_CamBallFollow, EKeys::B);
	IMC_Match->MapKey(IA_CamFree, EKeys::F);
	IMC_Match->MapKey(IA_ReplayToggle, EKeys::V);
}

void UCricketPlayerInputComponent::SetupInput(UEnhancedInputComponent* EIC, APlayerController* PC)
{
	Controller = PC;
	BuildInput();
	if (!EIC) { return; }

	using ET = ETriggerEvent;
	auto B = [&](UInputAction* IA, ET Evt, auto Fn) { EIC->BindAction(IA, Evt, this, Fn); };

	// Batting controller.
	B(IA_FootFront, ET::Started, &UCricketPlayerInputComponent::OnFootFront);
	B(IA_FootFront, ET::Completed, &UCricketPlayerInputComponent::OnFootFrontEnd);
	B(IA_FootBack, ET::Started, &UCricketPlayerInputComponent::OnFootBack);
	B(IA_FootBack, ET::Completed, &UCricketPlayerInputComponent::OnFootBackEnd);
	B(IA_Defensive, ET::Started, &UCricketPlayerInputComponent::OnDefensive);
	B(IA_Defensive, ET::Completed, &UCricketPlayerInputComponent::OnDefensiveEnd);
	B(IA_Lofted, ET::Started, &UCricketPlayerInputComponent::OnLofted);
	B(IA_Lofted, ET::Completed, &UCricketPlayerInputComponent::OnLoftedEnd);
	B(IA_DirStraight, ET::Started, &UCricketPlayerInputComponent::OnDirStraight);
	B(IA_DirOff, ET::Started, &UCricketPlayerInputComponent::OnDirOff);
	B(IA_DirLeg, ET::Started, &UCricketPlayerInputComponent::OnDirLeg);
	B(IA_DirFine, ET::Started, &UCricketPlayerInputComponent::OnDirFine);
	B(IA_DirCover, ET::Started, &UCricketPlayerInputComponent::OnDirCover);
	B(IA_DirMid, ET::Started, &UCricketPlayerInputComponent::OnDirMid);
	B(IA_PlayShot, ET::Started, &UCricketPlayerInputComponent::OnPlayShot);

	// Bowling controller.
	B(IA_DeliveryStock, ET::Started, &UCricketPlayerInputComponent::OnDeliveryStock);
	B(IA_DeliveryVar, ET::Started, &UCricketPlayerInputComponent::OnDeliveryVar);
	B(IA_DeliveryAggr, ET::Started, &UCricketPlayerInputComponent::OnDeliveryAggr);
	B(IA_LineLeft, ET::Started, &UCricketPlayerInputComponent::OnLineLeft);
	B(IA_LineRight, ET::Started, &UCricketPlayerInputComponent::OnLineRight);
	B(IA_LengthUp, ET::Started, &UCricketPlayerInputComponent::OnLengthUp);
	B(IA_LengthDown, ET::Started, &UCricketPlayerInputComponent::OnLengthDown);
	B(IA_SwingMod, ET::Started, &UCricketPlayerInputComponent::OnSwingMod);
	B(IA_SwingMod, ET::Completed, &UCricketPlayerInputComponent::OnSwingModEnd);
	B(IA_SpinMod, ET::Started, &UCricketPlayerInputComponent::OnSpinMod);
	B(IA_SpinMod, ET::Completed, &UCricketPlayerInputComponent::OnSpinModEnd);
	B(IA_Bowl, ET::Started, &UCricketPlayerInputComponent::OnBowl);

	// Running controller.
	B(IA_RunTake, ET::Started, &UCricketPlayerInputComponent::OnRunTake);
	B(IA_RunSendBack, ET::Started, &UCricketPlayerInputComponent::OnRunSendBack);
	B(IA_RunDive, ET::Started, &UCricketPlayerInputComponent::OnRunDive);

	// Fielding controller.
	B(IA_FieldCatch, ET::Started, &UCricketPlayerInputComponent::OnFieldCatch);
	B(IA_FieldThrow, ET::Started, &UCricketPlayerInputComponent::OnFieldThrow);
	B(IA_FieldDive, ET::Started, &UCricketPlayerInputComponent::OnFieldDive);
	B(IA_FieldRelay, ET::Started, &UCricketPlayerInputComponent::OnFieldRelay);

	// Camera controller.
	B(IA_CamCycle, ET::Started, &UCricketPlayerInputComponent::OnCamCycle);
	B(IA_CamBallFollow, ET::Started, &UCricketPlayerInputComponent::OnCamBallFollow);
	B(IA_CamFree, ET::Started, &UCricketPlayerInputComponent::OnCamFree);

	// Replay controller.
	B(IA_ReplayToggle, ET::Started, &UCricketPlayerInputComponent::OnReplayToggle);
	B(IA_ReplayPause, ET::Started, &UCricketPlayerInputComponent::OnReplayPause);
	B(IA_ReplaySlowDown, ET::Started, &UCricketPlayerInputComponent::OnReplaySlowDown);
	B(IA_ReplaySlowUp, ET::Started, &UCricketPlayerInputComponent::OnReplaySlowUp);
	B(IA_ReplayStepBack, ET::Started, &UCricketPlayerInputComponent::OnReplayStepBack);
	B(IA_ReplayStepFwd, ET::Started, &UCricketPlayerInputComponent::OnReplayStepFwd);

	// Add the shared base layer and start in the batting layer.
	if (UEnhancedInputLocalPlayerSubsystem* Sub = (PC && PC->GetLocalPlayer())
		? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		Sub->AddMappingContext(IMC_Match, 0);
	}
	SetContext(ECricketInputContext::Batting);
}

void UCricketPlayerInputComponent::SetContext(ECricketInputContext NewContext)
{
	ActiveContext = NewContext;
	APlayerController* PC = Controller.Get();
	UEnhancedInputLocalPlayerSubsystem* Sub = (PC && PC->GetLocalPlayer())
		? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Sub) { return; }

	// Swap the role layer; the Match base layer stays on underneath.
	Sub->RemoveMappingContext(IMC_Batting);
	Sub->RemoveMappingContext(IMC_Bowling);
	Sub->RemoveMappingContext(IMC_Fielding);
	Sub->RemoveMappingContext(IMC_Replay);
	switch (NewContext)
	{
	case ECricketInputContext::Batting:  Sub->AddMappingContext(IMC_Batting, 1); break;
	case ECricketInputContext::Bowling:  Sub->AddMappingContext(IMC_Bowling, 1); break;
	case ECricketInputContext::Fielding: Sub->AddMappingContext(IMC_Fielding, 1); break;
	case ECricketInputContext::Replay:   Sub->AddMappingContext(IMC_Replay, 2); break;
	default: break;
	}
}

// ============================ Batting controller =============================

void UCricketPlayerInputComponent::OnFootFront(const FInputActionValue&)    { BattingState.bFrontFoot = true; }
void UCricketPlayerInputComponent::OnFootFrontEnd(const FInputActionValue&) { BattingState.bFrontFoot = false; }
void UCricketPlayerInputComponent::OnFootBack(const FInputActionValue&)     { BattingState.bBackFoot = true; }
void UCricketPlayerInputComponent::OnFootBackEnd(const FInputActionValue&)  { BattingState.bBackFoot = false; }
void UCricketPlayerInputComponent::OnDefensive(const FInputActionValue&)    { BattingState.bDefensive = true; }
void UCricketPlayerInputComponent::OnDefensiveEnd(const FInputActionValue&) { BattingState.bDefensive = false; }
void UCricketPlayerInputComponent::OnLofted(const FInputActionValue&)       { BattingState.bLofted = true; }
void UCricketPlayerInputComponent::OnLoftedEnd(const FInputActionValue&)    { BattingState.bLofted = false; }
void UCricketPlayerInputComponent::OnDir(ECricketShotDirection Dir)         { BattingState.Direction = Dir; }

void UCricketPlayerInputComponent::OnPlayShot(const FInputActionValue&)
{
	UCricketBattingComponent* Bat = Batting.Get();
	if (!Bat) { return; }

	LastShotIntent = FCricketInputModel::ResolveBattingShot(BattingState);
	const FCricketBattingInput In = FCricketInputModel::ToBattingInput(LastShotIntent, bRightHandedBatter);
	Bat->SetHanded(In.bRightHanded);
	Bat->SelectShot(In.ShotType);
	Bat->SetFootwork(In.Footwork);
	Bat->SetAimYawDeg(In.AimYawDeg);
	Bat->SetPower(In.PowerScale);
	Bat->TriggerSwing();   // the timing; the contact + physics decide the result
	bHasPlayedShot = true;
}

// ============================ Bowling controller =============================

void UCricketPlayerInputComponent::OnDeliveryStock(const FInputActionValue&) { BowlingState.Delivery = ECricketDeliveryChoice::Stock; if (Bowling.IsValid()) { Bowling->SetPace01(0.8); } }
void UCricketPlayerInputComponent::OnDeliveryVar(const FInputActionValue&)   { BowlingState.Delivery = ECricketDeliveryChoice::Variation; if (Bowling.IsValid()) { Bowling->SetPace01(0.65); } }
void UCricketPlayerInputComponent::OnDeliveryAggr(const FInputActionValue&)  { BowlingState.Delivery = ECricketDeliveryChoice::Aggressive; if (Bowling.IsValid()) { Bowling->SetPace01(1.0); } }
void UCricketPlayerInputComponent::OnLineLeft(const FInputActionValue&)   { BowlingState.LineStep = -1; if (Bowling.IsValid()) { Bowling->StepLine(-1); } }
void UCricketPlayerInputComponent::OnLineRight(const FInputActionValue&)  { BowlingState.LineStep = +1; if (Bowling.IsValid()) { Bowling->StepLine(+1); } }
void UCricketPlayerInputComponent::OnLengthUp(const FInputActionValue&)   { BowlingState.LengthStep = +1; if (Bowling.IsValid()) { Bowling->StepLength(+1); } }
void UCricketPlayerInputComponent::OnLengthDown(const FInputActionValue&) { BowlingState.LengthStep = -1; if (Bowling.IsValid()) { Bowling->StepLength(-1); } }
void UCricketPlayerInputComponent::OnSwingMod(const FInputActionValue&)    { BowlingState.bSwingMod = true; }
void UCricketPlayerInputComponent::OnSwingModEnd(const FInputActionValue&) { BowlingState.bSwingMod = false; }
void UCricketPlayerInputComponent::OnSpinMod(const FInputActionValue&)     { BowlingState.bSpinMod = true; }
void UCricketPlayerInputComponent::OnSpinModEnd(const FInputActionValue&)  { BowlingState.bSpinMod = false; }

void UCricketPlayerInputComponent::OnBowl(const FInputActionValue&)
{
	UCricketBowlingComponent* Bowl = Bowling.Get();
	if (!Bowl) { return; }
	const FCricketBowlingControlIntent BI = FCricketInputModel::ResolveDelivery(BowlingState);
	Bowl->SetPace01(BI.Pace01);
	Bowl->SetSwingAmount(BI.SwingAmount);
	Bowl->SetSpinAmount(BI.SpinAmount);
	Bowl->BowlNow();   // release conditions only; the flight is pure physics
}

// ============================ Running controller =============================

void UCricketPlayerInputComponent::OnRunTake(const FInputActionValue&)     { LastRunCall = FCricketInputModel::ResolveRunCall(true, false, false); }
void UCricketPlayerInputComponent::OnRunSendBack(const FInputActionValue&) { LastRunCall = FCricketInputModel::ResolveRunCall(false, true, false); }
void UCricketPlayerInputComponent::OnRunDive(const FInputActionValue&)     { LastRunCall = FCricketInputModel::ResolveRunCall(false, false, true); }

// ============================ Fielding controller ============================
// The fielder keeps using its own prediction/state machine; these record the
// player's requested action (and could nudge the controlled fielder).

void UCricketPlayerInputComponent::OnFieldCatch(const FInputActionValue&) { LastFieldAction = FCricketInputModel::ResolveFieldAction(true, false, false, false, false); }
void UCricketPlayerInputComponent::OnFieldThrow(const FInputActionValue&) { LastFieldAction = FCricketInputModel::ResolveFieldAction(false, false, true, false, false); }
void UCricketPlayerInputComponent::OnFieldDive(const FInputActionValue&)  { LastFieldAction = FCricketInputModel::ResolveFieldAction(false, true, false, false, false); }
void UCricketPlayerInputComponent::OnFieldRelay(const FInputActionValue&) { LastFieldAction = FCricketInputModel::ResolveFieldAction(false, false, false, true, false); }

// ============================ Camera controller ==============================

void UCricketPlayerInputComponent::OnCamCycle(const FInputActionValue&)      { if (Camera.IsValid()) { Camera->CycleGameplayMode(+1); } }
void UCricketPlayerInputComponent::OnCamBallFollow(const FInputActionValue&) { if (Camera.IsValid()) { Camera->SetMode(ECricketCameraMode::BallFollow); } }
void UCricketPlayerInputComponent::OnCamFree(const FInputActionValue&)       { if (Camera.IsValid()) { Camera->SetMode(ECricketCameraMode::Free); } }

// ============================ Replay controller ==============================

void UCricketPlayerInputComponent::OnReplayToggle(const FInputActionValue&)
{
	UCricketReplayComponent* Rep = Replay.Get();
	if (!Rep) { return; }
	if (Rep->IsReplaying()) { Rep->StopReplay(); SetContext(ECricketInputContext::Batting); }
	else { Rep->StartReplay(); SetContext(ECricketInputContext::Replay); }
}
void UCricketPlayerInputComponent::OnReplayPause(const FInputActionValue&)    { if (Replay.IsValid()) { Replay->TogglePause(); } }
void UCricketPlayerInputComponent::OnReplaySlowDown(const FInputActionValue&) { if (Replay.IsValid()) { Replay->AdjustRate(-0.25); } }
void UCricketPlayerInputComponent::OnReplaySlowUp(const FInputActionValue&)   { if (Replay.IsValid()) { Replay->AdjustRate(+0.25); } }
void UCricketPlayerInputComponent::OnReplayStepBack(const FInputActionValue&) { if (Replay.IsValid()) { Replay->StepFrames(-1); } }
void UCricketPlayerInputComponent::OnReplayStepFwd(const FInputActionValue&)  { if (Replay.IsValid()) { Replay->StepFrames(+1); } }
