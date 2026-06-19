#include "CricketContestModel.h"

namespace
{
	// Unique names: anonymous-namespace helpers must not collide with other CricketAI
	// .cpp files when the unity build buckets them into one translation unit.
	bool ContestStumpsLine(ECricketLine L) { return L == ECricketLine::OffStump || L == ECricketLine::Middle || L == ECricketLine::LegStump; }
	bool ContestFullLength(ECricketLength L) { return L == ECricketLength::Full || L == ECricketLength::Yorker || L == ECricketLength::FullToss; }
	bool ContestShortLength(ECricketLength L) { return L == ECricketLength::Short || L == ECricketLength::Bouncer; }

	/** Sample a small running-runs count for a non-boundary stroke. */
	int32 SampleRunningRuns(double Single, double Two, double Three, FRandomStream& Rng)
	{
		const double R = Rng.FRand();
		if (R < Single) { return 1; }
		if (R < Single + Two) { return 2; }
		if (R < Single + Two + Three) { return 3; }
		return 0;
	}
}

FCricketBallResult FCricketContestModel::Resolve(
	const FCricketBowlerDecision& Bowl,
	const FCricketBatterDecision& Bat,
	const FCricketAIPlayerProfile& Bowler,
	const FCricketAIPlayerProfile& Batter,
	const FCricketDeliveryRead& Ball,
	FRandomStream& Rng,
	const FCricketBalanceConfig& Balance)
{
	FCricketBallResult R;

	// Movement (lateral threat) is swing for pace, turn for spin — one dial each.
	const double MoveScale = Ball.IsSpin() ? Balance.SpinStrength : Balance.SwingStrength;

	const FCricketBattingTendencies& Bt = Batter.Batting;
	const FCricketBowlingTendencies& W = Bowler.Bowling;
	const double Competence = Ball.IsSpin() ? Bt.VsSpin : Bt.VsPace;
	const double Scatter    = Bowl.ExecutionScatter;

	// ---------------------------------------------------------------------
	// 1. Legality — wides and no-balls from execution scatter / wide intent.
	// ---------------------------------------------------------------------
	const double WideProb = 0.012 + Scatter * 0.09 + (Bowl.Intent.Line == ECricketLine::WideOutsideOff ? 0.05 : 0.0)
	                      + (Bowl.Intent.Line == ECricketLine::DownLeg ? 0.04 : 0.0);
	if (Rng.FRand() < WideProb)
	{
		R.bWide = true;
		// Occasionally byes are run on the wide.
		R.RunsRun = (Rng.FRand() < 0.10) ? 1 : 0;
		return R;
	}
	const double NoBallProb = 0.006 + Scatter * 0.03;
	const bool bNoBall = Rng.FRand() < NoBallProb;
	if (bNoBall) { R.bNoBall = true; } // play continues; runs off bat still count

	// ---------------------------------------------------------------------
	// 2. The leave — offering no shot. A misjudged leave at the stumps is fatal.
	// ---------------------------------------------------------------------
	if (Bat.bOffersNoShot)
	{
		R.bStruck = false;
		if (ContestStumpsLine(Bowl.Intent.Line) && (ContestFullLength(Bowl.Intent.Length) || Bowl.Intent.Length == ECricketLength::GoodLength) && !bNoBall)
		{
			const double pPunish = Bat.Risk * FMath::Lerp(1.1, 0.5, Competence) * Balance.GlobalWicketScale;
			if (Rng.FRand() < pPunish)
			{
				// Full -> usually bowled/lbw; good length -> lbw or bowled.
				if (Rng.FRand() < 0.5) { R.bHitStumps = true; } else { R.bLbw = true; }
			}
		}
		return R; // a clean leave is a dot
	}

	R.bStruck = true;

	// ---------------------------------------------------------------------
	// 3. Wicket probability — built from the intent, the matchup and the field.
	// ---------------------------------------------------------------------
	double pWicket = 0.0;
	switch (Bat.Action)
	{
	case ECricketBatterAction::Defend:      pWicket = 0.005; break;
	case ECricketBatterAction::Rotate:      pWicket = 0.011; break;
	case ECricketBatterAction::Attack:      pWicket = 0.021; break;
	case ECricketBatterAction::BoundaryHit: pWicket = 0.041; break;
	default:                                pWicket = 0.011; break;
	}

	// Skill: a competent batter survives; the risk the batter accepted raises it.
	pWicket *= FMath::Lerp(1.25, 0.5, Competence);
	pWicket *= FMath::Lerp(0.8, 1.0 + 0.4 * Balance.BatterRiskTaking, Bat.Risk);

	// Matchup traps: attacking your weakness is how you get out.
	if (ContestShortLength(Bowl.Intent.Length) && Bat.Input.ShotType == ECricketShotType::PullShot)
		pWicket += Bt.ShortBallWeakness * 0.06 * Balance.BounceVariation; // top-edged pull off a lively deck
	if ((Bowl.Intent.Line == ECricketLine::OutsideOff || Bowl.Intent.Line == ECricketLine::WideOutsideOff)
		&& Bat.Action >= ECricketBatterAction::Attack)
		pWicket += Bt.OutsideOffTemptation * 0.05;                    // nick off
	if (ContestFullLength(Bowl.Intent.Length) && ContestStumpsLine(Bowl.Intent.Line))
		pWicket += Bt.FullBallWeakness * 0.035;                       // bowled/lbw driving

	// Bowling quality: movement and the wicket ball threaten more; a loose (scattered)
	// ball threatens less even as it leaks runs.
	pWicket += (Bowl.Intent.Movement != ECricketMovement::SeamUp ? W.Movement * 0.03 * MoveScale : 0.0);
	pWicket += Bowl.WicketIntent * W.Aggression * 0.04;
	pWicket *= FMath::Lerp(1.15, 0.8, Scatter);

	// Close catchers convert the aerial/edged chance.
	if (Bat.Action >= ECricketBatterAction::Attack)
		pWicket += Ball.CatchersUp * 0.03;

	// Master wicket-rate dial — the headline tuning knob for the validation batch.
	pWicket = FMath::Clamp(pWicket * Balance.GlobalWicketScale, 0.0, 0.6);

	const bool bOut = (!bNoBall) && (Rng.FRand() < pWicket); // can't be bowled off a no-ball

	if (bOut)
	{
		// Pick a dismissal mode consistent with the shot and the ball.
		const bool bAerial = (Bat.Action == ECricketBatterAction::BoundaryHit) || (Bat.Action == ECricketBatterAction::Attack && Rng.FRand() < 0.5);
		if (ContestShortLength(Bowl.Intent.Length) && Bat.Input.ShotType == ECricketShotType::PullShot)
		{
			R.bCaught = true;                                         // top edge
		}
		else if (ContestFullLength(Bowl.Intent.Length) && ContestStumpsLine(Bowl.Intent.Line) && !bAerial)
		{
			if (Rng.FRand() < 0.55) { R.bHitStumps = true; } else { R.bLbw = true; }
		}
		else if (Ball.IsSpin() && Bat.Input.Footwork == ECricketFootwork::FrontFoot && Rng.FRand() < FMath::Clamp(0.18 * Balance.SpinStrength, 0.0, 0.6))
		{
			R.bStumped = true;                                        // beaten down the track
		}
		else
		{
			R.bCaught = true;                                         // edged or holed out
		}
		return R; // dismissed — no runs off the bat (run-outs handled below survive path)
	}

	// ---------------------------------------------------------------------
	// 4. Survived — distribute runs by intent, power, contact quality and field.
	// ---------------------------------------------------------------------
	// Mistiming: even surviving, an attacking shot can be a dot if poorly struck. The
	// bat-timing-window dial recentres clean contact (a wider window middles more).
	const double TimingOffset = (Balance.BatTimingWindow - 1.0) * 0.2;
	const double CleanContact = FMath::Clamp(Competence * 0.6 + (1.0 - Bat.Risk) * 0.4 + TimingOffset + Rng.FRandRange(-0.15, 0.15), 0.0, 1.0);
	const double FieldOpen = 1.0 - Ball.BoundaryProtection;

	switch (Bat.Action)
	{
	case ECricketBatterAction::Defend:
		R.RunsRun = (Rng.FRand() < 0.15) ? 1 : 0;                     // mostly a dead-bat dot; the odd nudged single
		break;

	case ECricketBatterAction::Rotate:
		// Working a single is the intent; a good ball still beats the push, so a real share
		// are dots, but rotation also turns plenty into twos. (Re-tuned: the run rate must
		// be carried by boundaries and twos, not by near-free singles, which let the optimal
		// AI milk strike and crushed the dot rate.)
		R.RunsRun = SampleRunningRuns(0.34 + 0.30 * Bt.StrikeRotation, 0.18, 0.02, Rng);
		break;

	case ECricketBatterAction::Attack:
	{
		// Positive stroke: mostly four, the odd lofted six, otherwise worked hard for runs.
		const double pSix  = FMath::Clamp(0.14 * (0.45 + Bt.Power) * FieldOpen * CleanContact * Balance.BoundaryScale, 0.0, 0.32);
		const double pFour = FMath::Clamp((0.82 * (0.45 + Bt.Power) * FieldOpen * CleanContact + 0.15) * Balance.BoundaryScale, 0.0, 0.74);
		const double Roll = Rng.FRand();
		if (Roll < pSix)              { R.bBoundarySix = true; }
		else if (Roll < pSix + pFour) { R.bBoundaryFour = true; }
		else { R.RunsRun = SampleRunningRuns(0.42, 0.26, 0.03, Rng); } // ones and twos, sometimes a dot
		break;
	}

	case ECricketBatterAction::BoundaryHit:
	{
		// Going big: a high six share by intent, plenty of fours, but a real miss rate.
		const double pSix  = FMath::Clamp(1.10 * (0.55 + Bt.Power) * FieldOpen * CleanContact * Balance.BoundaryScale, 0.0, 0.64);
		const double pFour = FMath::Clamp((0.44 * FieldOpen * CleanContact + 0.08) * Balance.BoundaryScale, 0.0, 0.52);
		const double Roll = Rng.FRand();
		if (Roll < pSix)             { R.bBoundarySix = true; }
		else if (Roll < pSix + pFour){ R.bBoundaryFour = true; }
		else { R.RunsRun = SampleRunningRuns(0.30, 0.22, 0.03, Rng); } // mishit, scrambled
		break;
	}
	default: break;
	}

	// A small chance the running between the wickets brings a run-out.
	if (R.RunsRun >= 1 && !R.bBoundaryFour && !R.bBoundarySix)
	{
		const double pRunOut = 0.012 + (R.RunsRun >= 2 ? 0.02 : 0.0) + (1.0 - Batter.FieldingRating) * 0.0;
		if (Rng.FRand() < pRunOut)
		{
			R.bRunOut = true;
			R.bRunOutStriker = (Rng.FRand() < 0.5);
			R.RunsRun = FMath::Max(0, R.RunsRun - 1);                  // the completed runs before the out
		}
	}

	return R;
}
