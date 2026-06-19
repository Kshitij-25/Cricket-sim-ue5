#include "CricketBatterBrain.h"
#include "CricketTacticalEvaluator.h"

namespace
{
	bool IsWideLine(ECricketLine L) { return L == ECricketLine::WideOutsideOff || L == ECricketLine::OutsideOff; }
	bool IsStumpsLine(ECricketLine L) { return L == ECricketLine::OffStump || L == ECricketLine::Middle || L == ECricketLine::LegStump; }
	bool IsLegSideLine(ECricketLine L) { return L == ECricketLine::LegStump || L == ECricketLine::DownLeg; }

	/** How easy this ball is to put away (0..1) before intent — pure geometry/length. */
	double Hittability(const FCricketDeliveryRead& B)
	{
		double H = 0.35;                                   // a good-length ball: hard to score
		if (FCricketBatterBrain::IsDrivableLength(B.Length)) { H = 0.85; }          // half-volley
		else if (B.Length == ECricketLength::Short)          { H = 0.70; }          // pull/cut on offer
		else if (B.Length == ECricketLength::Bouncer)        { H = 0.45; }          // hittable but risky
		else if (B.Length == ECricketLength::BackOfLength)   { H = 0.45; }
		// A very full straight ball (yorker) is hard to hit for four despite being full.
		if (B.Length == ECricketLength::Yorker) { H = 0.40; }
		return FMath::Clamp(H, 0.0, 1.0);
	}

	/** Choose a concrete stroke (shot/footwork/aim/power) for the chosen action. */
	void BuildStroke(ECricketBatterAction Action, const FCricketDeliveryRead& B,
		const FCricketAIPlayerProfile& Batter, FCricketBattingInput& In, double& OutRisk)
	{
		In.bRightHanded = true; // handedness hook; rosters are RH in the MVP

		const bool bBack = FCricketBatterBrain::IsBackFootLength(B.Length);
		const bool bDrive = FCricketBatterBrain::IsDrivableLength(B.Length);
		const double Competence = B.IsSpin() ? Batter.Batting.VsSpin : Batter.Batting.VsPace;

		// --- Footwork from length ---
		if (bDrive)            { In.Footwork = ECricketFootwork::FrontFoot; }
		else if (bBack)        { In.Footwork = ECricketFootwork::BackFoot; }
		else                   { In.Footwork = ECricketFootwork::Neutral; }

		auto SetAim = [&In](double Deg) { In.AimYawDeg = FMath::Clamp(Deg, -45.0, 45.0); };

		switch (Action)
		{
		case ECricketBatterAction::Leave:
			In.ShotType = ECricketShotType::DefensiveBlock; In.Footwork = ECricketFootwork::Neutral;
			In.PowerScale = 0.0; SetAim(0.0);
			// Leaving a straight ball is a misjudgement waiting to happen.
			OutRisk = IsStumpsLine(B.Line) ? 0.55 : 0.05;
			return;

		case ECricketBatterAction::Defend:
			In.ShotType = ECricketShotType::DefensiveBlock;
			In.PowerScale = 0.30; SetAim(0.0);
			OutRisk = 0.10 + (B.Movement != ECricketMovement::SeamUp ? 0.10 : 0.0) + (1.0 - Competence) * 0.10;
			return;

		default: break; // Rotate / Attack / BoundaryHit fall through to shot selection
		}

		// --- Attacking/working strokes: pick the shot from line & length ---
		if (B.Length == ECricketLength::Short || B.Length == ECricketLength::Bouncer)
		{
			In.ShotType = ECricketShotType::PullShot; In.Footwork = ECricketFootwork::BackFoot;
			SetAim(-28.0);
		}
		else if (IsWideLine(B.Line))
		{
			In.ShotType = ECricketShotType::CoverDrive;
			SetAim(+26.0);
		}
		else if (IsLegSideLine(B.Line))
		{
			In.ShotType = ECricketShotType::StraightDrive; // worked through the on-side
			SetAim(-18.0);
		}
		else
		{
			In.ShotType = ECricketShotType::StraightDrive;
			SetAim(B.Line == ECricketLine::OffStump ? +8.0 : 0.0);
		}

		// --- Power & risk by intent ---
		switch (Action)
		{
		case ECricketBatterAction::Rotate:
			In.PowerScale = 0.55; SetAim(In.AimYawDeg * 0.6);
			OutRisk = 0.15 + (1.0 - Competence) * 0.15;
			break;
		case ECricketBatterAction::Attack:
			In.PowerScale = 1.0;
			OutRisk = 0.30 + (1.0 - Competence) * 0.30 + B.CatchersUp * 0.15
			        + (bDrive ? 0.0 : 0.10);                       // attacking a non-half-volley is riskier
			break;
		case ECricketBatterAction::BoundaryHit:
			In.PowerScale = 1.35;                                  // over the top
			OutRisk = 0.48 + (1.0 - Competence) * 0.30 + B.BoundaryProtection * 0.10
			        + (B.Length == ECricketLength::Bouncer ? 0.12 : 0.0);
			break;
		default: break;
		}
		OutRisk = FMath::Clamp(OutRisk, 0.0, 0.97);
	}
}

FCricketBatterDecision FCricketBatterBrain::Decide(
	const FCricketMatchSituation& S,
	const FCricketAIPlayerProfile& Batter,
	const FCricketDeliveryRead& B,
	const FCricketTeamStrategy& Strategy,
	const FCricketAIDifficultyParams& D,
	FRandomStream& Rng)
{
	FCricketBatterDecision Out;
	FCricketDecisionTrace& T = Out.Trace;
	T.Domain = TEXT("Batter");
	T.Pressure = S.Pressure;

	const FCricketBattingTendencies& Bt = Batter.Batting;
	const ECricketInningsApproach Approach = FCricketTacticalEvaluator::PhaseApproach(
		S, Strategy.PowerplayApproach, Strategy.MiddleApproach, Strategy.DeathApproach);
	const double A = FCricketTacticalEvaluator::BattingAggressionTarget(S, Approach, D.SituationalAwareness)
	               * FMath::Lerp(0.7, 1.15, Bt.Aggression);          // personality scales the target
	const double H = Hittability(B);
	const double Competence = B.IsSpin() ? Bt.VsSpin : Bt.VsPace;

	// --- Score the five intents from cricket logic ---
	// Leave: only the wide, non-threatening ball, and only when you don't need runs.
	double LeaveScore = -0.6;
	if (B.Line == ECricketLine::WideOutsideOff && !IsDrivableLength(B.Length))
		LeaveScore = 0.55 + Bt.Technique * 0.2;
	else if (B.Line == ECricketLine::OutsideOff && !IsDrivableLength(B.Length))
		LeaveScore = 0.20 + Bt.Technique * 0.15;
	LeaveScore -= A * 0.8;                                            // chasing runs? don't leave

	// Defend: the safe answer to a threatening ball; favoured at low aggression.
	const double Threat = (B.Length == ECricketLength::GoodLength && IsStumpsLine(B.Line) ? 0.30 : 0.0)
	                    + (B.Movement != ECricketMovement::SeamUp ? 0.18 : 0.0)
	                    + (1.0 - Competence) * 0.20;
	double DefendScore = (0.5 + Threat + Bt.Technique * 0.15) * (1.0 - 0.7 * A);

	// Rotate: the staple; peaks at moderate aggression, rewarded by good rotators.
	double RotateScore = 0.55 + Bt.StrikeRotation * 0.25 - FMath::Abs(A - 0.5) * 0.35;
	if (B.Line == ECricketLine::WideOutsideOff) RotateScore -= 0.15;  // hard to work a wide one

	// Attack: ground aggression; wants aggression, a hittable ball, a good matchup.
	double AttackScore = A * (0.35 + H * 0.6) + Bt.Power * 0.15 + Competence * 0.10 - B.CatchersUp * 0.15;

	// Boundary: the aerial; aggression^1.5, power-driven, deterred by sweepers.
	double BoundaryScore = FMath::Pow(A, 1.5) * (0.25 + H * 0.5 + Bt.Power * 0.45) - B.BoundaryProtection * 0.30;

	// --- Profile-driven temptation: weaknesses inflate the WRONG option's appeal ---
	if (IsWideLine(B.Line)) { AttackScore += Bt.OutsideOffTemptation * 0.35; }        // chase the wide one
	if (B.Length == ECricketLength::Short || B.Length == ECricketLength::Bouncer)
	{ BoundaryScore += Bt.ShortBallWeakness * 0.30; AttackScore += Bt.ShortBallWeakness * 0.15; }
	if (IsDrivableLength(B.Length) && IsStumpsLine(B.Line))
	{ AttackScore += Bt.FullBallWeakness * 0.15; }                                     // drive on the up

	// A near-hopeless chase forces the issue (must take the aerial route).
	if (S.bChasing && S.RequiredRunRate > 12.0) { BoundaryScore += 0.4; AttackScore += 0.25; LeaveScore -= 0.5; }
	// Protect the wicket if you've already won-ish or have nothing to chase.
	if (S.bChasing && S.RunsRequired <= 2 && S.BallsRemaining > 6) { DefendScore += 0.4; RotateScore += 0.3; BoundaryScore -= 0.6; }

	T.Add(TEXT("Leave"),    LeaveScore);
	T.Add(TEXT("Defend"),   DefendScore);
	T.Add(TEXT("Rotate"),   RotateScore);
	T.Add(TEXT("Attack"),   AttackScore);
	T.Add(TEXT("Boundary"), BoundaryScore);

	// Under pressure, lapses are more likely (a rush of blood) — but only mildly, or a
	// chasing side (which carries high pressure throughout) collapses far more often than
	// real T20 sides do. (Calibrated from the validation batch's chase-success metric.)
	const double MistakeScale = 1.0 + S.Pressure * 0.8;
	const int32 Chosen = FCricketTacticalEvaluator::SelectOption(T, D, Rng, MistakeScale);

	static const ECricketBatterAction Actions[] = {
		ECricketBatterAction::Leave, ECricketBatterAction::Defend, ECricketBatterAction::Rotate,
		ECricketBatterAction::Attack, ECricketBatterAction::BoundaryHit };
	Out.Action = Actions[FMath::Clamp(Chosen, 0, 4)];
	Out.bOffersNoShot = (Out.Action == ECricketBatterAction::Leave);

	BuildStroke(Out.Action, B, Batter, Out.Input, Out.Risk);

	T.Intent = FString::Printf(TEXT("A=%.2f %s %s%s -> %s"),
		A,
		S.bChasing ? *FString::Printf(TEXT("RRR=%.1f"), S.RequiredRunRate) : TEXT("set"),
		B.IsSpin() ? TEXT("vSpin ") : TEXT("vPace "),
		IsWideLine(B.Line) ? TEXT("wide") : (IsDrivableLength(B.Length) ? TEXT("full") : TEXT("len")),
		*T.Options[Chosen].Label.ToString());

	return Out;
}
