#pragma once

#include "CoreMinimal.h"
#include "CricketBalanceConfig.generated.h"

/**
 * CricketBalanceConfig — the PRODUCTION BALANCING FRAMEWORK.
 *
 * One data-driven struct of tuning dials that the headless contest resolver and the
 * match simulator consult so the whole simulation can be re-balanced WITHOUT
 * recompiling brain logic or touching the physics core. Each dial is a multiplier or
 * a bias with a NEUTRAL value (1.0 for scales, 0.0 for biases); the default-
 * constructed config is therefore the identity transform and reproduces the shipped
 * behaviour BIT-FOR-BIT — the determinism and believability tests still pass at
 * default. Designers move a dial, re-run the validation batch, and read the metric
 * deltas straight out of the analytics report.
 *
 * The dials map onto the brief's six required tuning systems:
 *   - Swing strength      -> SwingStrength
 *   - Spin strength       -> SpinStrength
 *   - Bounce variation    -> BounceVariation
 *   - Bat timing windows  -> BatTimingWindow
 *   - AI aggression       -> BatterAggressionBias / BowlerAggressionBias
 *   - AI risk-taking      -> BatterRiskTaking
 * plus two master scoring/wicket dials (BoundaryScale, GlobalWicketScale) that let a
 * report-driven tune nudge the headline run-rate / wicket-rate without distorting the
 * matchup-specific behaviour the individual dials govern.
 *
 * DESIGN INVARIANT — these never grant the AI privileged information or override an
 * outcome. They scale the SAME physical factors the contest already weighs (movement
 * threat, timing scatter, the batter's intent), exactly as a different pitch or ball
 * would. The AI still chooses; the result still emerges.
 */
USTRUCT(BlueprintType)
struct CRICKETAI_API FCricketBalanceConfig
{
	GENERATED_BODY()

	// --- Bowling movement (the ball off the pitch / in the air) ---------------

	/** Scales the wicket threat lateral movement (swing/seam) generates. 1 = shipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|Bowling", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	double SwingStrength = 1.0;

	/** Scales spin's effect — beating the bat, the stumping chance down the track. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|Bowling", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	double SpinStrength = 1.0;

	/**
	 * Pitch liveliness: scales how much the short ball and an uneven surface punish
	 * the batter (top-edged pulls, mistimed strokes). 1 = a true pitch; >1 = spicy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|Bowling", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	double BounceVariation = 1.0;

	// --- Batting -------------------------------------------------------------

	/**
	 * Bat timing leniency: widens (>1) or tightens (<1) the clean-contact window, so a
	 * larger window middles more balls (more boundaries, fewer mistimed dots) and a
	 * smaller one punishes timing. Tunes the headline scoring feel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|Batting", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	double BatTimingWindow = 1.0;

	/** Additive bias to every batter's intent-to-score tendency [-0.5,0.5]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|AI", meta = (ClampMin = "-0.5", ClampMax = "0.5"))
	double BatterAggressionBias = 0.0;

	/**
	 * Scales how much the risk a batter accepts feeds into the wicket chance. >1 makes
	 * aggressive intent costlier (risk-takers fall more); <1 forgives it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|AI", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	double BatterRiskTaking = 1.0;

	/** Additive bias to every bowler's attacking-vs-containing tendency [-0.5,0.5]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|AI", meta = (ClampMin = "-0.5", ClampMax = "0.5"))
	double BowlerAggressionBias = 0.0;

	// --- Master scoring / wicket dials ---------------------------------------

	/** Master multiplier on every boundary probability. The headline run-rate dial. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|Master", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	double BoundaryScale = 1.0;

	/** Master multiplier on every wicket probability. The headline wicket-rate dial. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance|Master", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	double GlobalWicketScale = 1.0;

	/** Human-readable preset name (carried into reports). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Balance")
	FString PresetName = TEXT("Neutral");

	// --- Helpers --------------------------------------------------------------

	/** Apply the batter-intent bias to a tendency value, clamped to [0,1]. */
	double BiasedAggression(double Base) const { return FMath::Clamp(Base + BatterAggressionBias, 0.0, 1.0); }
	double BiasedBowlerAggression(double Base) const { return FMath::Clamp(Base + BowlerAggressionBias, 0.0, 1.0); }

	/** True if every dial sits at its neutral value (identity transform). */
	bool IsNeutral() const
	{
		return SwingStrength == 1.0 && SpinStrength == 1.0 && BounceVariation == 1.0
			&& BatTimingWindow == 1.0 && BatterAggressionBias == 0.0 && BatterRiskTaking == 1.0
			&& BowlerAggressionBias == 0.0 && BoundaryScale == 1.0 && GlobalWicketScale == 1.0;
	}

	// --- Presets --------------------------------------------------------------
	// The shipped, named tuning points. Conditions double as the "different pitch
	// conditions" axis the validation batch sweeps; the *Friendly presets are the
	// balance directions a tuning pass moves in.

	/** The shipped, neutral balance (identity). */
	static FCricketBalanceConfig Neutral() { return FCricketBalanceConfig(); }

	/** A flat, true batting deck — a road. Bat dominates; high totals. */
	static FCricketBalanceConfig FlatTrack()
	{
		FCricketBalanceConfig B; B.PresetName = TEXT("FlatTrack");
		B.SwingStrength = 0.7; B.BatTimingWindow = 1.15; B.BoundaryScale = 1.12;
		B.GlobalWicketScale = 0.85; B.BounceVariation = 0.8;
		return B;
	}

	/** A green seamer — lateral movement and bounce; tough up top. */
	static FCricketBalanceConfig GreenSeamer()
	{
		FCricketBalanceConfig B; B.PresetName = TEXT("GreenSeamer");
		B.SwingStrength = 1.5; B.BounceVariation = 1.4; B.BoundaryScale = 0.9;
		B.GlobalWicketScale = 1.25;
		return B;
	}

	/** A dry, turning rank-turner — spin bites, scoring is a grind. */
	static FCricketBalanceConfig Turner()
	{
		FCricketBalanceConfig B; B.PresetName = TEXT("Turner");
		B.SpinStrength = 1.7; B.SwingStrength = 0.8; B.BoundaryScale = 0.88;
		B.GlobalWicketScale = 1.2; B.BatTimingWindow = 0.92;
		return B;
	}

	/** A worn day-2 surface — variable bounce, reverse, lower scoring. */
	static FCricketBalanceConfig WornPitch()
	{
		FCricketBalanceConfig B; B.PresetName = TEXT("WornPitch");
		B.SwingStrength = 1.25; B.SpinStrength = 1.35; B.BounceVariation = 1.5;
		B.BatTimingWindow = 0.9; B.BoundaryScale = 0.9; B.GlobalWicketScale = 1.15;
		return B;
	}

	/** The set of conditions the validation batch sweeps (a believable spread). */
	static TArray<FCricketBalanceConfig> ConditionSweep()
	{
		return { Neutral(), FlatTrack(), GreenSeamer(), Turner(), WornPitch() };
	}
};
