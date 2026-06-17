#include "CricketPlayerPawn.h"
#include "CricketBattingComponent.h"
#include "CricketBattingDebugComponent.h"
#include "CricketCameraDirectorComponent.h"
#include "CricketReplayComponent.h"
#include "CricketPlayerInputComponent.h"
#include "CricketInputDebugComponent.h"
#include "CricketBowlingComponent.h"
#include "CricketBowlingActionAsset.h"
#include "CricketBall.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketCameraTypes.h"
#include "CricketPhysicsConstants.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

using namespace CricketPhysics;

ACricketPlayerPawn::ACricketPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bUsePawnControlRotation = false;

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
	Director = CreateDefaultSubobject<UCricketCameraDirectorComponent>(TEXT("CameraDirector"));
	Replay = CreateDefaultSubobject<UCricketReplayComponent>(TEXT("Replay"));
	Input = CreateDefaultSubobject<UCricketPlayerInputComponent>(TEXT("PlayerInput"));
	InputDebug = CreateDefaultSubobject<UCricketInputDebugComponent>(TEXT("InputDebug"));

	BallClass = ACricketBall::StaticClass();
}

void ACricketPlayerPawn::BeginPlay()
{
	Super::BeginPlay();

	// Face the bowler: forward becomes -X (down the pitch toward the feeder).
	SetActorRotation(FRotator(0.0, 180.0, 0.0));

	UWorld* World = GetWorld();
	if (!World) { return; }
	if (!BallClass) { BallClass = ACricketBall::StaticClass(); }

	const FVector RigLoc = GetActorLocation();
	const FVector Forward = GetActorForwardVector(); // -X world
	StrikerStumpsCm = RigLoc;
	BowlerStumpsCm = RigLoc + Forward * PitchLengthCm;
	const FVector FeederLoc = BowlerStumpsCm + FVector(0, 0, 200.0);
	const FRotator FeederRot = (-Forward).Rotation();

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Ball = World->SpawnActor<ACricketBall>(BallClass, FeederLoc, FeederRot, SP);

	// The feeder: a real bowling component on an anchor at the bowler's end.
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

	if (Batting) { Batting->SetTargetBall(Ball); }
	if (Replay) { Replay->SetBall(Ball); Replay->RegisterActor(this, 1); }
	if (Director)
	{
		Director->SetCamera(Camera);
		Director->Config.HeightCm = 200.0;
		Director->SetMode(ECricketCameraMode::Batting, 0.f);
	}
	if (Input) { Input->SetTargets(Batting, Feeder, nullptr, Director, Replay); }

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

void ACricketPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (Input) { Input->SetupInput(EIC, Cast<APlayerController>(GetController())); }
	}
}

void ACricketPlayerPawn::FeedDelivery()
{
	if (Feeder) { Feeder->BowlNow(); }
}

void ACricketPlayerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UCricketBallPhysicsComponent* BP = Ball ? Ball->GetBallPhysics() : nullptr;
	const bool bLive = BP && BP->IsBallInFlight();
	const bool bReplaying = Replay && Replay->IsReplaying();

	// Auto-feed deliveries so a ball is always coming (no bowler-side AI needed).
	if (!bLive && !bReplaying)
	{
		FeedTimer += DeltaSeconds;
		if (FeedTimer >= FeedInterval) { FeedTimer = 0.f; FeedDelivery(); }
	}
	else { FeedTimer = 0.f; }

	// Drive the camera from the live (or replayed) subjects.
	if (Director)
	{
		FCricketCameraSubjects S;
		S.BatterStumpsCm = StrikerStumpsCm;
		S.BowlerStumpsCm = BowlerStumpsCm;
		S.BallCm = bReplaying ? Replay->GetReplayBallCm() : (Ball ? Ball->GetActorLocation() : StrikerStumpsCm);
		S.BallVelocityMS = BP ? BP->GetVelocityMS() : FVector::ZeroVector;
		S.bBallInFlight = bReplaying || bLive;
		S.OrbitPivotCm = S.BallCm;
		Director->ApplyToCamera(S, DeltaSeconds);
	}
}
