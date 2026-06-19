#include "CricketBowlingRig.h"
#include "CricketBowlingComponent.h"
#include "CricketBowlingDebugComponent.h"
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

ACricketBowlingRig::ACricketBowlingRig()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	// Behind and above the bowler, looking down the pitch toward the striker.
	Camera->SetRelativeLocationAndRotation(FVector(-450.0, 0.0, 350.0), FRotator(-18.0, 0.0, 0.0));
	Camera->bUsePawnControlRotation = false;

	// A thin WorldStatic collision slab running down the pitch (top surface at the
	// rig's ground level), so UCricketBallPhysicsComponent's world sweep finds a
	// pitch to bounce on even in an otherwise empty level. Query-only: it exists to
	// be swept against, not to push anything around.
	PitchCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("PitchCollision"));
	PitchCollision->SetupAttachment(Root);
	PitchCollision->SetBoxExtent(FVector(1100.0, 300.0, 5.0));      // cm: ~22 m long, 6 m wide, 10 cm thick
	PitchCollision->SetRelativeLocation(FVector(1000.0, 0.0, -5.0)); // forward along the pitch; top face at Z=0
	PitchCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PitchCollision->SetCollisionObjectType(ECC_WorldStatic);
	PitchCollision->SetCollisionResponseToAllChannels(ECR_Block);
	PitchCollision->SetGenerateOverlapEvents(false);
	PitchCollision->CanCharacterStepUpOn = ECB_No;

	Bowling = CreateDefaultSubobject<UCricketBowlingComponent>(TEXT("Bowling"));
	BowlingDebug = CreateDefaultSubobject<UCricketBowlingDebugComponent>(TEXT("BowlingDebug"));
	Anim = CreateDefaultSubobject<UCricketCharacterAnimComponent>(TEXT("Anim"));
	AnimDebug = CreateDefaultSubobject<UCricketAnimDebugComponent>(TEXT("AnimDebug"));

	BallClass = ACricketBall::StaticClass();
}

void ACricketBowlingRig::BeginPlay()
{
	Super::BeginPlay();

	// Built-in bowlers cycled with Tab.
	Bowlers =
	{
		UCricketBowlingActionAsset::MakeExpressQuick(),
		UCricketBowlingActionAsset::MakeSwingBowler(),
		UCricketBowlingActionAsset::MakeOffSpinner(),
		UCricketBowlingActionAsset::MakeLegSpinner(),
	};
	BowlerIndex = 0;
	if (Bowling)
	{
		Bowling->SetAction(Bowlers[BowlerIndex]);
	}

	// Spawn the target ball at the release point.
	if (UWorld* World = GetWorld())
	{
		if (!BallClass) { BallClass = ACricketBall::StaticClass(); }
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector ReleaseCm = Bowling ? Bowling->GetReleaseWorldCm() : GetActorLocation();
		Ball = World->SpawnActor<ACricketBall>(BallClass, ReleaseCm, FRotator::ZeroRotator, Params);
		if (Bowling && Ball)
		{
			Bowling->SetTargetBall(Ball);
		}
	}

	// Pin the striker's stumps a pitch-length down the bowler's forward axis.
	if (Bowling)
	{
		const FVector Loc = GetActorLocation();
		FVector Fwd = GetActorForwardVector();
		Fwd.Z = 0.0;
		Fwd = Fwd.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(1, 0, 0));
		FVector Striker = Loc + Fwd * (Bowling->GetAction().ReleaseToStumpsM * MetersToUE);
		Striker.Z = Loc.Z;
		Bowling->SetStrikerStumpsWorldCm(Striker);
	}

	// Route the run-up's release notify to the actual bowl: Input -> Animation ->
	// Physics. The animation decides WHEN the ball leaves the hand; the delivery
	// generator + ball physics decide everything the ball then does.
	if (Anim)
	{
		Anim->OnAnimNotify.AddDynamic(this, &ACricketBowlingRig::HandleAnimNotify);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
		EnableInput(PC);
	}
}

void ACricketBowlingRig::HandleAnimNotify(ECricketAnimNotify Notify)
{
	if (Notify == ECricketAnimNotify::BallRelease && Bowling)
	{
		Bowling->BowlNow();
	}
}

void ACricketBowlingRig::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PollInput(DeltaSeconds);
	DrawControlsHelp();
}

void ACricketBowlingRig::PollInput(float /*DeltaSeconds*/)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !Bowling)
	{
		return;
	}

	// Bowl: start the run-up animation; its BallRelease notify fires the delivery.
	// (Falls back to an immediate bowl if the anim controller is somehow absent.)
	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		if (Anim && !Anim->IsBowlingActionPlaying()) { Anim->StartBowlingAction(); }
		else if (!Anim) { Bowling->BowlNow(); }
	}

	// Length (fuller / shorter) and line (toward leg / toward off).
	if (PC->WasInputKeyJustPressed(EKeys::Up))    { Bowling->StepLength(-1); }
	if (PC->WasInputKeyJustPressed(EKeys::Down))  { Bowling->StepLength(+1); }
	if (PC->WasInputKeyJustPressed(EKeys::Right)) { Bowling->StepLine(+1); }
	if (PC->WasInputKeyJustPressed(EKeys::Left))  { Bowling->StepLine(-1); }

	// Pace (brackets or mouse wheel).
	if (PC->WasInputKeyJustPressed(EKeys::RightBracket) || PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		Bowling->AdjustPace(+PaceStep);
	}
	if (PC->WasInputKeyJustPressed(EKeys::LeftBracket) || PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		Bowling->AdjustPace(-PaceStep);
	}

	// Swing / spin amount.
	if (PC->WasInputKeyJustPressed(EKeys::E)) { Bowling->AdjustSwing(+AmountStep); }
	if (PC->WasInputKeyJustPressed(EKeys::Q)) { Bowling->AdjustSwing(-AmountStep); }
	if (PC->WasInputKeyJustPressed(EKeys::C)) { Bowling->AdjustSpin(+AmountStep); }
	if (PC->WasInputKeyJustPressed(EKeys::Z)) { Bowling->AdjustSpin(-AmountStep); }

	// Movement / bowler cycling.
	if (PC->WasInputKeyJustPressed(EKeys::M))   { CycleMovement(+1); }
	if (PC->WasInputKeyJustPressed(EKeys::Tab)) { CycleBowler(+1); }

	// Preset deliveries 1..8.
	static const FKey NumberKeys[8] =
	{
		EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight
	};
	for (int32 i = 0; i < 8; ++i)
	{
		if (PC->WasInputKeyJustPressed(NumberKeys[i]))
		{
			Bowling->SelectPreset(i);
		}
	}

	// Ball ageing.
	if (PC->WasInputKeyJustPressed(EKeys::Hyphen)) { Bowling->AgeBall(0.1); }
	if (PC->WasInputKeyJustPressed(EKeys::R))      { Bowling->ResetBall(); }

	// Mouse fine aim: X trims line, Y trims length.
	float DX = 0.f, DY = 0.f;
	PC->GetInputMouseDelta(DX, DY);
	if (FMath::Abs(DX) > KINDA_SMALL_NUMBER || FMath::Abs(DY) > KINDA_SMALL_NUMBER)
	{
		const FCricketBowlingIntent& In = Bowling->GetIntent();
		Bowling->SetLineFineM(FMath::Clamp(In.LineFineM + DX * MouseAimSensitivity, -0.6, 0.6));
		Bowling->SetLengthFineM(FMath::Clamp(In.LengthFineM - DY * MouseAimSensitivity, -2.0, 2.0));
	}
}

void ACricketBowlingRig::CycleMovement(int32 Dir)
{
	if (!Bowling) { return; }
	constexpr int32 Count = static_cast<int32>(ECricketMovement::LegBreak) + 1;
	const int32 Cur = static_cast<int32>(Bowling->GetIntent().Movement);
	const int32 Next = ((Cur + Dir) % Count + Count) % Count;
	Bowling->SetMovement(static_cast<ECricketMovement>(Next));
}

void ACricketBowlingRig::CycleBowler(int32 Dir)
{
	if (!Bowling || Bowlers.Num() == 0) { return; }
	BowlerIndex = ((BowlerIndex + Dir) % Bowlers.Num() + Bowlers.Num()) % Bowlers.Num();
	Bowling->SetAction(Bowlers[BowlerIndex]);
}

void ACricketBowlingRig::DrawControlsHelp() const
{
#if UE_BUILD_SHIPPING
	return; // Developer harness overlay — compiled out of Shipping builds.
#else
	if (!GEngine || !Bowling) { return; }

	const FString BowlerName = Bowling->GetAction().BowlerName.ToString();
	GEngine->AddOnScreenDebugMessage(4100, 0.f, FColor::Silver,
		FString::Printf(TEXT("Bowler [Tab]: %s   (presets 1-%d)"), *BowlerName, Bowling->NumPresets()));
	GEngine->AddOnScreenDebugMessage(4101, 0.f, FColor::Silver,
		TEXT("Space=Bowl  Up/Dn=Length  L/R=Line  [ ]=Pace  Q/E=Swing  Z/C=Spin  M=Movement  - =Scuff  R=NewBall"));
#endif
}
