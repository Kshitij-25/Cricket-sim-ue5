#include "CricketPresentationSubsystem.h"

#include "CricketEventClassifier.h"
#include "CricketMatchFlowModel.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

#include "CricketMatchRunner.h"
#include "CricketMatchEngine.h"
#include "CricketScoringTypes.h"            // FCricketDeliveryOutcome
#include "CricketReplayComponent.h"
#include "CricketCameraDirectorComponent.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketAudioSubsystem.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPresentationDebug(TEXT("cricket.Presentation.Debug"), 0,
		TEXT("Presentation debug overlay: state, active camera, active event, replay triggers, crowd atmosphere, score graphics. 0=off, 1=on"));

	constexpr int32 MaxDebugRows = 10;

	float ReactionIntensity(ECricketPresentationSeverity Sev)
	{
		switch (Sev)
		{
		case ECricketPresentationSeverity::Defining: return 1.0f;
		case ECricketPresentationSeverity::High:     return 0.85f;
		case ECricketPresentationSeverity::Medium:   return 0.6f;
		default:                                      return 0.4f;
		}
	}
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UCricketPresentationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = ECricketPresentationState::Idle;
}

void UCricketPresentationSubsystem::Deinitialize()
{
	if (Replay.IsValid() && Replay->IsReplaying())
	{
		Replay->StopReplay();
		Replay->SetRate(1.0);
	}
	Super::Deinitialize();
}

bool UCricketPresentationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCricketPresentationSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCricketPresentationSubsystem, STATGROUP_Tickables);
}

void UCricketPresentationSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	NowSeconds += DeltaTime;
	if (!bEnabled) { return; }

	ResolveAndBindSources();

	// The crowd atmosphere arc breathes every frame regardless of state.
	Crowd.Tick(DeltaTime);

	switch (State)
	{
	case ECricketPresentationState::BroadcastSequence: UpdateSequence(DeltaTime); break;
	case ECricketPresentationState::Replay:            UpdateReplay(DeltaTime);   break;
	case ECricketPresentationState::EventBeat:         UpdateEventBeat(DeltaTime); break;
	case ECricketPresentationState::LivePlay:
	case ECricketPresentationState::Idle:
	default:                                            UpdateLiveCamera(DeltaTime); break;
	}

	if (CVarPresentationDebug.GetValueOnGameThread() != 0) { DrawDebug(); }
}

// ---------------------------------------------------------------------------
// Discovery + binding (mirrors the audio manager's passive listener)
// ---------------------------------------------------------------------------

void UCricketPresentationSubsystem::ResolveAndBindSources()
{
	UWorld* World = GetWorld();
	if (!World) { return; }

	if (!MatchEngine.IsValid()) { bBoundMatch = false; }

	if (!MatchEngine.IsValid())
	{
		for (TActorIterator<ACricketMatchRunner> It(World); It; ++It)
		{
			if (UCricketMatchEngine* E = It->GetEngine()) { MatchEngine = E; break; }
		}
	}
	if (!Replay.IsValid() || !CameraDirector.IsValid() || !BallPhysics.IsValid())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (!Replay.IsValid())         { if (auto* C = A->FindComponentByClass<UCricketReplayComponent>())          { Replay = C; } }
			if (!CameraDirector.IsValid()) { if (auto* C = A->FindComponentByClass<UCricketCameraDirectorComponent>())  { CameraDirector = C; } }
			if (!BallPhysics.IsValid())    { if (auto* C = A->FindComponentByClass<UCricketBallPhysicsComponent>())     { BallPhysics = C; } }
		}
	}
	if (!Audio.IsValid())
	{
		Audio = World->GetSubsystem<UCricketAudioSubsystem>();
	}

	if (MatchEngine.IsValid() && !bBoundMatch)
	{
		MatchEngine->OnMatchStateChanged.AddDynamic(this, &UCricketPresentationSubsystem::HandleMatchStateChanged);
		MatchEngine->OnBallApplied.AddDynamic(this, &UCricketPresentationSubsystem::HandleBallApplied);
		bBoundMatch = true;
		if (State == ECricketPresentationState::Idle) { State = ECricketPresentationState::LivePlay; }
	}
}

// ---------------------------------------------------------------------------
// Bound engine handlers
// ---------------------------------------------------------------------------

void UCricketPresentationSubsystem::HandleMatchStateChanged(ECricketMatchState NewState)
{
	if (!bEnabled || !MatchEngine.IsValid()) { return; }

	switch (NewState)
	{
	case ECricketMatchState::FirstInnings:
	{
		// First ball of the match coming up: open with the intro over the master shot.
		const FCricketMatchSnapshot S = FCricketMatchSnapshot::Capture(*MatchEngine);
		Score.ResetInnings();
		LastSnapshot = S;
		if (!S.BattingTeam.IsEmpty() && !S.BowlingTeam.IsEmpty())
		{
			BeginSequence(FCricketMatchFlowModel::BuildMatchIntro(S.BattingTeam, S.BowlingTeam, MatchEngine->GetRules().OversPerInnings));
		}
		break;
	}
	case ECricketMatchState::InningsBreak:
	{
		// The first innings just closed; present its total and the target to chase.
		const FCricketInningsScorecard& First = MatchEngine->GetInnings(0);
		const int32 Target = First.Totals.Runs + 1;
		BeginSequence(FCricketMatchFlowModel::BuildInningsTransition(
			First.BattingTeam, First.Totals.Runs, First.Totals.Wickets, Target));
		break;
	}
	case ECricketMatchState::SecondInnings:
	{
		Score.ResetInnings();
		LastSnapshot = FCricketMatchSnapshot::Capture(*MatchEngine);
		break;
	}
	case ECricketMatchState::MatchComplete:
	{
		const FCricketMatchSnapshot S = FCricketMatchSnapshot::Capture(*MatchEngine);
		BeginSequence(FCricketMatchFlowModel::BuildMatchResult(S));
		break;
	}
	default:
		break;
	}
}

void UCricketPresentationSubsystem::HandleBallApplied(FCricketDeliveryOutcome Outcome)
{
	if (!bEnabled || !MatchEngine.IsValid()) { return; }

	const FCricketMatchSnapshot After = FCricketMatchSnapshot::Capture(*MatchEngine);

	// Update the broadcast graphics + atmosphere context from the new scoreboard.
	const bool bOverDone = Score.OnBall(LastSnapshot, After);
	Crowd.UpdateContext(After);

	// Classify the ball into zero or more moments (wicket > boundary > milestone > result).
	const TArray<FCricketPresentationEvent> Events =
		FCricketEventClassifier::ClassifyDelivery(Outcome, LastSnapshot, After);

	for (int32 i = 0; i < Events.Num(); ++i)
	{
		if (i == 0) { ProcessHeadlineEvent(Events[i]); }
		else        { ProcessSecondaryEvent(Events[i]); }
	}

	// An over just completed (and no bigger beat is mid-flight): roll the over bumper.
	if (bOverDone)
	{
		BeginSequence(FCricketMatchFlowModel::BuildOverTransition(
			Score.LastOverSummary, FCricketScorePresentationModel::ChaseText(After)));
	}

	// This ball's post-state is the next ball's "before".
	LastSnapshot = After;
}

// ---------------------------------------------------------------------------
// Event orchestration
// ---------------------------------------------------------------------------

void UCricketPresentationSubsystem::ProcessHeadlineEvent(const FCricketPresentationEvent& Event)
{
	ActiveEvent = Event;
	ActiveCaption = Event.Headline;

	Crowd.ApplyEvent(Event);
	OnPresentationEvent.Broadcast(Event);
	EmitReaction(Event);

	const bool bWantsReplay = bAutoReplays && Replay.IsValid() && !Replay->IsReplaying()
		&& ReplayDir.ShouldAutoReplay(Event);
	RecordDebug(Event, bWantsReplay);

	// Don't hijack a running broadcast sequence (e.g. the closing result package).
	if (State == ECricketPresentationState::BroadcastSequence) { return; }

	// Cut to the moment's best angle and hold the live beat; the replay (if any) is
	// armed and fires once the beat elapses, letting the real ball finish first.
	ForceCamera(FCricketBroadcastDirector::SelectCameraForEvent(Event));
	ActiveReplayPlan = bWantsReplay ? ReplayDir.BuildPlan(Event) : FCricketReplayPlan{};
	BeatElapsed = 0.0f;
	State = ECricketPresentationState::EventBeat;
}

void UCricketPresentationSubsystem::ProcessSecondaryEvent(const FCricketPresentationEvent& Event)
{
	// Secondary moments (e.g. a milestone that shares the ball with a boundary, or the
	// match-result beat behind a sealing six) still drive crowd/graphics/reactions, but
	// never fight the headline beat for the camera or the replay.
	Crowd.ApplyEvent(Event);
	OnPresentationEvent.Broadcast(Event);
	EmitReaction(Event);
	RecordDebug(Event, false);
}

void UCricketPresentationSubsystem::EmitReaction(const FCricketPresentationEvent& Event)
{
	const float Intensity = ReactionIntensity(Event.Severity);
	auto Fire = [this](ECricketReactionType Type, const FString& Player, float In)
	{
		if (Type == ECricketReactionType::None || Player.IsEmpty()) { return; }
		FCricketReactionIntent Intent;
		Intent.Type = Type;
		Intent.Player = Player;
		Intent.Intensity = In;
		OnReactionRequested.Broadcast(Intent);
	};

	switch (Event.Type)
	{
	case ECricketPresentationEventType::Wicket:
		Fire(ECricketReactionType::WicketCelebration, Event.SecondaryPlayer, Intensity); // the bowler/fielders
		Fire(ECricketReactionType::Frustration, Event.PrimaryPlayer, Intensity);          // the dismissed batter
		break;
	case ECricketPresentationEventType::Six:
	case ECricketPresentationEventType::Boundary:
		Fire(ECricketReactionType::BoundaryCelebration, Event.PrimaryPlayer, Intensity);
		break;
	case ECricketPresentationEventType::Milestone:
		Fire(ECricketReactionType::MilestoneCelebration, Event.PrimaryPlayer, Intensity);
		break;
	case ECricketPresentationEventType::MatchResult:
		Fire(ECricketReactionType::VictoryCelebration, Event.PrimaryPlayer, 1.0f);
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Per-tick state machine stages
// ---------------------------------------------------------------------------

void UCricketPresentationSubsystem::UpdateLiveCamera(float DeltaSeconds)
{
	ActiveCaption.Reset();
	const bool bInFlight = BallInFlight();
	const ECricketBroadcastCamera Cam = Broadcast.SelectLiveCamera(/*bWaitingToBowl*/ !bInFlight, bInFlight, /*bChasingRope*/ false, DeltaSeconds);
	if (!bHasAppliedCamera || Cam != AppliedCamera)
	{
		DriveCameraMode(Cam);
	}
}

void UCricketPresentationSubsystem::UpdateEventBeat(float DeltaSeconds)
{
	BeatElapsed += DeltaSeconds;
	if (BeatElapsed < EventBeatSeconds) { return; }

	if (ActiveReplayPlan.bShouldReplay && Replay.IsValid())
	{
		BeginReplay(ActiveReplayPlan);
	}
	else
	{
		FinishBeatOrReplay();
	}
}

void UCricketPresentationSubsystem::UpdateReplay(float DeltaSeconds)
{
	ReplayAngleElapsed += DeltaSeconds;
	if (ReplayAngleElapsed < ActiveReplayPlan.SecondsPerAngle) { return; }

	ReplayAngleElapsed = 0.0f;
	++ReplayAngleIndex;
	if (ActiveReplayPlan.Angles.IsValidIndex(ReplayAngleIndex))
	{
		ForceCamera(ActiveReplayPlan.Angles[ReplayAngleIndex], 0.35f);
		// Restart the recorded clip for the next angle so each pass shows the moment whole.
		if (Replay.IsValid()) { Replay->StartReplay(); Replay->SetRate(ActiveReplayPlan.SlowMoRate); }
	}
	else
	{
		EndReplay();
	}
}

void UCricketPresentationSubsystem::UpdateSequence(float DeltaSeconds)
{
	const bool bStillRunning = ActiveSequence.Advance(DeltaSeconds);
	if (const FCricketBroadcastStep* Step = ActiveSequence.ActiveStep())
	{
		ActiveCaption = Step->Caption;
		if (!bHasAppliedCamera || Step->Camera != AppliedCamera)
		{
			ForceCamera(Step->Camera);
		}
	}
	if (!bStillRunning)
	{
		ActiveCaption.Reset();
		ActiveSequence.bActive = false;
		State = ECricketPresentationState::LivePlay;
		if (bHasPendingSequence) { bHasPendingSequence = false; BeginSequence(PendingSequence); }
	}
}

// ---------------------------------------------------------------------------
// Orchestration helpers
// ---------------------------------------------------------------------------

void UCricketPresentationSubsystem::BeginSequence(const FCricketBroadcastSequence& Seq)
{
	if (!Seq.IsValid()) { return; }

	// A live event beat / replay owns the screen; queue the sequence to drain after it.
	if (State == ECricketPresentationState::EventBeat || State == ECricketPresentationState::Replay)
	{
		PendingSequence = Seq;
		bHasPendingSequence = true;
		return;
	}

	ActiveSequence = Seq;
	ActiveSequence.Begin();
	State = ECricketPresentationState::BroadcastSequence;
	if (const FCricketBroadcastStep* Step = ActiveSequence.ActiveStep())
	{
		ActiveCaption = Step->Caption;
		ForceCamera(Step->Camera);
	}
}

void UCricketPresentationSubsystem::BeginReplay(const FCricketReplayPlan& Plan)
{
	if (!Plan.bShouldReplay || Plan.Angles.Num() == 0 || !Replay.IsValid())
	{
		FinishBeatOrReplay();
		return;
	}

	State = ECricketPresentationState::Replay;
	ReplayAngleIndex = 0;
	ReplayAngleElapsed = 0.0f;
	ActiveReplayPlan = Plan;
	ActiveCaption = TEXT("REPLAY");

	Replay->StartReplay();
	Replay->SetRate(Plan.SlowMoRate);
	ForceCamera(Plan.Angles[0], 0.35f);
}

void UCricketPresentationSubsystem::EndReplay()
{
	if (Replay.IsValid() && Replay->IsReplaying())
	{
		Replay->StopReplay();
		Replay->SetRate(1.0);
	}
	FinishBeatOrReplay();
}

void UCricketPresentationSubsystem::FinishBeatOrReplay()
{
	ActiveCaption.Reset();
	State = ECricketPresentationState::LivePlay;
	if (bHasPendingSequence)
	{
		bHasPendingSequence = false;
		BeginSequence(PendingSequence);
	}
}

void UCricketPresentationSubsystem::ForceCamera(ECricketBroadcastCamera Camera, float BlendSeconds)
{
	Broadcast.Force(Camera);
	DriveCameraMode(Camera, BlendSeconds);
}

void UCricketPresentationSubsystem::DriveCameraMode(ECricketBroadcastCamera Camera, float BlendSeconds)
{
	AppliedCamera = Camera;
	bHasAppliedCamera = true;
	if (bDriveCamera && CameraDirector.IsValid())
	{
		CameraDirector->SetMode(FCricketBroadcastDirector::CameraModeFor(Camera), BlendSeconds);
	}
}

void UCricketPresentationSubsystem::IngestEvent(const FCricketPresentationEvent& Event)
{
	if (!Event.IsValid()) { return; }
	ProcessHeadlineEvent(Event);
}

bool UCricketPresentationSubsystem::BallInFlight() const
{
	return BallPhysics.IsValid() && BallPhysics->IsBallInFlight();
}

// ---------------------------------------------------------------------------
// Debug overlay
// ---------------------------------------------------------------------------

void UCricketPresentationSubsystem::RecordDebug(const FCricketPresentationEvent& Event, bool bTriggeredReplay)
{
	FCricketPresentationDebugEntry Entry;
	Entry.Type = Event.Type;
	Entry.Severity = Event.Severity;
	Entry.Headline = Event.Headline;
	Entry.bTriggeredReplay = bTriggeredReplay;
	Entry.TimeSeconds = NowSeconds;
	RecentEvents.Insert(Entry, 0);
	if (RecentEvents.Num() > MaxDebugRows) { RecentEvents.SetNum(MaxDebugRows); }
}

void UCricketPresentationSubsystem::DrawDebug() const
{
	if (!GEngine) { return; }

	auto StateName = [](ECricketPresentationState S) -> const TCHAR*
	{
		switch (S)
		{
		case ECricketPresentationState::Idle:              return TEXT("Idle");
		case ECricketPresentationState::LivePlay:          return TEXT("Live Play");
		case ECricketPresentationState::EventBeat:         return TEXT("Event Beat");
		case ECricketPresentationState::Replay:            return TEXT("Replay");
		case ECricketPresentationState::BroadcastSequence: return TEXT("Broadcast Sequence");
		default:                                            return TEXT("?");
		}
	};

	const int32 Key = 0x10C; // a stable base key so the block redraws in place
	int32 K = Key;
	auto Line = [&](const FString& Text, const FColor& Color)
	{
		GEngine->AddOnScreenDebugMessage(K++, 0.0f, Color, Text);
	};

	Line(TEXT("== Cricket Presentation =="), FColor::Cyan);
	Line(FString::Printf(TEXT("State: %s   Camera: %s"),
		StateName(State), *FCricketBroadcastDirector::CameraName(Broadcast.Current)),
		bDriveCamera && CameraDirector.IsValid() ? FColor::White : FColor::Silver);
	Line(FString::Printf(TEXT("Active event: %s%s"),
		ActiveEvent.IsValid() ? *ActiveEvent.Headline : TEXT("(none)"),
		ActiveEvent.bMatchDefining ? TEXT("  [DEFINING]") : TEXT("")),
		FColor::Yellow);
	Line(FString::Printf(TEXT("Crowd: %.2f  (%s)"),
		Crowd.Atmosphere, *FCricketCrowdPresentationModel::MoodName(Crowd.Mood())), FColor::Orange);
	Line(FString::Printf(TEXT("%s   |   %s"),
		*Score.PartnershipText(),
		Score.bHasOverSummary ? *Score.LastOverSummary : TEXT("(over in progress)")), FColor::White);
	if (!ActiveCaption.IsEmpty())
	{
		Line(FString::Printf(TEXT("Caption: \"%s\""), *ActiveCaption), FColor::Green);
	}
	Line(FString::Printf(TEXT("Sources  engine:%s replay:%s camera:%s ball:%s audio:%s"),
		MatchEngine.IsValid() ? TEXT("Y") : TEXT("-"),
		Replay.IsValid() ? TEXT("Y") : TEXT("-"),
		CameraDirector.IsValid() ? TEXT("Y") : TEXT("-"),
		BallPhysics.IsValid() ? TEXT("Y") : TEXT("-"),
		Audio.IsValid() ? TEXT("Y") : TEXT("-")), FColor::Silver);

	Line(TEXT("-- recent moments --"), FColor::Cyan);
	for (const FCricketPresentationDebugEntry& E : RecentEvents)
	{
		Line(FString::Printf(TEXT("  %5.1fs  %s%s"),
			E.TimeSeconds, *E.Headline, E.bTriggeredReplay ? TEXT("  (replay)") : TEXT("")),
			FColor::White);
	}
}
