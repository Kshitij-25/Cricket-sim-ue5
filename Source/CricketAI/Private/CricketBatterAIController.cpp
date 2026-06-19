#include "CricketBatterAIController.h"
#include "CricketBattingComponent.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketPerfProfiler.h"

UCricketBatterAIController::UCricketBatterAIController()
{
	PrimaryComponentTick.bCanEverTick = true;
	Profile.Name = TEXT("AI Batter");
}

void UCricketBatterAIController::SetTargets(UCricketBattingComponent* InBatting)
{
	Batting = InBatting;
}

FCricketDeliveryRead UCricketBatterAIController::ReadDelivery(UCricketBallPhysicsComponent* BP) const
{
	FCricketDeliveryRead Read;
	Read.BowlerStyle = ECricketBowlingStyle::Pace; // unknown live; matchup still uses VsPace/VsSpin sensibly
	Read.BowlerPace01 = 0.75;

	const UCricketBattingComponent* Bat = Batting.Get();
	if (!Bat) { return Read; }

	const FCricketBallState& St = BP->GetState();
	const FVector PosM = St.Position;
	const FVector VelM = St.Velocity;
	const FVector StanceM = Bat->GetStanceOriginM();

	const double Vx = VelM.X;
	if (FMath::Abs(Vx) < 0.5) { return Read; }                 // not travelling down the pitch yet

	// Time for the ball to reach the striker's stance plane, and the predicted line there.
	const double tContact = FMath::Clamp((StanceM.X - PosM.X) / Vx, 0.0, 2.0);
	const double PredY = PosM.Y + VelM.Y * tContact;           // +Y = off side for RH

	if (PredY > 0.30)       { Read.Line = ECricketLine::WideOutsideOff; }
	else if (PredY > 0.12)  { Read.Line = ECricketLine::OutsideOff; }
	else if (PredY > 0.0)   { Read.Line = ECricketLine::OffStump; }
	else if (PredY > -0.12) { Read.Line = ECricketLine::Middle; }
	else if (PredY > -0.30) { Read.Line = ECricketLine::LegStump; }
	else                    { Read.Line = ECricketLine::DownLeg; }

	// Length: solve where the ball pitches (z reaches ground) and how far that is in
	// front of the stumps. If it has effectively passed the bounce, read it full.
	const double GroundZ = StanceM.Z;
	const double Vz = VelM.Z;
	const double H = PosM.Z - GroundZ;
	double BounceX = PosM.X;
	if (H > 0.02)
	{
		// z(t) = H + Vz t - 0.5 g t^2 = 0  -> larger positive root.
		const double g = 9.81;
		const double Disc = Vz * Vz + 2.0 * g * H;
		if (Disc > 0.0)
		{
			const double tb = (Vz + FMath::Sqrt(Disc)) / g;
			BounceX = PosM.X + Vx * tb;
		}
	}
	const double LenFromStumpsM = StanceM.X - BounceX;          // + = pitches before the stumps
	if (LenFromStumpsM < 0.0)       { Read.Length = ECricketLength::FullToss; }
	else if (LenFromStumpsM < 0.5)  { Read.Length = ECricketLength::Yorker; }
	else if (LenFromStumpsM < 2.5)  { Read.Length = ECricketLength::Full; }
	else if (LenFromStumpsM < 5.0)  { Read.Length = ECricketLength::GoodLength; }
	else if (LenFromStumpsM < 7.0)  { Read.Length = ECricketLength::BackOfLength; }
	else if (LenFromStumpsM < 9.0)  { Read.Length = ECricketLength::Short; }
	else                            { Read.Length = ECricketLength::Bouncer; }

	// Pace read from the down-pitch speed (km/h ~ Vx*3.6); spin reads slow.
	Read.BowlerPace01 = FMath::Clamp((FMath::Abs(Vx) * 3.6 - 110.0) / 40.0, 0.0, 1.0);
	Read.BowlerStyle  = (FMath::Abs(Vx) * 3.6 < 95.0) ? ECricketBowlingStyle::OffSpin : ECricketBowlingStyle::Pace;

	// Field is unknown to a lone batter controller; assume a neutral set.
	Read.BoundaryProtection = 0.4;
	Read.CatchersUp = 0.3;
	return Read;
}

void UCricketBatterAIController::OnNewDelivery(UCricketBallPhysicsComponent* BP)
{
	LastRead = ReadDelivery(BP);

	const FCricketAIDifficultyParams D = FCricketAIDifficultyParams::Resolve(Difficulty);
	FRandomStream Rng(Seed++);
	LastDecision = FCricketBatterBrain::Decide(Situation, Profile, LastRead, Strategy, D, Rng);

	// Push the chosen intent into the batting controller (the human control surface).
	if (UCricketBattingComponent* Bat = Batting.Get())
	{
		Bat->SetHanded(LastDecision.Input.bRightHanded);
		Bat->SelectShot(LastDecision.Input.ShotType);
		Bat->SetFootwork(LastDecision.Input.Footwork);
		Bat->SetAimYawDeg(LastDecision.Input.AimYawDeg);
		Bat->SetPower(LastDecision.Input.PowerScale);
	}

	// Mistiming: a worse player swings early/late. Pure execution quality, not a
	// reaction-time advantage — it only ever HURTS the timing.
	TimingOffsetSec = (Rng.FRand() - 0.5) * 2.0 * D.ExecutionNoise * 0.10;
}

void UCricketBatterAIController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Profiled: batter brain read/decision each frame.
	CRICKET_PERF_SCOPE(AI);

	UCricketBattingComponent* Bat = Batting.Get();
	if (!Bat) { return; }
	UCricketBallPhysicsComponent* BP = Bat->GetTargetBallPhysics();
	const bool bInFlight = BP && BP->IsBallInFlight();

	if (!bInFlight)
	{
		// Between deliveries: re-arm for the next ball.
		bArmed = false; bSwingFired = false;
		return;
	}

	if (!bArmed)
	{
		OnNewDelivery(BP);
		bArmed = true;
	}

	if (bSwingFired || Bat->IsSwinging() || Bat->HasPlayed()) { return; }
	if (LastDecision.bOffersNoShot) { return; }                 // leaving: never offer a shot

	// Trigger when the ball is roughly one downswing away from the contact zone.
	const FCricketBallState& St = BP->GetState();
	const FVector StanceM = Bat->GetStanceOriginM();
	const double Vx = St.Velocity.X;
	if (FMath::Abs(Vx) < 0.5) { return; }

	const double tContact = (StanceM.X - St.Position.X) / Vx;
	const double Lead = SwingLeadSec + TimingOffsetSec;
	if (tContact <= Lead && tContact > -0.20)
	{
		if (LastDecision.Action == ECricketBatterAction::Defend) { Bat->Defend(); }
		else { Bat->TriggerSwing(); }
		bSwingFired = true;
	}
}
