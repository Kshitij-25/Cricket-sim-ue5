#pragma once

#include "CoreMinimal.h"
#include "CricketScoringTypes.h"
#include "CricketOutcomeInterpreter.generated.h"

/**
 * FCricketBallResult — the RAW physical facts of one delivery, as produced by the
 * simulation systems (bowling legality, bat-ball contact, where the ball ended up,
 * what the fielders did). This is the boundary the match engine sits behind: the
 * physics fills this in, the interpreter classifies it, the engine scores it. The
 * engine never reads physics directly and never changes any of these facts.
 */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketBallResult
{
	GENERATED_BODY()

	// --- From the bowling system ---
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bWide = false;   // illegal line/height
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bNoBall = false; // overstep/other no-ball

	// --- From the bat-ball collision / batting system ---
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bStruck = false; // bat made contact

	// --- Terminal physics/fielding events (mutually informative) ---
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bHitStumps = false;  // ball hit the stumps (bowled)
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bLbw = false;        // pad intercept, would have hit
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bCaught = false;     // fielder took it on the full
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bStumped = false;    // missed, keeper whipped the bails
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bRunOut = false;
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bRunOutStriker = true;

	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bBoundaryFour = false; // grounded to the rope
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bBoundarySix = false;  // cleared the rope on the full

	/** Runs completed by running (non-boundary). For a run out, the runs that counted. */
	UPROPERTY(BlueprintReadWrite, Category = "Ball") int32 RunsRun = 0;
	/** If the batter did NOT make contact, runs run are byes (or leg-byes off the pad). */
	UPROPERTY(BlueprintReadWrite, Category = "Ball") bool bRunsAreLegByes = false;
};

/**
 * FCricketOutcomeInterpreter — converts the physical facts of a ball into the
 * FCricketDeliveryOutcome the match engine consumes. This is the ONLY place
 * physics meets the laws; it interprets, it never alters. Pure and testable.
 */
class CRICKETSIM_API FCricketOutcomeInterpreter
{
public:
	static FCricketDeliveryOutcome Interpret(const FCricketBallResult& R);
};
