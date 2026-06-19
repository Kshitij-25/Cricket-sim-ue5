#include "CricketMatchRunner.h"
#include "CricketMatchEngine.h"
#include "CricketOutcomeInterpreter.h"
#include "CricketDiagnosticsSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"

namespace
{
	FCricketSquad BuildSquad(const FString& Team, const FString& Code, const TArray<FString>& Names)
	{
		FCricketSquad S; S.TeamName = Team; S.ShortCode = Code; S.PlayerNames = Names; return S;
	}

	// Deterministic [0,1) value from the seed + a sequence index. No engine RNG.
	double Hash01(int32 Seed, int32 A, int32 B)
	{
		uint32 H = HashCombine(GetTypeHash(Seed), GetTypeHash(A * 2654435761u + (uint32)B));
		H = HashCombine(H, GetTypeHash((uint32)(A ^ (B << 16))));
		return (H % 1000000u) / 1000000.0;
	}
}

ACricketMatchRunner::ACricketMatchRunner()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void ACricketMatchRunner::BeginPlay()
{
	Super::BeginPlay();
	SetupMatch();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		EnableInput(PC);
	}
}

void ACricketMatchRunner::SetupMatch()
{
	if (!Engine) { Engine = NewObject<UCricketMatchEngine>(this); }

	India = BuildSquad(TEXT("India"), TEXT("IND"),
		{ TEXT("Rohit"), TEXT("Gill"), TEXT("Kohli"), TEXT("Suryakumar"), TEXT("Pandya"),
		  TEXT("Jadeja"), TEXT("Pant"), TEXT("Axar"), TEXT("Bumrah"), TEXT("Arshdeep"), TEXT("Chahal") });
	Australia = BuildSquad(TEXT("Australia"), TEXT("AUS"),
		{ TEXT("Head"), TEXT("Warner"), TEXT("Marsh"), TEXT("Smith"), TEXT("Maxwell"),
		  TEXT("Stoinis"), TEXT("Wade"), TEXT("Cummins"), TEXT("Starc"), TEXT("Hazlewood"), TEXT("Zampa") });

	FCricketMatchRules Rules;
	Rules.OversPerInnings = FMath::Max(1, OversPerInnings);
	Engine->ConfigureMatch(Rules, India, Australia);
	Engine->StartMatch();
	Engine->PerformToss(0, /*bWinnerBatsFirst*/ true); // India bat first

	DeliveryCounter = 0;
	BowlerRotation = 0;
	BallTimer = 0.f;
	bResultReported = false;

	// Telemetry: record match start (sets the crash breadcrumb context).
	if (UCricketDiagnosticsSubsystem* Diag = UCricketDiagnosticsSubsystem::Get(this))
	{
		Diag->RecordMatchStart(India.TeamName, Australia.TeamName, Rules.OversPerInnings);
	}
}

void ACricketMatchRunner::ReportResultIfComplete()
{
	if (bResultReported || !Engine || Engine->GetMatchState() != ECricketMatchState::MatchComplete)
	{
		return;
	}
	bResultReported = true;

	if (UCricketDiagnosticsSubsystem* Diag = UCricketDiagnosticsSubsystem::Get(this))
	{
		const FCricketInningsState& I0 = Engine->GetInnings(0).Totals;
		const FCricketInningsState& I1 = Engine->GetInnings(1).Totals;
		Diag->RecordMatchResult(Engine->GetResult(), I0.Runs, I0.Wickets, I1.Runs, I1.Wickets);
	}
}

void ACricketMatchRunner::EnsureBowler()
{
	if (!Engine || !Engine->NeedsBowler()) { return; }

	const FString BowlTeam = Engine->GetActiveInnings().BowlingTeam;
	const FCricketSquad& Squad = (BowlTeam == India.TeamName) ? India : Australia;

	// Try the back-of-order (all-rounders + bowlers), rotating the start each over.
	for (int32 k = 0; k < 7; ++k)
	{
		const int32 Idx = 4 + ((BowlerRotation + k) % 7); // indices 4..10
		if (Squad.PlayerNames.IsValidIndex(Idx) && Engine->SetBowler(Squad.PlayerNames[Idx]))
		{
			BowlerRotation = (BowlerRotation + k + 1) % 7;
			return;
		}
	}
	// Fallback: anyone legal.
	for (const FString& Name : Squad.PlayerNames)
	{
		if (Engine->SetBowler(Name)) { return; }
	}
}

FCricketBallResult ACricketMatchRunner::GenerateResult()
{
	const int32 Innings = Engine->GetActiveInningsIndex();
	const double v = Hash01(Seed, DeliveryCounter, Innings * 7919 + 1);

	FCricketBallResult R;
	if      (v < 0.30) { R.bStruck = true; R.RunsRun = 0; }                       // dot
	else if (v < 0.58) { R.bStruck = true; R.RunsRun = 1; }                       // single
	else if (v < 0.70) { R.bStruck = true; R.RunsRun = 2; }                       // two
	else if (v < 0.74) { R.bStruck = true; R.RunsRun = 3; }                       // three
	else if (v < 0.85) { R.bStruck = true; R.bBoundaryFour = true; }              // four
	else if (v < 0.91) { R.bStruck = true; R.bBoundarySix = true; }               // six
	else if (v < 0.945){ R.bWide = true; R.RunsRun = (Hash01(Seed, DeliveryCounter, 31) < 0.85) ? 0 : 1; }
	else if (v < 0.955){ R.bNoBall = true; }                                      // no-ball (penalty)
	else // wicket (~4.5%)
	{
		const int32 Kind = (int32)(Hash01(Seed, DeliveryCounter, 53) * 5.0) % 5;
		switch (Kind)
		{
		case 0: R.bHitStumps = true; break;                 // bowled
		case 1: R.bStruck = true; R.bCaught = true; break;  // caught
		case 2: R.bLbw = true; break;                       // lbw
		case 3: R.bStruck = true; R.bRunOut = true; R.bRunOutStriker = (Hash01(Seed, DeliveryCounter, 71) < 0.5); R.RunsRun = 1; break;
		default: R.bStumped = true; break;                  // stumped
		}
	}
	return R;
}

void ACricketMatchRunner::StepBall()
{
	if (!Engine) { return; }

	switch (Engine->GetMatchState())
	{
	case ECricketMatchState::InningsBreak:
		Engine->StartSecondInnings();
		return;
	case ECricketMatchState::FirstInnings:
	case ECricketMatchState::SecondInnings:
		EnsureBowler();
		if (Engine->NeedsBowler()) { return; }
		Engine->ApplyDelivery(FCricketOutcomeInterpreter::Interpret(GenerateResult()));
		++DeliveryCounter;
		return;
	default:
		return; // PreMatch / Toss / MatchComplete
	}
}

void ACricketMatchRunner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	PollInput();

	if (bAutoPlay && Engine && Engine->GetMatchState() != ECricketMatchState::MatchComplete)
	{
		BallTimer += DeltaSeconds;
		if (BallTimer >= BallInterval)
		{
			BallTimer = 0.f;
			StepBall();
		}
	}
	ReportResultIfComplete();
	DrawHUD();
}

void ACricketMatchRunner::PollInput()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) { return; }

	if (PC->WasInputKeyJustPressed(EKeys::SpaceBar)) { StepBall(); }
	if (PC->WasInputKeyJustPressed(EKeys::Enter))    { bAutoPlay = !bAutoPlay; }
	if (PC->WasInputKeyJustPressed(EKeys::Up))       { BallInterval = FMath::Max(0.05f, BallInterval - 0.1f); }
	if (PC->WasInputKeyJustPressed(EKeys::Down))     { BallInterval = FMath::Min(2.0f, BallInterval + 0.1f); }
	if (PC->WasInputKeyJustPressed(EKeys::R))        { SetupMatch(); }
}

void ACricketMatchRunner::DrawHUD() const
{
#if UE_BUILD_SHIPPING
	return; // Placeholder on-screen scoreboard for the dev runner — the shipping HUD is CricketUI. Compiled out of Shipping.
#else
	if (!GEngine || !Engine) { return; }
	auto Line = [&](int32 Key, const FColor& C, const FString& T) { GEngine->AddOnScreenDebugMessage(Key, 0.f, C, T); };

	const UEnum* StateEnum = StaticEnum<ECricketMatchState>();
	const FString StateStr = StateEnum ? StateEnum->GetDisplayNameTextByValue((int64)Engine->GetMatchState()).ToString() : TEXT("?");

	Line(6000, FColor::White, FString::Printf(TEXT("=== INDIA vs AUSTRALIA — T20 (%d overs) ===   [%s]"), FMath::Max(1, OversPerInnings), *StateStr));
	Line(6001, FColor::Silver, FString::Printf(TEXT("Space=Ball  Enter=Auto(%s)  Up/Dn=Speed(%.2fs)  R=Restart"),
		bAutoPlay ? TEXT("on") : TEXT("off"), BallInterval));

	if (Engine->GetMatchState() == ECricketMatchState::MatchComplete)
	{
		Line(6002, FColor::Yellow, FString::Printf(TEXT(">>> %s <<<"), *Engine->GetResult().Summary));
	}

	auto DrawInnings = [&](int32 Idx, int32 KeyBase, bool bActive)
	{
		const FCricketInningsScorecard& C = Engine->GetInnings(Idx);
		if (C.BattingTeam.IsEmpty()) { return; }
		const FColor Col = bActive ? FColor::Green : FColor(170, 170, 170);
		Line(KeyBase, Col, FString::Printf(TEXT("%s  %d/%d  (%d.%d ov)   RR %.2f   Extras %d"),
			*C.BattingTeam, C.Totals.Runs, C.Totals.Wickets, C.CompletedOvers(), C.BallsThisOver(),
			C.RunRate(), C.Totals.Extras));
		if (bActive && Engine->IsLive())
		{
			Line(KeyBase + 1, Col, FString::Printf(TEXT("  %s*  %d (%d)    %s  %d (%d)"),
				*Engine->GetStrikerName(),
				C.Batters.IsValidIndex(C.StrikerIndex) ? C.Batters[C.StrikerIndex].Runs : 0,
				C.Batters.IsValidIndex(C.StrikerIndex) ? C.Batters[C.StrikerIndex].Balls : 0,
				*Engine->GetNonStrikerName(),
				C.Batters.IsValidIndex(C.NonStrikerIndex) ? C.Batters[C.NonStrikerIndex].Runs : 0,
				C.Batters.IsValidIndex(C.NonStrikerIndex) ? C.Batters[C.NonStrikerIndex].Balls : 0));
			if (C.Bowlers.IsValidIndex(C.CurrentBowler))
			{
				const FCricketBowlerStats& B = C.Bowlers[C.CurrentBowler];
				Line(KeyBase + 2, Col, FString::Printf(TEXT("  Bowling: %s  %d.%d-%d-%d-%d  (econ %.2f)"),
					*B.Name, B.CompletedOvers(), B.BallsInPartOver(), B.Maidens, B.RunsConceded, B.Wickets, B.Economy()));
			}
		}
	};

	DrawInnings(0, 6010, Engine->GetActiveInningsIndex() == 0 && Engine->IsLive());
	if (Engine->GetActiveInningsIndex() == 1 || Engine->GetMatchState() == ECricketMatchState::MatchComplete)
	{
		DrawInnings(1, 6020, Engine->GetActiveInningsIndex() == 1 && Engine->IsLive());
	}

	if (Engine->GetMatchState() == ECricketMatchState::SecondInnings)
	{
		Line(6030, FColor::Cyan, FString::Printf(TEXT("Target %d  |  Need %d off %d  |  RRR %.2f"),
			Engine->GetTarget(), Engine->RunsRequired(), Engine->BallsRemaining(), Engine->RequiredRunRate()));
	}
	else if (Engine->GetMatchState() == ECricketMatchState::InningsBreak)
	{
		Line(6030, FColor::Cyan, FString::Printf(TEXT("Innings break — Target %d. Press Space to start the chase."), Engine->GetTarget()));
	}
#endif
}
