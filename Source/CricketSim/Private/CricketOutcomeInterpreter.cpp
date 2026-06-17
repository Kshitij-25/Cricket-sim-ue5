#include "CricketOutcomeInterpreter.h"

FCricketDeliveryOutcome FCricketOutcomeInterpreter::Interpret(const FCricketBallResult& R)
{
	// --- Illegal deliveries first: they are re-bowled, runs run are extras. ---
	if (R.bWide)
	{
		FCricketDeliveryOutcome O = FCricketDeliveryOutcome::Wide(R.RunsRun);
		if (R.bStumped) { O.Dismissal = ECricketDismissal::Stumped; }
		else if (R.bRunOut) { O.Dismissal = ECricketDismissal::RunOut; O.bDismissedStriker = R.bRunOutStriker; }
		return O;
	}
	if (R.bNoBall)
	{
		FCricketDeliveryOutcome O = FCricketDeliveryOutcome::NoBall();
		if (R.bBoundarySix) { O.RunsOffBat = 6; O.bBoundary = true; }
		else if (R.bBoundaryFour) { O.RunsOffBat = 4; O.bBoundary = true; }
		else if (R.bStruck) { O.RunsOffBat = R.RunsRun; }
		else { O.RanExtraRuns = R.RunsRun; O.ExtraType = R.bRunsAreLegByes ? ECricketExtraType::LegBye : ECricketExtraType::Bye; }
		// Only a run out is possible off a no-ball.
		if (R.bRunOut) { O.Dismissal = ECricketDismissal::RunOut; O.bDismissedStriker = R.bRunOutStriker; }
		return O;
	}

	// --- Legal deliveries: a dismissal takes precedence over runs. ---
	if (R.bHitStumps) { return FCricketDeliveryOutcome::Out(ECricketDismissal::Bowled); }
	if (R.bLbw)       { return FCricketDeliveryOutcome::Out(ECricketDismissal::LBW); }
	if (R.bCaught)    { return FCricketDeliveryOutcome::Out(ECricketDismissal::Caught); }
	if (R.bStumped)   { return FCricketDeliveryOutcome::Out(ECricketDismissal::Stumped); }
	if (R.bRunOut)
	{
		FCricketDeliveryOutcome O = R.bStruck ? FCricketDeliveryOutcome::Runs(R.RunsRun) : FCricketDeliveryOutcome();
		if (!R.bStruck && R.RunsRun > 0)
		{
			O.RanExtraRuns = R.RunsRun;
			O.ExtraType = R.bRunsAreLegByes ? ECricketExtraType::LegBye : ECricketExtraType::Bye;
		}
		O.Dismissal = ECricketDismissal::RunOut;
		O.bDismissedStriker = R.bRunOutStriker;
		return O;
	}

	// --- No dismissal: boundaries, then runs, then byes, then a dot. ---
	if (R.bBoundarySix)  { return FCricketDeliveryOutcome::Six(); }
	if (R.bBoundaryFour) { return FCricketDeliveryOutcome::Four(); }

	if (R.bStruck) { return FCricketDeliveryOutcome::Runs(R.RunsRun); }
	if (R.RunsRun > 0)
	{
		return R.bRunsAreLegByes ? FCricketDeliveryOutcome::LegBye(R.RunsRun) : FCricketDeliveryOutcome::Bye(R.RunsRun);
	}
	return FCricketDeliveryOutcome::Dot();
}
