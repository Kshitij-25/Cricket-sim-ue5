#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "CricketPerfCategory.h"

/**
 * CRICKET_PERF_ENABLED — compile-time master switch. Instrumentation is fully
 * compiled OUT of Shipping builds (zero cost, zero footprint); it is active in
 * Development/Debug/Test where profiling matters.
 */
#ifndef CRICKET_PERF_ENABLED
	#if UE_BUILD_SHIPPING
		#define CRICKET_PERF_ENABLED 0
	#else
		#define CRICKET_PERF_ENABLED 1
	#endif
#endif

/**
 * FCricketFrameTimings — one frame's accumulated gameplay scope time, per category,
 * in milliseconds, plus the number of scopes that contributed. Produced by
 * FCricketProfiler::CaptureAndReset() once per frame.
 */
struct FCricketFrameTimings
{
	double Ms[CricketPerfCategoryCount] = {};
	int32 Calls[CricketPerfCategoryCount] = {};

	double GetMs(ECricketPerfCategory C) const { return Ms[static_cast<int32>(C)]; }
	int32 GetCalls(ECricketPerfCategory C) const { return Calls[static_cast<int32>(C)]; }
};

/**
 * FCricketProfiler — the global, game-thread profiling sink.
 *
 * Gameplay code wraps hot sections in CRICKET_PERF_SCOPE(Category); each scope adds
 * its elapsed time to the current frame's accumulator for that category. Once per
 * frame the Performance Manager calls CaptureAndReset() to read the totals and start
 * a fresh frame. This is deliberately a tiny, dependency-free accumulator so the
 * lowest modules can emit scopes without pulling in the manager.
 *
 * Threading: designed for the game thread (where the sim ticks). AddTime uses an
 * atomic add so a stray worker-thread scope can't corrupt the accumulator, but the
 * intended and tested usage is single-threaded per frame.
 */
class CRICKETPERFCORE_API FCricketProfiler
{
public:
	static FCricketProfiler& Get();

	/** When false, AddTime is ignored and scope timers skip their clock reads. */
	bool IsEnabled() const { return bEnabled; }
	void SetEnabled(bool bIn) { bEnabled = bIn; }

	/** Accumulate Seconds of cost into a category for the current frame. */
	void AddTime(ECricketPerfCategory Category, double Seconds);

	/** Read this frame's totals (ms) and reset the accumulators for the next frame. */
	FCricketFrameTimings CaptureAndReset();

	/** Drop all accumulated time without producing a report. */
	void ResetFrame();

private:
	FCricketProfiler() = default;

	bool bEnabled = (CRICKET_PERF_ENABLED != 0);
	double AccumSeconds[CricketPerfCategoryCount] = {};
	int32 AccumCalls[CricketPerfCategoryCount] = {};
};

/**
 * FCricketScopeTimer — RAII timer. Times its lifetime and adds the elapsed seconds
 * to the named category on destruction. Use via CRICKET_PERF_SCOPE; construct only
 * through the macro so it vanishes in Shipping.
 */
class FCricketScopeTimer
{
public:
	explicit FCricketScopeTimer(ECricketPerfCategory InCategory)
		: Category(InCategory)
		, StartSeconds(FCricketProfiler::Get().IsEnabled() ? FPlatformTime::Seconds() : 0.0)
		, bActive(FCricketProfiler::Get().IsEnabled())
	{
	}

	~FCricketScopeTimer()
	{
		if (bActive)
		{
			FCricketProfiler::Get().AddTime(Category, FPlatformTime::Seconds() - StartSeconds);
		}
	}

	FCricketScopeTimer(const FCricketScopeTimer&) = delete;
	FCricketScopeTimer& operator=(const FCricketScopeTimer&) = delete;

private:
	ECricketPerfCategory Category;
	double StartSeconds;
	bool bActive;
};

#if CRICKET_PERF_ENABLED
	#define CRICKET_PERF_SCOPE(Cat) \
		FCricketScopeTimer PREPROCESSOR_JOIN(CricketPerfScope_, __LINE__)(ECricketPerfCategory::Cat)
	#define CRICKET_PERF_ADD_SECONDS(Cat, Seconds) \
		FCricketProfiler::Get().AddTime(ECricketPerfCategory::Cat, (Seconds))
#else
	#define CRICKET_PERF_SCOPE(Cat)
	#define CRICKET_PERF_ADD_SECONDS(Cat, Seconds)
#endif
