#include "CricketAnimationModel.h"

namespace
{
	FCricketAnimPhase Phase(int32 StateId, double Duration)
	{
		FCricketAnimPhase P; P.StateId = StateId; P.Duration = Duration; return P;
	}
	FCricketAnimNotifyDef Notify(ECricketAnimNotify Type, double Time)
	{
		FCricketAnimNotifyDef N; N.Type = Type; N.Time = Time; return N;
	}
}

FCricketLocomotionSample FCricketAnimationModel::ClassifyLocomotion(
	double SpeedMS, double TurnRateDeg, double AccelMS2,
	ECricketLocomotionState Prev, const FCricketLocomotionConfig& Config)
{
	FCricketLocomotionSample S;
	S.SpeedMS = SpeedMS;
	S.GaitBlend = FMath::Clamp(SpeedMS / FMath::Max(Config.SprintSpeed, 0.1), 0.0, 1.0);

	// Stopping: moving but decelerating hard toward a halt.
	if (AccelMS2 <= -Config.StopDecelMS2 && SpeedMS > Config.StopSpeed && SpeedMS < Config.JogSpeed)
	{
		S.State = ECricketLocomotionState::Stop;
		return S;
	}

	// Idle / turn-in-place.
	if (SpeedMS < Config.StopSpeed)
	{
		S.State = (FMath::Abs(TurnRateDeg) > Config.TurnRateDeg) ? ECricketLocomotionState::Turn : ECricketLocomotionState::Idle;
		return S;
	}

	// A sharp turn while still moving slowly reads as a turn.
	if (FMath::Abs(TurnRateDeg) > Config.TurnRateDeg && SpeedMS < Config.WalkSpeed)
	{
		S.State = ECricketLocomotionState::Turn;
		return S;
	}

	// Gait by speed band.
	if (SpeedMS < Config.WalkSpeed)       { S.State = ECricketLocomotionState::Walk; }
	else if (SpeedMS < Config.JogSpeed)   { S.State = ECricketLocomotionState::Jog; }
	else                                  { S.State = ECricketLocomotionState::Sprint; }
	return S;
}

FCricketActionMontage FCricketAnimationModel::MakeBowlingMontage(const FCricketBowlingActionTimeline& T)
{
	FCricketActionMontage M;
	// Phases: run-up -> delivery stride -> (release is an instant within stride) ->
	// follow-through -> recover. The "Release" state is a brief window around the
	// release instant for the AnimBP to play the release pose.
	const double ReleaseWindow = FMath::Min(0.08, T.DeliveryStrideTimeSec * 0.3);
	const double StridePre = FMath::Max(T.ReleaseInStrideSec - ReleaseWindow * 0.5, 0.0);
	const double StridePost = FMath::Max(T.DeliveryStrideTimeSec - StridePre - ReleaseWindow, 0.0);

	M.Phases.Add(Phase((int32)ECricketBowlingAnimState::RunUp, T.RunUpTimeSec));
	M.Phases.Add(Phase((int32)ECricketBowlingAnimState::DeliveryStride, StridePre));
	M.Phases.Add(Phase((int32)ECricketBowlingAnimState::Release, ReleaseWindow));
	M.Phases.Add(Phase((int32)ECricketBowlingAnimState::FollowThrough, StridePost + T.FollowThroughTimeSec));
	M.Phases.Add(Phase((int32)ECricketBowlingAnimState::Recover, 0.3));

	// Footfalls during the run-up, then the all-important release.
	const int32 Strides = 3;
	for (int32 i = 1; i <= Strides; ++i)
	{
		M.Notifies.Add(Notify(ECricketAnimNotify::FootPlant, T.RunUpTimeSec * i / (Strides + 1)));
	}
	M.Notifies.Add(Notify(ECricketAnimNotify::FootPlant, T.RunUpTimeSec)); // back-foot landing
	M.Notifies.Add(Notify(ECricketAnimNotify::BallRelease, T.ReleaseTimeSec()));
	return M;
}

FCricketActionMontage FCricketAnimationModel::MakeCatchMontage(double ReachTimeSec, double SecureTimeSec)
{
	FCricketActionMontage M;
	M.Phases.Add(Phase((int32)ECricketFieldingAnimState::Catch, ReachTimeSec));
	M.Phases.Add(Phase((int32)ECricketFieldingAnimState::Catch, SecureTimeSec));
	M.Notifies.Add(Notify(ECricketAnimNotify::CatchAttempt, ReachTimeSec)); // hands close
	return M;
}

FCricketActionMontage FCricketAnimationModel::MakePickupMontage(double ReachTimeSec, double GatherTimeSec)
{
	FCricketActionMontage M;
	M.Phases.Add(Phase((int32)ECricketFieldingAnimState::GroundStop, ReachTimeSec));
	M.Phases.Add(Phase((int32)ECricketFieldingAnimState::Pickup, GatherTimeSec));
	M.Notifies.Add(Notify(ECricketAnimNotify::PickupContact, ReachTimeSec)); // hand on the ball
	return M;
}

FCricketActionMontage FCricketAnimationModel::MakeThrowMontage(double WindupTimeSec, double ReleaseTimeSec, double RecoverTimeSec)
{
	FCricketActionMontage M;
	M.Phases.Add(Phase((int32)ECricketFieldingAnimState::Throw, ReleaseTimeSec));
	M.Phases.Add(Phase((int32)ECricketFieldingAnimState::ReturnToPosition, RecoverTimeSec));
	M.Notifies.Add(Notify(ECricketAnimNotify::ThrowRelease, ReleaseTimeSec)); // ball leaves the hand
	(void)WindupTimeSec;
	return M;
}

ECricketBattingAnimState FCricketAnimationModel::MapBattingPhase(ECricketSwingPhase Phase)
{
	switch (Phase)
	{
	case ECricketSwingPhase::Idle:          return ECricketBattingAnimState::Guard;
	case ECricketSwingPhase::Backlift:      return ECricketBattingAnimState::Backlift;
	case ECricketSwingPhase::Downswing:     return ECricketBattingAnimState::Downswing;
	case ECricketSwingPhase::Contact:       return ECricketBattingAnimState::Impact;
	case ECricketSwingPhase::FollowThrough: return ECricketBattingAnimState::FollowThrough;
	case ECricketSwingPhase::Recovery:      return ECricketBattingAnimState::Recover;
	default:                                return ECricketBattingAnimState::Guard;
	}
}

FCricketActionMontage FCricketAnimationModel::MakeBattingMontage(double BackliftTimeSec, double DownswingTimeSec, double FollowThroughTimeSec)
{
	FCricketActionMontage M;
	M.Phases.Add(Phase((int32)ECricketBattingAnimState::Backlift, BackliftTimeSec));
	M.Phases.Add(Phase((int32)ECricketBattingAnimState::Downswing, DownswingTimeSec));
	M.Phases.Add(Phase((int32)ECricketBattingAnimState::FollowThrough, FollowThroughTimeSec));
	M.Phases.Add(Phase((int32)ECricketBattingAnimState::Recover, 0.15));
	// Impact is at the end of the downswing — the contact instant.
	M.Notifies.Add(Notify(ECricketAnimNotify::BatImpact, BackliftTimeSec + DownswingTimeSec));
	return M;
}
