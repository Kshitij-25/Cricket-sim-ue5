#include "CricketReplayTypes.h"

int32 FCricketReplayClip::FrameIndexAtTime(double T) const
{
	if (Frames.Num() == 0) { return INDEX_NONE; }
	if (T <= Frames[0].Time) { return 0; }
	if (T >= Frames.Last().Time) { return Frames.Num() - 1; }
	// Binary search for the last frame whose time <= T.
	int32 Lo = 0, Hi = Frames.Num() - 1;
	while (Lo < Hi)
	{
		const int32 Mid = (Lo + Hi + 1) / 2;
		if (Frames[Mid].Time <= T) { Lo = Mid; } else { Hi = Mid - 1; }
	}
	return Lo;
}

FCricketReplayFrame FCricketReplayClip::SampleAtTime(double T) const
{
	if (Frames.Num() == 0) { return FCricketReplayFrame(); }
	if (T <= Frames[0].Time) { return Frames[0]; }
	if (T >= Frames.Last().Time) { return Frames.Last(); }

	const int32 i = FrameIndexAtTime(T);
	if (!Frames.IsValidIndex(i + 1)) { return Frames[i]; }

	const FCricketReplayFrame& A = Frames[i];
	const FCricketReplayFrame& B = Frames[i + 1];
	const double Span = B.Time - A.Time;
	const double Alpha = Span > KINDA_SMALL_NUMBER ? (T - A.Time) / Span : 0.0;

	FCricketReplayFrame Out;
	Out.Time = T;

	// Ball: lerp vectors; the (nearly-unit) seam is lerp + renormalise.
	Out.Ball.PositionM = FMath::Lerp(A.Ball.PositionM, B.Ball.PositionM, Alpha);
	Out.Ball.VelocityMS = FMath::Lerp(A.Ball.VelocityMS, B.Ball.VelocityMS, Alpha);
	Out.Ball.AngularVelocityRadS = FMath::Lerp(A.Ball.AngularVelocityRadS, B.Ball.AngularVelocityRadS, Alpha);
	Out.Ball.SeamNormal = FMath::Lerp(A.Ball.SeamNormal, B.Ball.SeamNormal, Alpha).GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
	Out.Ball.bInFlight = (Alpha < 0.5) ? A.Ball.bInFlight : B.Ball.bInFlight;

	// Actors: match by ActorId, lerp location, slerp rotation, nearest anim state.
	Out.Actors.Reserve(A.Actors.Num());
	for (const FCricketActorSnapshot& SA : A.Actors)
	{
		const FCricketActorSnapshot* SB = B.Actors.FindByPredicate(
			[&](const FCricketActorSnapshot& X) { return X.ActorId == SA.ActorId; });

		FCricketActorSnapshot Snap;
		Snap.ActorId = SA.ActorId;
		if (SB)
		{
			Snap.LocationCm = FMath::Lerp(SA.LocationCm, SB->LocationCm, Alpha);
			Snap.Rotation = FQuat::Slerp(SA.Rotation.Quaternion(), SB->Rotation.Quaternion(), Alpha).Rotator();
			Snap.AnimStateId = (Alpha < 0.5) ? SA.AnimStateId : SB->AnimStateId;
		}
		else
		{
			Snap.LocationCm = SA.LocationCm;
			Snap.Rotation = SA.Rotation;
			Snap.AnimStateId = SA.AnimStateId;
		}
		Out.Actors.Add(Snap);
	}
	return Out;
}

void FCricketReplayClip::GetBallPath(TArray<FVector>& OutPathM) const
{
	OutPathM.Reset(Frames.Num());
	for (const FCricketReplayFrame& F : Frames)
	{
		OutPathM.Add(F.Ball.PositionM);
	}
}

void FCricketReplayClip::GetEventLocations(ECricketReplayEventType Type, TArray<FVector>& OutM) const
{
	OutM.Reset();
	for (const FCricketReplayEvent& E : Events)
	{
		if (E.Type == Type) { OutM.Add(E.LocationM); }
	}
}
