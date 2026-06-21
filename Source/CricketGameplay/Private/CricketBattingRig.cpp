#include "CricketBattingRig.h"
#include "CricketBattingComponent.h"
#include "CricketBattingDebugComponent.h"
#include "CricketBowlingComponent.h"
#include "CricketBowlingActionAsset.h"
#include "CricketCharacterAnimComponent.h"
#include "CricketAnimDebugComponent.h"
#include "CricketBall.h"
#include "CricketPhysicsConstants.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"

using namespace CricketPhysics;

ACricketBattingRig::ACricketBattingRig()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	// Behind and above the striker, looking down the pitch toward the bowler.
	Camera->SetRelativeLocationAndRotation(FVector(-420.0, 0.0, 320.0), FRotator(-16.0, 0.0, 0.0));
	Camera->bUsePawnControlRotation = false;

	// Query-only pitch slab running from the striker toward the bowler (relative +X
	// becomes world -X once the rig faces the bowler), so the fed ball bounces.
	PitchCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PitchCollision"));
	PitchCollision->SetupAttachment(Root);
	PitchCollision->SetBoxExtent(FVector(1100.0, 300.0, 5.0));
	PitchCollision->SetRelativeLocation(FVector(1000.0, 0.0, -5.0));
	PitchCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PitchCollision->SetCollisionObjectType(ECC_WorldStatic);
	PitchCollision->SetCollisionResponseToAllChannels(ECR_Block);
	PitchCollision->SetGenerateOverlapEvents(false);
	PitchCollision->CanCharacterStepUpOn = ECB_No;

	Batting = CreateDefaultSubobject<UCricketBattingComponent>(TEXT("Batting"));
	BattingDebug = CreateDefaultSubobject<UCricketBattingDebugComponent>(TEXT("BattingDebug"));

	// Follows the batting sim: derives the Backlift/Downswing/Impact/FollowThrough
	// anim state, and fires BatImpact exactly when the swing meets the ball.
	Anim = CreateDefaultSubobject<UCricketCharacterAnimComponent>(TEXT("Anim"));
	AnimDebug = CreateDefaultSubobject<UCricketAnimDebugComponent>(TEXT("AnimDebug"));

	BallClass = ACricketBall::StaticClass();
}

void ACricketBattingRig::BeginPlay()
{
	Super::BeginPlay();

	// Face the bowler: forward becomes -X (down the pitch toward the feeder).
	SetActorRotation(FRotator(0.0, 180.0, 0.0));

	UWorld* World = GetWorld();
	if (!World) { return; }

	const FVector RigLoc = GetActorLocation();
	const FVector Forward = GetActorForwardVector(); // -X world
	const FVector FeederLoc = RigLoc + Forward * PitchLengthCm + FVector(0, 0, 200.0); // bowler hand height
	const FRotator FeederRot = (-Forward).Rotation(); // face back toward the striker

	// Spawn the ball at the bowler's end.
	if (!BallClass) { BallClass = ACricketBall::StaticClass(); }
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Ball = World->SpawnActor<ACricketBall>(BallClass, FeederLoc, FeederRot, SP);

	// Build the feeder from the REAL bowling component on an anchor at the bowler's end.
	FeederAnchor = World->SpawnActor<AActor>(AActor::StaticClass(), FeederLoc, FeederRot, SP);
	if (FeederAnchor)
	{
		USceneComponent* FRoot = NewObject<USceneComponent>(FeederAnchor, TEXT("FeederRoot"));
		FeederAnchor->SetRootComponent(FRoot);
		FRoot->RegisterComponent();
		FeederAnchor->SetActorLocationAndRotation(FeederLoc, FeederRot);

		Feeder = NewObject<UCricketBowlingComponent>(FeederAnchor, TEXT("Feeder"));
		Feeder->RegisterComponent();
		Feeder->SetAction(UCricketBowlingActionAsset::MakeExpressQuick());
		Feeder->SetTargetBall(Ball);
		Feeder->SetStrikerStumpsWorldCm(RigLoc);
	}

	if (Batting)
	{
		Batting->SetTargetBall(Ball);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		EnableInput(PC);
	}
}

void ACricketBattingRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PollInput(DeltaSeconds);
	DrawControlsHelp();
}

void ACricketBattingRig::PollInput(float /*DeltaSeconds*/)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !Batting) { return; }

	// Play the selected stroke — the timing input.
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		Batting->TriggerSwing();
	}

	// Shot selection.
	if (PC->WasInputKeyJustPressed(EKeys::One))   { Batting->SelectShot(ECricketShotType::DefensiveBlock); }
	if (PC->WasInputKeyJustPressed(EKeys::Two))   { Batting->SelectShot(ECricketShotType::StraightDrive); }
	if (PC->WasInputKeyJustPressed(EKeys::Three)) { Batting->SelectShot(ECricketShotType::CoverDrive); }
	if (PC->WasInputKeyJustPressed(EKeys::Four))  { Batting->SelectShot(ECricketShotType::PullShot); }

	// Footwork.
	if (PC->WasInputKeyJustPressed(EKeys::Up))   { Batting->SetFootwork(ECricketFootwork::FrontFoot); }
	if (PC->WasInputKeyJustPressed(EKeys::Down)) { Batting->SetFootwork(ECricketFootwork::BackFoot); }
	if (PC->WasInputKeyJustPressed(EKeys::N))    { Batting->SetFootwork(ECricketFootwork::Neutral); }

	// Aim trim (keys + mouse X).
	if (PC->WasInputKeyJustPressed(EKeys::Left))  { Batting->AdjustAimYawDeg(-AimStepDeg); }
	if (PC->WasInputKeyJustPressed(EKeys::Right)) { Batting->AdjustAimYawDeg(+AimStepDeg); }
	float DX = 0.f, DY = 0.f;
	PC->GetInputMouseDelta(DX, DY);
	if (FMath::Abs(DX) > KINDA_SMALL_NUMBER)
	{
		Batting->AdjustAimYawDeg(DX * MouseAimSensitivity);
	}

	// Power.
	if (PC->WasInputKeyJustPressed(EKeys::E)) { Batting->AdjustPower(+PowerStep); }
	if (PC->WasInputKeyJustPressed(EKeys::Q)) { Batting->AdjustPower(-PowerStep); }

	// Feeder.
	if (PC->WasInputKeyJustPressed(EKeys::F)) { Feed(); }
	if (Feeder)
	{
		if (PC->WasInputKeyJustPressed(EKeys::RightBracket)) { Feeder->StepLength(-1); } // fuller
		if (PC->WasInputKeyJustPressed(EKeys::LeftBracket))  { Feeder->StepLength(+1); } // shorter
		if (PC->WasInputKeyJustPressed(EKeys::Tab)) { CycleFeeder(+1); }
		if (PC->WasInputKeyJustPressed(EKeys::R))   { Feeder->ResetBall(); }
	}
}

void ACricketBattingRig::Feed()
{
	if (Feeder)
	{
		Feeder->BowlNow();
	}
}

void ACricketBattingRig::CycleFeeder(int32 Dir)
{
	if (!Feeder) { return; }
	static const int32 Count = 4;
	FeederIndex = ((FeederIndex + Dir) % Count + Count) % Count;
	switch (FeederIndex)
	{
	case 0: Feeder->SetAction(UCricketBowlingActionAsset::MakeExpressQuick()); break;
	case 1: Feeder->SetAction(UCricketBowlingActionAsset::MakeSwingBowler()); break;
	case 2: Feeder->SetAction(UCricketBowlingActionAsset::MakeOffSpinner()); break;
	case 3: Feeder->SetAction(UCricketBowlingActionAsset::MakeLegSpinner()); break;
	default: break;
	}
}

void ACricketBattingRig::DrawControlsHelp() const
{
#if UE_BUILD_SHIPPING
	return; // Developer harness overlay — compiled out of Shipping builds.
#else
	if (!GEngine) { return; }
	GEngine->AddOnScreenDebugMessage(4200, 0.f, FColor::Silver,
		TEXT("Space=Play  1-4=Shot  Up/Dn=Foot  N=Neutral  L/R/Mouse=Aim  Q/E=Power  F=Feed  [ ]=Length  Tab=Bowler  R=NewBall"));
#endif
}
