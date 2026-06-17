#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CricketPhysicsTypes.h"
#include "CricketBallProfileAsset.generated.h"

/**
 * UCricketBallProfileAsset — designer-authored aerodynamic + surface preset for
 * a ball "personality": new Kookaburra, old reversing SG, dewy white ball, etc.
 * Decouples tuning from code; selectable per innings / per over from data.
 */
UCLASS(BlueprintType)
class CRICKETPHYSICS_API UCricketBallProfileAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ball")
	FName ProfileName = TEXT("New Ball");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ball")
	ECricketDeliveryArchetype DefaultArchetype = ECricketDeliveryArchetype::SeamUp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ball")
	FCricketAeroCoefficients Coefficients;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ball")
	FCricketBallSurface InitialSurface;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("BallProfile"), GetFName());
	}
};
