#include "CricketPerfStats.h"

void FCricketRollingStat::Reset(int32 InCapacity)
{
	const int32 Cap = FMath::Max(InCapacity, 1);
	Samples.Reset(Cap);
	Samples.SetNumZeroed(Cap);
	Head = 0;
	Count = 0;
	LastValue = 0.0;
}

void FCricketRollingStat::Clear()
{
	Head = 0;
	Count = 0;
	LastValue = 0.0;
}

void FCricketRollingStat::Push(double Value)
{
	if (Samples.Num() == 0)
	{
		Reset(120);
	}
	Samples[Head] = Value;
	Head = (Head + 1) % Samples.Num();
	Count = FMath::Min(Count + 1, Samples.Num());
	LastValue = Value;
}

double FCricketRollingStat::Min() const
{
	if (Count == 0) { return 0.0; }
	double M = TNumericLimits<double>::Max();
	for (int32 i = 0; i < Count; ++i) { M = FMath::Min(M, Samples[i]); }
	return M;
}

double FCricketRollingStat::Max() const
{
	if (Count == 0) { return 0.0; }
	double M = TNumericLimits<double>::Lowest();
	for (int32 i = 0; i < Count; ++i) { M = FMath::Max(M, Samples[i]); }
	return M;
}

double FCricketRollingStat::Average() const
{
	if (Count == 0) { return 0.0; }
	double Sum = 0.0;
	for (int32 i = 0; i < Count; ++i) { Sum += Samples[i]; }
	return Sum / Count;
}

double FCricketRollingStat::Percentile(double P) const
{
	if (Count == 0) { return 0.0; }
	// Copy the valid samples and sort; the windows are small (≤ a few hundred), so
	// a full sort per query is cheap and keeps push O(1).
	TArray<double, TInlineAllocator<256>> Sorted;
	Sorted.Reserve(Count);
	for (int32 i = 0; i < Count; ++i) { Sorted.Add(Samples[i]); }
	Sorted.Sort();
	const double Clamped = FMath::Clamp(P, 0.0, 1.0);
	// Nearest-rank percentile.
	const int32 Rank = FMath::Clamp(
		FMath::CeilToInt(Clamped * Count) - 1, 0, Count - 1);
	return Sorted[Rank];
}

FCricketStatSnapshot FCricketRollingStat::Snapshot() const
{
	FCricketStatSnapshot S;
	S.Last = static_cast<float>(LastValue);
	S.Min = static_cast<float>(Min());
	S.Avg = static_cast<float>(Average());
	S.Max = static_cast<float>(Max());
	S.P95 = static_cast<float>(Percentile(0.95));
	S.Samples = Count;
	return S;
}
