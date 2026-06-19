#include "CricketPerfBudget.h"

void FCricketSimulationBudget::SetFromFrameTarget(double InFrameTargetMs)
{
	FrameTargetMs = FMath::Max(InFrameTargetMs, 1.0);
	const double T = FrameTargetMs;

	// Whole-frame measures share the full target (they ARE the frame).
	BudgetMs[static_cast<int32>(ECricketPerfCategory::Frame)]        = T;
	BudgetMs[static_cast<int32>(ECricketPerfCategory::GameThread)]   = T;
	BudgetMs[static_cast<int32>(ECricketPerfCategory::RenderThread)] = T;
	BudgetMs[static_cast<int32>(ECricketPerfCategory::GPU)]          = T;

	// Gameplay slices, as fractions of the frame target. Physics-first: the ball
	// integrator + contact resolution gets the largest gameplay slice. These sum to
	// well under 1.0 — the remainder is engine overhead, gameplay glue and headroom.
	BudgetMs[static_cast<int32>(ECricketPerfCategory::Physics)]    = 0.22 * T;
	BudgetMs[static_cast<int32>(ECricketPerfCategory::Prediction)] = 0.10 * T;
	BudgetMs[static_cast<int32>(ECricketPerfCategory::AI)]         = 0.15 * T;
	BudgetMs[static_cast<int32>(ECricketPerfCategory::Animation)]  = 0.12 * T;
	BudgetMs[static_cast<int32>(ECricketPerfCategory::Replay)]     = 0.08 * T;
	BudgetMs[static_cast<int32>(ECricketPerfCategory::Other)]      = 0.10 * T;
}

FCricketBudgetResult FCricketSimulationBudget::Evaluate(ECricketPerfCategory Category, double CostMs) const
{
	FCricketBudgetResult R;
	R.Category = Category;
	R.CostMs = static_cast<float>(CostMs);

	const double Budget = GetBudget(Category);
	R.BudgetMs = static_cast<float>(Budget);

	if (Budget <= 0.0)
	{
		// No budget assigned: report fraction 0 and never flag.
		R.FractionUsed = 0.0f;
		R.Status = ECricketBudgetStatus::UnderBudget;
		return R;
	}

	const double Fraction = CostMs / Budget;
	R.FractionUsed = static_cast<float>(Fraction);

	if (Fraction > 1.0)        { R.Status = ECricketBudgetStatus::OverBudget; }
	else if (Fraction >= WarnFraction) { R.Status = ECricketBudgetStatus::Warning; }
	else                       { R.Status = ECricketBudgetStatus::UnderBudget; }

	return R;
}
