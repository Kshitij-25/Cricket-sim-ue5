#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CricketMatchTypes.h"
#include "CricketScoringTypes.h"
#include "CricketMatchEngine.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketMatchStateChanged, ECricketMatchState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketBallApplied, FCricketDeliveryOutcome, Outcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCricketOverComplete);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketWicket, ECricketDismissal, How);

/**
 * UCricketMatchEngine — the T20 Match Engine: the cricket-rules layer above the
 * physics sandbox. It consumes interpreted ball outcomes (FCricketDeliveryOutcome)
 * and applies the laws; it NEVER alters a physics result, only scores it.
 *
 * The brief's seven architectural pieces all live here as clearly-delineated
 * responsibilities (one engine keeps the rules consistent and atomic per ball):
 *   1. Match Engine        — this class: orchestration + public API.
 *   2. Score Engine        — Score_Apply* (runs, extras, boundaries -> totals/stats).
 *   3. Innings Manager      — Innings_Begin/Close, target, end conditions.
 *   4. Over Manager         — Over_Advance (ball/over counting, strike rotation, bowlers).
 *   5. Wicket Manager       — Wicket_Apply (dismissal, new batter, all-out).
 *   6. Statistics System    — the FCricketBatter/BowlerStats kept live in the cards.
 *   7. Match State Machine  — SetMatchState + the transition methods below.
 *
 * Deterministic and free of UWorld, so it runs headlessly in automation tests and
 * is driven in-game by ACricketMatchRunner.
 */
UCLASS(BlueprintType)
class CRICKETSIM_API UCricketMatchEngine : public UObject
{
	GENERATED_BODY()

public:
	// --- Setup & state machine transitions ----------------------------------

	/** Configure the format and the two squads. Resets to PreMatch. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Match")
	void ConfigureMatch(const FCricketMatchRules& InRules, const FCricketSquad& TeamA, const FCricketSquad& TeamB);

	/** PreMatch -> Toss. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Match")
	void StartMatch();

	/**
	 * Resolve the toss and start the first innings. TossWinner is 0 (Team A) or 1
	 * (Team B); bWinnerBatsFirst chooses to bat or bowl. Toss -> FirstInnings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Match")
	void PerformToss(int32 TossWinner, bool bWinnerBatsFirst);

	/** InningsBreak -> SecondInnings (sets up the chase). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Match")
	void StartSecondInnings();

	// --- The over: who is bowling -------------------------------------------

	/**
	 * Nominate the bowler for the next over (call at innings start and each over).
	 * Enforces no consecutive overs and the per-bowler over cap; returns false (and
	 * keeps the previous bowler) if the choice is illegal.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Match")
	bool SetBowler(const FString& BowlerName);

	/** True if BowlerName may legally bowl the next over right now. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Match")
	bool CanBowl(const FString& BowlerName) const;

	// --- The ball -----------------------------------------------------------

	/**
	 * Apply one delivery's interpreted outcome. This is the heart: it scores the
	 * ball, updates stats, rotates the strike, counts the over, applies any wicket,
	 * and ends the innings/match when the laws say so. Returns false if no ball can
	 * be bowled right now (wrong state / innings closed).
	 */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Match")
	bool ApplyDelivery(const FCricketDeliveryOutcome& Outcome);

	// --- Read-back (match / score / stats) ----------------------------------

	UFUNCTION(BlueprintPure, Category = "Cricket|Match") ECricketMatchState GetMatchState() const { return MatchState; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") bool IsLive() const { return MatchState == ECricketMatchState::FirstInnings || MatchState == ECricketMatchState::SecondInnings; }

	/** True when a new over needs a bowler nominated before the next ball. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") bool NeedsBowler() const { return IsLive() && GetActiveInnings().CurrentBowler == INDEX_NONE && !GetActiveInnings().bClosed; }

	/** The innings currently being played (0 or 1); INDEX_NONE if none is live. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") int32 GetActiveInningsIndex() const { return ActiveInnings; }
	const FCricketInningsScorecard& GetInnings(int32 Index) const;
	const FCricketInningsScorecard& GetActiveInnings() const;

	/** Runs the chasing side needs (2nd innings); 0 in the first innings. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") int32 GetTarget() const { return Target; }

	/** Required run rate for the chase (run/over); 0 if not chasing. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") double RequiredRunRate() const;

	/** Runs still needed to win (2nd innings). */
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") int32 RunsRequired() const;

	/** Balls remaining in the current innings. */
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") int32 BallsRemaining() const;

	UFUNCTION(BlueprintPure, Category = "Cricket|Match") const FCricketMatchResult& GetResult() const { return Result; }
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") const FCricketMatchRules& GetRules() const { return Rules; }

	/** Convenience: current striker / non-striker / bowler names ("" if none). */
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") FString GetStrikerName() const;
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") FString GetNonStrikerName() const;
	UFUNCTION(BlueprintPure, Category = "Cricket|Match") FString GetBowlerName() const;

	// --- Events -------------------------------------------------------------
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Match") FOnCricketMatchStateChanged OnMatchStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Match") FOnCricketBallApplied OnBallApplied;
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Match") FOnCricketOverComplete OnOverComplete;
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Match") FOnCricketWicket OnWicket;

private:
	// --- State machine ---
	void SetMatchState(ECricketMatchState NewState);

	// --- Innings Manager ---
	void Innings_Begin(int32 InningsIndex, const FCricketSquad& BattingSquad, const FCricketSquad& BowlingSquad);
	bool Innings_CheckEnd();   // returns true if the active innings just ended
	void Innings_Close();

	// --- Score Engine ---  (mutates totals + striker/bowler stats; no rules flow)
	void Score_Apply(const FCricketDeliveryOutcome& O, int32& OutBowlerRuns, int32& OutRanRuns, bool bLegal);

	// --- Over Manager ---
	void Over_Advance(bool bLegal, bool bWicketFell);  // counts the ball, rotates strike, closes the over

	// --- Wicket Manager ---
	void Wicket_Apply(const FCricketDeliveryOutcome& O);

	// --- Helpers ---
	FCricketInningsScorecard& Active();
	FCricketBowlerStats& CurrentBowlerStats();
	int32 FindOrAddBowler(const FString& Name);
	void SwapStrike();
	int32 PlayersPerTeam() const { return FMath::Max(Rules.PlayersPerTeam, 2); }

	// --- Members ---
	UPROPERTY() FCricketMatchRules Rules;
	UPROPERTY() FCricketSquad Squads[2];
	UPROPERTY() FCricketInningsScorecard Innings[2];

	ECricketMatchState MatchState = ECricketMatchState::PreMatch;
	int32 ActiveInnings = INDEX_NONE;
	int32 BattingFirst = 0;        // which squad (0/1) bats first
	int32 Target = 0;
	FCricketMatchResult Result;

	FString PrevOverBowlerName;    // to forbid consecutive overs
	bool bAwaitingBowler = false;  // an over just ended; a new bowler must be set
};
