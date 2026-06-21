#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CricketTeamKitDataAsset.generated.h"

class UMaterialInterface;
class USkeletalMesh;

/**
 * UCricketTeamKitDataAsset — a team's VISUAL identity for the character pipeline:
 * uniform colours and the material/mesh overrides ACricketCharacter::ApplyTeamKit
 * applies to a shared base character. Deliberately separate from
 * UCricketTeamDataAsset (CricketSim module, gameplay roster) so the rendering-asset
 * references here never reach the core sim/rules layer; the two are joined at
 * runtime by ShortCode, e.g. "IND" / "AUS".
 *
 * One instance per team scales to any number of additional teams without touching
 * C++ or the shared skeleton/animations.
 */
UCLASS(BlueprintType)
class CRICKETGAMEPLAY_API UCricketTeamKitDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Kit")
	FString TeamName;

	/** Join key matching UCricketTeamDataAsset::ShortCode, e.g. "IND", "AUS". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Kit")
	FString ShortCode;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Kit")
	FLinearColor PrimaryColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Kit")
	FLinearColor SecondaryColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Kit")
	FLinearColor TrimColor = FLinearColor::Gray;

	/** Material instance driving the shared kit material's team-colour parameters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Kit")
	TSoftObjectPtr<UMaterialInterface> KitMaterial;

	/** Optional per-team body mesh override; unset means use the archetype default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team Kit")
	TSoftObjectPtr<USkeletalMesh> BodyMeshOverride;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TeamKit"), GetFName());
	}
};
