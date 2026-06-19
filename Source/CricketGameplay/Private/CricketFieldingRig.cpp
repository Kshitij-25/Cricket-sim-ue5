#include "CricketFieldingRig.h"
#include "CricketFielder.h"
#include "CricketFielderComponent.h"
#include "CricketBall.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketFieldingPredictor.h"
#include "CricketCameraDirectorComponent.h"
#include "CricketReplayComponent.h"
#include "CricketPhysicsConstants.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "InputCoreTypes.h"

using namespace CricketPhysics;

namespace
{
	// Fielding positions (cm) for a right-hander hitting toward +X (off side +Y).
	const TArray<FVector> kFieldOffsets =
	{
		FVector(1500, -800, 0),   // mid-on
		FVector(1500,  800, 0),   // mid-off
		FVector(1200,  1700, 0),  // cover
		FVector(1200, -1700, 0),  // midwicket
		FVector(2700, -1100, 0),  // long-on
		FVector(2700,  1100, 0),  // long-off
		FVector(300,   2300, 0),  // point
		FVector(300,  -2300, 0),  // square leg
	};

	// Launch presets: initial ball velocity (m/s) from the bat. Landings emerge
	// from the physics — these are launches, never scripted landing points.
	FVector PresetVelocity(int32 Index)
	{
		switch (Index)
		{
		case 1: return FVector(24, 6, 0.6);   // ground ball
		case 2: return FVector(16, 8, 12);    // lofted drive
		case 3: return FVector(5, 2, 22);     // high catch / skyer
		case 4: return FVector(30, 12, 5);    // boundary-bound
		case 5: return FVector(20, 0, 1.2);   // straight push (run-out)
		default: return FVector(20, 5, 6);
		}
	}
}

ACricketFieldingRig::ACricketFieldingRig()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	// High, angled view over the whole field from behind the striker.
	Camera->SetRelativeLocationAndRotation(FVector(-1100.0, 0.0, 1900.0), FRotator(-52.0, 0.0, 0.0));
	Camera->bUsePawnControlRotation = false;

	// A big query-only ground plane so struck/thrown balls bounce anywhere.
	GroundCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("GroundCollision"));
	GroundCollision->SetupAttachment(Root);
	GroundCollision->SetBoxExtent(FVector(5000.0, 5000.0, 5.0));
	GroundCollision->SetRelativeLocation(FVector(2000.0, 0.0, -5.0));
	GroundCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GroundCollision->SetCollisionObjectType(ECC_WorldStatic);
	GroundCollision->SetCollisionResponseToAllChannels(ECR_Block);
	GroundCollision->SetGenerateOverlapEvents(false);
	GroundCollision->CanCharacterStepUpOn = ECB_No;

	Director = CreateDefaultSubobject<UCricketCameraDirectorComponent>(TEXT("CameraDirector"));
	Replay = CreateDefaultSubobject<UCricketReplayComponent>(TEXT("Replay"));

	BallClass = ACricketBall::StaticClass();
	FielderClass = ACricketFielder::StaticClass();
}

void ACricketFieldingRig::BeginPlay()
{
	Super::BeginPlay();
	SpawnField();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		EnableInput(PC);
	}
}

void ACricketFieldingRig::SpawnField()
{
	UWorld* World = GetWorld();
	if (!World) { return; }
	if (!BallClass) { BallClass = ACricketBall::StaticClass(); }
	if (!FielderClass) { FielderClass = ACricketFielder::StaticClass(); }

	const FVector Origin = GetActorLocation(); // the bat / striker's stumps
	StrikerStumpsCm = Origin;
	BowlerStumpsCm = Origin + FVector(2000.0, 0.0, 0.0);
	KeeperCm = Origin + FVector(-350.0, 0.0, 0.0);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Ball = World->SpawnActor<ACricketBall>(BallClass, Origin + FVector(0, 0, 80), FRotator::ZeroRotator, SP);

	auto MakeFielder = [&](const FVector& Pos, ECricketThrowTarget Pref) -> ACricketFielder*
	{
		ACricketFielder* F = World->SpawnActor<ACricketFielder>(FielderClass, Pos, FRotator::ZeroRotator, SP);
		if (F && F->GetFielder())
		{
			UCricketFielderComponent* C = F->GetFielder();
			C->SetTargetBall(Ball);
			C->SetHomePosition(Pos);
			C->SetThrowContext(StrikerStumpsCm, KeeperCm);
			C->ThrowPreference = Pref;
			C->OnThrew.AddDynamic(this, &ACricketFieldingRig::HandleThrew);
			C->OnStateChanged.AddDynamic(this, &ACricketFieldingRig::HandleFielderState);
		}
		return F;
	};

	Fielders.Reset();
	for (const FVector& Off : kFieldOffsets)
	{
		if (ACricketFielder* F = MakeFielder(Origin + Off, ECricketThrowTarget::Keeper))
		{
			Fielders.Add(F);
		}
	}
	// The keeper: stands behind the stumps, receives throws, takes nicks.
	Keeper = MakeFielder(KeeperCm, ECricketThrowTarget::Keeper);
	if (Keeper && Keeper->GetFielder())
	{
		Keeper->GetFielder()->MaxSpeedMS = 5.5; // keepers cover less ground than the ring
		Fielders.Add(Keeper);
	}

	// Camera + replay wiring: record the ball and every fielder, drive the camera.
	if (Replay)
	{
		Replay->SetBall(Ball);
		for (int32 i = 0; i < Fielders.Num(); ++i)
		{
			if (Fielders[i]) { Replay->RegisterActor(Fielders[i], i + 1); }
		}
	}
	if (Director)
	{
		Director->SetCamera(Camera);
		Director->Config.HeightCm = 350.0;
		Director->SetMode(ECricketCameraMode::Spectator, 0.f);
	}
}

void ACricketFieldingRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PollCameraReplayInput();
	const bool bReplaying = Replay && Replay->IsReplaying();
	if (!bReplaying)
	{
		// Live gameplay drives the sim; during a replay the recording drives it.
		PollInput();
		Coordinate();
		DetectDirectHit();
	}
	UpdateCamera(DeltaSeconds);
	DrawFieldAndHelp();
	if (DirectHitMsgTimer > 0.f) { DirectHitMsgTimer -= DeltaSeconds; }
}

void ACricketFieldingRig::UpdateCamera(float DeltaSeconds)
{
	if (!Director) { return; }

	UCricketBallPhysicsComponent* BP = Ball ? Ball->GetBallPhysics() : nullptr;
	const bool bReplaying = Replay && Replay->IsReplaying();

	FCricketCameraSubjects S;
	S.BatterStumpsCm = StrikerStumpsCm;
	S.BowlerStumpsCm = BowlerStumpsCm;
	S.BallCm = bReplaying ? Replay->GetReplayBallCm() : (Ball ? Ball->GetActorLocation() : GetActorLocation());
	S.BallVelocityMS = BP ? BP->GetVelocityMS() : FVector::ZeroVector;
	S.bBallInFlight = bReplaying || (BP && BP->IsBallInFlight());
	S.OrbitPivotCm = S.BallCm;
	if (CamFielder)
	{
		S.ActiveFielderCm = CamFielder->GetActorLocation();
		S.bHasActiveFielder = true;
	}
	Director->ApplyToCamera(S, DeltaSeconds);
}

void ACricketFieldingRig::CycleReplayCamera()
{
	static const ECricketCameraMode ReplayCams[] = {
		ECricketCameraMode::BallFollow, ECricketCameraMode::Orbit,
		ECricketCameraMode::PhysicsInspection, ECricketCameraMode::Spectator };
	ReplayCamIndex = (ReplayCamIndex + 1) % UE_ARRAY_COUNT(ReplayCams);
	if (Director) { Director->SetMode(ReplayCams[ReplayCamIndex]); }
}

void ACricketFieldingRig::PollCameraReplayInput()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !Director || !Replay) { return; }

	if (PC->WasInputKeyJustPressed(EKeys::C))
	{
		if (Replay->IsReplaying()) { CycleReplayCamera(); } else { Director->CycleGameplayMode(+1); }
	}
	if (PC->WasInputKeyJustPressed(EKeys::V))
	{
		if (Replay->IsReplaying())
		{
			Replay->StopReplay();
			Director->SetMode(ECricketCameraMode::Spectator);
		}
		else
		{
			Replay->StartReplay();
			ReplayCamIndex = 0;
			Director->SetMode(ECricketCameraMode::BallFollow);
		}
	}

	if (Replay->IsReplaying())
	{
		if (PC->WasInputKeyJustPressed(EKeys::P))     { Replay->TogglePause(); }
		if (PC->WasInputKeyJustPressed(EKeys::Left))  { Replay->StepFrames(-1); }
		if (PC->WasInputKeyJustPressed(EKeys::Right)) { Replay->StepFrames(+1); }
		if (PC->WasInputKeyJustPressed(EKeys::Up))    { Replay->AdjustRate(+0.25); }
		if (PC->WasInputKeyJustPressed(EKeys::Down))  { Replay->AdjustRate(-0.25); }
	}
	else
	{
		// Adjustable framing for the chase cameras.
		if (PC->WasInputKeyJustPressed(EKeys::LeftBracket))  { Director->AdjustDistance(-80.0); }
		if (PC->WasInputKeyJustPressed(EKeys::RightBracket)) { Director->AdjustDistance(+80.0); }
	}
}

void ACricketFieldingRig::HandleFielderState(ECricketFielderState NewState)
{
	// Mark a catch on the timeline at the ball, for the replay overlay.
	if (Replay && Ball && NewState == ECricketFielderState::Catching)
	{
		Replay->MarkEvent(ECricketReplayEventType::Catch, Ball->GetActorLocation());
	}
}

void ACricketFieldingRig::Coordinate()
{
	UCricketBallPhysicsComponent* BallPhys = Ball ? Ball->GetBallPhysics() : nullptr;
	const bool bLive = BallPhys && BallPhys->IsBallInFlight();

	// Gather all teammate positions for the NearestFielder throw option.
	TArray<FVector> TeamPositions;
	for (const TObjectPtr<ACricketFielder>& F : Fielders)
	{
		if (F) { TeamPositions.Add(F->GetActorLocation()); }
	}

	// Pick the best chaser: the reachable fielder that meets the ball earliest.
	ACricketFielder* Best = nullptr;
	double BestTime = TNumericLimits<double>::Max();
	if (bLive)
	{
		for (const TObjectPtr<ACricketFielder>& F : Fielders)
		{
			if (!F || !F->GetFielder()) { continue; }
			// A fielder already holding/throwing the ball stays the chaser.
			if (F->GetFielder()->HasBall())
			{
				Best = F; BestTime = -1.0; break;
			}
			FCricketBallPrediction Pred;
			const FCricketInterceptResult R = F->GetFielder()->EvaluateIntercept(Pred);
			if (R.bCanIntercept && R.TimeSec < BestTime)
			{
				BestTime = R.TimeSec;
				Best = F;
			}
		}
	}

	for (const TObjectPtr<ACricketFielder>& F : Fielders)
	{
		if (!F || !F->GetFielder()) { continue; }
		UCricketFielderComponent* C = F->GetFielder();
		C->SetTeammates(TeamPositions);
		C->ThrowPreference = bRunOutMode ? ECricketThrowTarget::Stumps : ECricketThrowTarget::Keeper;
		C->SetActiveChaser(F == Best);
	}
	CamFielder = Best; // the fielding camera frames the active chaser
}

void ACricketFieldingRig::LaunchPreset(int32 Index)
{
	if (!Ball || !Ball->GetBallPhysics()) { return; }
	LastPreset = Index;

	FVector Vel = PresetVelocity(Index);
	Vel = FRotator(0.0, LaunchYawDeg, 0.0).RotateVector(Vel); // aim
	const FVector LaunchCm = GetActorLocation() + FVector(0, 0, 80);

	bMonitorStumps = false;
	bDirectHit = false;
	ClosestToStumpsM = 1e9;

	Ball->GetBallPhysics()->Release(LaunchCm, Vel, FVector::ZeroVector, FVector(0, 1, 0));
}

void ACricketFieldingRig::HandleThrew(FVector TargetWorldCm, FCricketThrowSolution /*Solution*/)
{
	// Mark the throw release on the replay timeline (the ball is at the hand now).
	if (Replay && Ball) { Replay->MarkEvent(ECricketReplayEventType::Throw, Ball->GetActorLocation()); }

	// If a throw was aimed at the stumps, start watching for a direct hit.
	if (FVector::Dist(TargetWorldCm, StrikerStumpsCm) < 50.0)
	{
		bMonitorStumps = true;
		ClosestToStumpsM = 1e9;
	}
}

void ACricketFieldingRig::DetectDirectHit()
{
	if (!bMonitorStumps || !Ball || !Ball->GetBallPhysics()) { return; }
	UCricketBallPhysicsComponent* BallPhys = Ball->GetBallPhysics();
	if (!BallPhys->IsBallInFlight()) { bMonitorStumps = false; return; }

	const FVector BallM = BallPhys->GetState().Position;
	const FVector StumpsM = WorldToMeters(StrikerStumpsCm);
	const double HorizM = FVector(BallM.X - StumpsM.X, BallM.Y - StumpsM.Y, 0.0).Size();

	ClosestToStumpsM = FMath::Min(ClosestToStumpsM, HorizM);
	// Stumps are ~0.23 m wide, ~0.71 m tall. A pass within that box is a direct hit.
	if (HorizM < 0.20 && BallM.Z < 0.75)
	{
		bDirectHit = true;
		bMonitorStumps = false;
		DirectHitMsgTimer = 3.0f;
	}
}

void ACricketFieldingRig::PollInput()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) { return; }

	if (PC->WasInputKeyJustPressed(EKeys::One))   { LaunchPreset(1); }
	if (PC->WasInputKeyJustPressed(EKeys::Two))   { LaunchPreset(2); }
	if (PC->WasInputKeyJustPressed(EKeys::Three)) { LaunchPreset(3); }
	if (PC->WasInputKeyJustPressed(EKeys::Four))  { LaunchPreset(4); }
	if (PC->WasInputKeyJustPressed(EKeys::Five))  { bRunOutMode = true; LaunchPreset(5); }
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar)) { LaunchPreset(LastPreset); }

	if (PC->WasInputKeyJustPressed(EKeys::Left))  { LaunchYawDeg = FMath::Clamp(LaunchYawDeg - AimStepDeg, -80.0, 80.0); }
	if (PC->WasInputKeyJustPressed(EKeys::Right)) { LaunchYawDeg = FMath::Clamp(LaunchYawDeg + AimStepDeg, -80.0, 80.0); }
	if (PC->WasInputKeyJustPressed(EKeys::T))     { bRunOutMode = !bRunOutMode; }
	if (PC->WasInputKeyJustPressed(EKeys::R))
	{
		if (Ball && Ball->GetBallPhysics())
		{
			Ball->GetBallPhysics()->Freeze();
			Ball->SetActorLocation(GetActorLocation() + FVector(0, 0, 80));
		}
		bMonitorStumps = false; bDirectHit = false;
	}
}

void ACricketFieldingRig::DrawFieldAndHelp() const
{
#if UE_BUILD_SHIPPING
	return; // Developer harness field/help overlay — compiled out of Shipping builds.
#else
	UWorld* World = GetWorld();
	if (!World) { return; }

	// Stumps.
	DrawDebugBox(World, StrikerStumpsCm + FVector(0, 0, 35), FVector(4, 12, 35), FQuat::Identity,
		bRunOutMode ? FColor::Red : FColor::White, false, -1.f, 0, 2.f);
	DrawDebugBox(World, BowlerStumpsCm + FVector(0, 0, 35), FVector(4, 12, 35), FQuat::Identity, FColor::White, false, -1.f, 0, 1.5f);
	DrawDebugString(World, KeeperCm + FVector(0, 0, 200), TEXT("keeper"), nullptr, FColor::Silver, 0.f, false, 1.0f);

	if (!GEngine) { return; }
	GEngine->AddOnScreenDebugMessage(5100, 0.f, FColor::Silver,
		TEXT("1=Ground 2=Lofted 3=HighCatch 4=Boundary 5=RunOut  Space=Replay  L/R=Aim  T=RunOutMode  R=Reset"));
	GEngine->AddOnScreenDebugMessage(5101, 0.f, bRunOutMode ? FColor::Red : FColor::Green,
		FString::Printf(TEXT("Run-out mode: %s   |   Aim %+.0f deg"), bRunOutMode ? TEXT("ON (throw at stumps)") : TEXT("off (throw to keeper)"), LaunchYawDeg));

	const bool bReplaying = Replay && Replay->IsReplaying();
	if (bReplaying)
	{
		GEngine->AddOnScreenDebugMessage(5103, 0.f, FColor::Cyan,
			FString::Printf(TEXT("REPLAY  %.0f%%  rate x%.2f %s  | C=Cam P=Pause L/R=Step Up/Dn=Speed V=Exit"),
				Replay->GetNormalizedTime() * 100.0, Replay->GetPlaybackRate(), Replay->IsPaused() ? TEXT("[paused]") : TEXT("")));
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(5103, 0.f, FColor::Cyan, TEXT("Camera: C=cycle mode  [ ]=distance   |   V=Replay last delivery"));
	}
	if (DirectHitMsgTimer > 0.f && bDirectHit)
	{
		GEngine->AddOnScreenDebugMessage(5102, 0.f, FColor::Red, TEXT(">>> DIRECT HIT — RUN OUT! <<<"));
	}
	else if (bMonitorStumps)
	{
		GEngine->AddOnScreenDebugMessage(5102, 0.f, FColor::Yellow,
			FString::Printf(TEXT("Throw at stumps... closest %.2f m"), ClosestToStumpsM));
	}
#endif
}
