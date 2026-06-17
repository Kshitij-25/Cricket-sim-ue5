#include "CricketInputModel.h"

double FCricketInputModel::DirectionAimYawDeg(ECricketShotDirection Direction)
{
	// + opens the face toward the off side (RH). These are gentle steering offsets;
	// the actual placement still emerges from the swing + contact physics.
	switch (Direction)
	{
	case ECricketShotDirection::Straight:        return 0.0;
	case ECricketShotDirection::OffSide:         return 30.0;
	case ECricketShotDirection::CoverRegion:     return 20.0;
	case ECricketShotDirection::LegSide:         return -30.0;
	case ECricketShotDirection::MidwicketRegion: return -20.0;
	case ECricketShotDirection::FineLeg:         return -42.0;
	default:                                     return 0.0;
	}
}

FCricketBattingShotIntent FCricketInputModel::ResolveBattingShot(const FCricketBattingControlState& State)
{
	FCricketBattingShotIntent Out;
	Out.Direction = State.Direction;
	Out.bLofted = State.bLofted && !State.bDefensive; // can't loft a block
	Out.AimYawDeg = DirectionAimYawDeg(State.Direction);

	// Defensive overrides everything (S).
	if (State.bDefensive)
	{
		Out.Shot = ECricketC07Shot::Defensive;
		Out.Footwork = ECricketFootwork::FrontFoot; // a solid forward block
		Out.PowerScale = 0.55;
		Out.AimYawDeg = 0.0;
		return Out;
	}

	// Footwork: D = front foot, W = back foot, neither = neutral.
	const bool bFront = State.bFrontFoot && !State.bBackFoot;
	const bool bBack = State.bBackFoot && !State.bFrontFoot;
	Out.Footwork = bFront ? ECricketFootwork::FrontFoot : (bBack ? ECricketFootwork::BackFoot : ECricketFootwork::Neutral);

	const bool bOff = (State.Direction == ECricketShotDirection::OffSide || State.Direction == ECricketShotDirection::CoverRegion);
	const bool bLeg = (State.Direction == ECricketShotDirection::LegSide || State.Direction == ECricketShotDirection::MidwicketRegion
		|| State.Direction == ECricketShotDirection::FineLeg);

	// The shot is the natural stroke for (foot x direction) — the Cricket 07 grid.
	if (bFront)
	{
		if (bOff)        { Out.Shot = ECricketC07Shot::CoverDrive; }
		else if (bLeg)   { Out.Shot = ECricketC07Shot::FlickShot; }   // on-side wristy flick
		else             { Out.Shot = ECricketC07Shot::StraightDrive; }
	}
	else if (bBack)
	{
		if (bOff)        { Out.Shot = ECricketC07Shot::CutShot; }      // back-foot off-side
		else if (bLeg)   { Out.Shot = ECricketC07Shot::PullShot; }
		else             { Out.Shot = ECricketC07Shot::StraightDrive; } // back-foot punch
	}
	else // neutral
	{
		if (bOff)        { Out.Shot = ECricketC07Shot::CoverDrive; }
		else if (bLeg)   { Out.Shot = ECricketC07Shot::PullShot; }
		else             { Out.Shot = ECricketC07Shot::StraightDrive; }
	}

	// The lofted modifier promotes a grounded drive to a lofted drive.
	if (Out.bLofted) { Out.Shot = ECricketC07Shot::LoftedDrive; }

	// Power: aggressive strokes (pull/cut/lofted) swing harder; flick is a placement.
	switch (Out.Shot)
	{
	case ECricketC07Shot::PullShot:   Out.PowerScale = 1.05; break;
	case ECricketC07Shot::CutShot:    Out.PowerScale = 1.0;  break;
	case ECricketC07Shot::LoftedDrive: Out.PowerScale = 1.2; break;
	case ECricketC07Shot::FlickShot:  Out.PowerScale = 0.9;  break;
	default:                          Out.PowerScale = 1.0;  break;
	}
	return Out;
}

FCricketBattingInput FCricketInputModel::ToBattingInput(const FCricketBattingShotIntent& Intent, bool bRightHanded)
{
	FCricketBattingInput In;
	In.bRightHanded = bRightHanded;
	In.Footwork = Intent.Footwork;
	In.PowerScale = Intent.PowerScale;

	// Map the seven control strokes onto the four physics base kinematics + aim. The
	// base shot only chooses the swing SHAPE; direction + footwork do the steering.
	double BaseAim = Intent.AimYawDeg;
	switch (Intent.Shot)
	{
	case ECricketC07Shot::Defensive:
		In.ShotType = ECricketShotType::DefensiveBlock; break;
	case ECricketC07Shot::StraightDrive:
		In.ShotType = ECricketShotType::StraightDrive; break;
	case ECricketC07Shot::CoverDrive:
		In.ShotType = ECricketShotType::CoverDrive; break;
	case ECricketC07Shot::PullShot:
		In.ShotType = ECricketShotType::PullShot; break;
	case ECricketC07Shot::CutShot:
		// A cut is a back-foot off-side stroke — cover-drive shape, squarer aim.
		In.ShotType = ECricketShotType::CoverDrive; In.Footwork = ECricketFootwork::BackFoot; BaseAim = FMath::Max(BaseAim, 35.0); break;
	case ECricketC07Shot::FlickShot:
		// A flick is an on-side stroke off the pads — pull shape, fine-ish leg aim.
		In.ShotType = ECricketShotType::PullShot; In.Footwork = ECricketFootwork::FrontFoot; BaseAim = FMath::Min(BaseAim, -18.0); break;
	case ECricketC07Shot::LoftedDrive:
		// A lofted drive: drive shape, more power; the LOFT itself emerges from the
		// contact under the ball — we don't script the launch angle.
		In.ShotType = ECricketShotType::StraightDrive; break;
	}

	In.AimYawDeg = FMath::Clamp(BaseAim, -45.0, 45.0);
	return In;
}

FCricketBowlingControlIntent FCricketInputModel::ResolveDelivery(const FCricketBowlingControlState& State)
{
	FCricketBowlingControlIntent Out;
	Out.LineStepDir = FMath::Clamp(State.LineStep, -1, 1);
	Out.LengthStepDir = FMath::Clamp(State.LengthStep, -1, 1);

	switch (State.Delivery)
	{
	case ECricketDeliveryChoice::Stock:      Out.Pace01 = 0.8; break;
	case ECricketDeliveryChoice::Variation:  Out.Pace01 = 0.65; break; // slower ball / change-up
	case ECricketDeliveryChoice::Aggressive: Out.Pace01 = 1.0; break;  // quickest
	}

	// Modifiers dial swing / spin. (A delivery is one or the other in practice; both
	// flags simply express the player's emphasis and are clamped by the action.)
	Out.SwingAmount = State.bSwingMod ? 0.9 : 0.3;
	Out.SpinAmount = State.bSpinMod ? 0.9 : 0.5;
	return Out;
}

ECricketRunCall FCricketInputModel::ResolveRunCall(bool bTake, bool bSendBack, bool bDive)
{
	if (bDive)     { return ECricketRunCall::Dive; }     // W (highest priority — emergency)
	if (bSendBack) { return ECricketRunCall::SendBack; } // A
	if (bTake)     { return ECricketRunCall::Take; }     // D
	return ECricketRunCall::None;
}

ECricketFieldAction FCricketInputModel::ResolveFieldAction(bool bCatch, bool bDive, bool bThrow, bool bRelay, bool bMove)
{
	if (bCatch) { return ECricketFieldAction::Catch; } // catching the ball comes first
	if (bDive)  { return ECricketFieldAction::Dive; }
	if (bThrow) { return ECricketFieldAction::Throw; }
	if (bRelay) { return ECricketFieldAction::RelayThrow; }
	if (bMove)  { return ECricketFieldAction::Move; }
	return ECricketFieldAction::None;
}

ECricketInputContext FCricketInputModel::ResolveContext(
	bool bReplayActive, bool bIsBatting, bool bIsBowling, bool bIsFielding)
{
	if (bReplayActive) { return ECricketInputContext::Replay; } // overrides all
	if (bIsBatting)    { return ECricketInputContext::Batting; }
	if (bIsBowling)    { return ECricketInputContext::Bowling; }
	if (bIsFielding)   { return ECricketInputContext::Fielding; }
	return ECricketInputContext::Match;
}
