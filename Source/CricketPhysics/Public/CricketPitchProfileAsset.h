#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CricketPitchInteraction.h"
#include "CricketPitchTypes.h"
#include "CricketPitchMaterial.h"
#include "CricketPitchProfileAsset.generated.h"

/**
 * UCricketPitchProfileAsset — a complete, data-driven pitch personality:
 * hard/bouncy (Perth), dry/turning (Chennai), green/seaming (Hobart), dead/low
 * (subcontinent day-5). Sampled per impact by the gameplay layer to feed the
 * pitch-interaction model. Different profiles = different deliverables, no code.
 *
 * The fastest way to author one is to pick a PitchType and call
 * ConfigureFromType() (or the editor "Reset From Type" action), which fills the
 * base patch and zones from FCricketPitchMaterialLibrary. Every field stays
 * hand-editable afterward.
 */
UCLASS(BlueprintType)
class CRICKETPHYSICS_API UCricketPitchProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
	FName ProfileName = TEXT("Balanced");

	/** The personality this profile is built from. Drives ConfigureFromType(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
	ECricketPitchType PitchType = ECricketPitchType::Balanced;

	/** Default surface used wherever no zone applies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
	FCricketSurfacePatch BasePatch;

	/** Length-banded overrides, searched in order; first containing band wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch")
	TArray<FCricketPitchZone> Zones;

	/**
	 * Global day/wear multiplier applied on top of the sampled patch:
	 * 0 = fresh pitch, 1 = heavily worn. Raises unevenness, roughness and grip,
	 * lowers hardness/restitution — a single dial to age the surface over a match.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Wear = 0.0;

	// --- Future-proofing (design only; see CricketPitchTypes.h) -------------
	/** Where in a (multi-day) match we are. ApplyDayProgression() folds it into Wear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch|Future")
	FCricketPitchDayProgression DayProgression;

	/** Footmarks / rough patches. Data is carried but NOT yet applied in sampling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pitch|Future")
	TArray<FCricketFootmark> Footmarks;

	/** Fill BasePatch + Zones (+ ProfileName) from the chosen PitchType. */
	UFUNCTION(BlueprintCallable, Category = "Pitch")
	void ConfigureFromType(ECricketPitchType Type);

	/** Recompute global Wear from the current DayProgression (deterioration hook). */
	UFUNCTION(BlueprintCallable, Category = "Pitch")
	void ApplyDayProgression();

	/** Sample the surface at a length down the pitch (metres from batter's stumps). */
	UFUNCTION(BlueprintCallable, Category = "Pitch")
	FCricketSurfacePatch SamplePatch(double DistanceDownPitchM) const;

#if WITH_EDITOR
	// Re-derive the surface whenever PitchType is changed in the details panel.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("PitchProfile"), GetFName());
	}
};
