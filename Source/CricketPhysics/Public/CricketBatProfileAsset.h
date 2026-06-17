#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CricketBatTypes.h"
#include "CricketBatProfileAsset.generated.h"

/**
 * UCricketBatProfileAsset — a data-driven bat: a heavy power bat vs a light
 * faster-swinging blade, a big sweet spot vs a punishing one. Decouples bat
 * tuning from code, selectable per batter.
 */
UCLASS(BlueprintType)
class CRICKETPHYSICS_API UCricketBatProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat")
	FName ProfileName = TEXT("Standard");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bat")
	FCricketBatProfile Profile;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BatProfile"), GetFName());
	}
};
