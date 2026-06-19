#include "CricketPerfProfiler.h"
#include "HAL/PlatformAtomics.h"

FCricketProfiler& FCricketProfiler::Get()
{
	static FCricketProfiler Instance;
	return Instance;
}

void FCricketProfiler::AddTime(ECricketPerfCategory Category, double Seconds)
{
	if (!bEnabled || Seconds < 0.0)
	{
		return;
	}
	const int32 Idx = static_cast<int32>(Category);
	// Game-thread accumulation is the common path; the atomic guards the rare case
	// of a scope opened on a worker thread so it can never tear the double/int.
	if (IsInGameThread())
	{
		AccumSeconds[Idx] += Seconds;
		AccumCalls[Idx] += 1;
	}
	else
	{
		// Reinterpret the double's bits to add atomically without a lock.
		volatile int64* AsInt = reinterpret_cast<volatile int64*>(&AccumSeconds[Idx]);
		for (;;)
		{
			const int64 Old = FPlatformAtomics::AtomicRead(AsInt);
			double OldD; FMemory::Memcpy(&OldD, &Old, sizeof(double));
			const double NewD = OldD + Seconds;
			int64 New; FMemory::Memcpy(&New, &NewD, sizeof(double));
			if (FPlatformAtomics::InterlockedCompareExchange(AsInt, New, Old) == Old) { break; }
		}
		FPlatformAtomics::InterlockedIncrement(reinterpret_cast<volatile int32*>(&AccumCalls[Idx]));
	}
}

FCricketFrameTimings FCricketProfiler::CaptureAndReset()
{
	FCricketFrameTimings Out;
	for (int32 i = 0; i < CricketPerfCategoryCount; ++i)
	{
		Out.Ms[i] = AccumSeconds[i] * 1000.0;
		Out.Calls[i] = AccumCalls[i];
		AccumSeconds[i] = 0.0;
		AccumCalls[i] = 0;
	}
	return Out;
}

void FCricketProfiler::ResetFrame()
{
	for (int32 i = 0; i < CricketPerfCategoryCount; ++i)
	{
		AccumSeconds[i] = 0.0;
		AccumCalls[i] = 0;
	}
}
