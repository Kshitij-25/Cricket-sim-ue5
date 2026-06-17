#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CricketBowlingTypes.h"
#include "CricketBowlingActionAsset.generated.h"

/**
 * UCricketBowlingActionAsset — a designer-authored bowler "action": the physical
 * envelope (release height/width, arm slot, pace range, spin capacity) plus the
 * named deliveries that bowler offers. Decouples bowler personalities from code,
 * exactly like UCricketBallProfileAsset does for the ball.
 *
 * The static factories build representative actions in code so the system runs
 * with zero authored assets (used by the training rig and the headless tests).
 */
UCLASS(BlueprintType)
class CRICKETPHYSICS_API UCricketBowlingActionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UCricketBowlingActionAsset();

	/** The bowler's physical action + preset deliveries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bowling")
	FCricketBowlingAction Action;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BowlingAction"), GetFName());
	}

	// --- Built-in actions (no asset authoring required) ----------------------

	/** An express seam bowler: ~130–150 km/h, yorker/length/short/bouncer presets. */
	static FCricketBowlingAction MakeExpressQuick();

	/** A swing specialist: ~125–142 km/h, out/in/reverse/wobble presets. */
	static FCricketBowlingAction MakeSwingBowler();

	/** A finger spinner: ~80–95 km/h, big revs, off-break presets. */
	static FCricketBowlingAction MakeOffSpinner();

	/** A wrist spinner: ~78–92 km/h, big revs, leg-break presets. */
	static FCricketBowlingAction MakeLegSpinner();
};
