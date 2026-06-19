#include "CricketReplayOptimizer.h"

int64 FCricketReplayOptimizer::EstimateClipBytes(const FCricketReplayClip& Clip)
{
	// Fixed per-frame struct cost plus the heap data its actor array points at.
	int64 Bytes = static_cast<int64>(Clip.Frames.Num()) * sizeof(FCricketReplayFrame);
	for (const FCricketReplayFrame& F : Clip.Frames)
	{
		Bytes += static_cast<int64>(F.Actors.Num()) * sizeof(FCricketActorSnapshot);
	}
	Bytes += static_cast<int64>(Clip.Events.Num()) * sizeof(FCricketReplayEvent);
	return Bytes;
}

namespace
{
	FVector QuantizeMeters(const FVector& P, int32 QuantizeMm)
	{
		if (QuantizeMm <= 0) { return P; }
		const double Step = static_cast<double>(QuantizeMm) / 1000.0; // mm → m
		return FVector(
			FMath::RoundToDouble(P.X / Step) * Step,
			FMath::RoundToDouble(P.Y / Step) * Step,
			FMath::RoundToDouble(P.Z / Step) * Step);
	}

	// Is there a recorded event within Guard seconds of time T?
	bool NearEvent(const FCricketReplayClip& Clip, double T, double Guard)
	{
		for (const FCricketReplayEvent& E : Clip.Events)
		{
			if (FMath::Abs(E.Time - T) <= Guard) { return true; }
		}
		return false;
	}
}

FCricketReplayOptimizationReport FCricketReplayOptimizer::Optimize(
	const FCricketReplayClip& In,
	const FCricketReplayOptimizerSettings& Settings,
	FCricketReplayClip& Out)
{
	FCricketReplayOptimizationReport Report;
	Report.OriginalFrames = In.Frames.Num();
	Report.OriginalBytes = EstimateClipBytes(In);

	Out.Reset();
	Out.MaxFrames = In.MaxFrames;
	Out.Events = In.Events; // events are sparse and always preserved verbatim

	const int32 N = In.Frames.Num();
	if (N == 0)
	{
		Report.OptimizedBytes = EstimateClipBytes(Out);
		Report.CompressionRatio = 1.0f;
		return Report;
	}

	const double Guard = FMath::Max(Settings.EventGuardSeconds, 0.0f);
	const double MinMotionSq = static_cast<double>(Settings.MinMotionM) * Settings.MinMotionM;

	// --- Pass 1: decide which frames to KEEP (adaptive sampling) ---------------
	// Keep the first & last frame, any frame near an event, and any frame whose ball
	// has moved past the motion threshold since the last kept frame.
	TArray<int32> Kept;
	Kept.Reserve(N);
	FVector LastKeptPos = In.Frames[0].Ball.PositionM;
	Kept.Add(0);

	for (int32 i = 1; i < N - 1; ++i)
	{
		const FCricketReplayFrame& F = In.Frames[i];
		const double MovedSq = FVector::DistSquared(F.Ball.PositionM, LastKeptPos);
		const bool bKeep = (MovedSq >= MinMotionSq) || NearEvent(In, F.Time, Guard);
		if (bKeep)
		{
			Kept.Add(i);
			LastKeptPos = F.Ball.PositionM;
		}
	}
	if (N > 1) { Kept.Add(N - 1); }

	// --- Pass 2: emit kept frames, quantizing positions -----------------------
	for (int32 KeptIdx : Kept)
	{
		FCricketReplayFrame F = In.Frames[KeptIdx];
		F.Ball.PositionM = QuantizeMeters(F.Ball.PositionM, Settings.PositionQuantizeMm);
		Out.Frames.Add(MoveTemp(F));
	}

	// --- Quality: worst-case reconstruction error over ALL original frames -----
	// For every original frame, reconstruct the ball position the way playback will
	// (linear interpolation across the kept frames) and measure the deviation.
	double MaxErr = 0.0;
	int32 KeptCursor = 0;
	for (int32 i = 0; i < N; ++i)
	{
		const double T = In.Frames[i].Time;
		// Advance the cursor so Out.Frames[KeptCursor].Time <= T <= next.
		while (KeptCursor + 1 < Out.Frames.Num() && Out.Frames[KeptCursor + 1].Time <= T)
		{
			++KeptCursor;
		}
		FVector Recon;
		if (KeptCursor + 1 < Out.Frames.Num())
		{
			const FCricketReplayFrame& A = Out.Frames[KeptCursor];
			const FCricketReplayFrame& B = Out.Frames[KeptCursor + 1];
			const double Span = B.Time - A.Time;
			const double Alpha = Span > KINDA_SMALL_NUMBER ? (T - A.Time) / Span : 0.0;
			Recon = FMath::Lerp(A.Ball.PositionM, B.Ball.PositionM, FMath::Clamp(Alpha, 0.0, 1.0));
		}
		else
		{
			Recon = Out.Frames.Last().Ball.PositionM;
		}
		MaxErr = FMath::Max(MaxErr, FVector::Dist(Recon, In.Frames[i].Ball.PositionM));
	}

	Report.OptimizedFrames = Out.Frames.Num();
	Report.OptimizedBytes = EstimateClipBytes(Out);
	Report.CompressionRatio = Report.OriginalBytes > 0
		? static_cast<float>(static_cast<double>(Report.OptimizedBytes) / Report.OriginalBytes)
		: 1.0f;
	Report.MaxPositionErrorM = static_cast<float>(MaxErr);
	Report.SavedMB = static_cast<float>(
		static_cast<double>(Report.OriginalBytes - Report.OptimizedBytes) / (1024.0 * 1024.0));
	return Report;
}
