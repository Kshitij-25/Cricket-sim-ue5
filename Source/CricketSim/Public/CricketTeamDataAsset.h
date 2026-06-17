#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CricketTeamDataAsset.generated.h"

/** Player roles relevant to MVP gameplay/AI selection. */
UENUM(BlueprintType)
enum class ECricketRole : uint8
{
	BatterTop      UMETA(DisplayName = "Top-order Batter"),
	BatterMiddle   UMETA(DisplayName = "Middle-order Batter"),
	AllRounder     UMETA(DisplayName = "All-rounder"),
	PaceBowler     UMETA(DisplayName = "Pace Bowler"),
	SpinBowler     UMETA(DisplayName = "Spin Bowler"),
	WicketKeeper   UMETA(DisplayName = "Wicket-keeper")
};

/**
 * FCricketPlayer — MVP player record. Ratings bias the physics inputs (release
 * speed, spin rpm, seam consistency) and AI decisions, never the OUTCOME, which
 * remains emergent from the ball physics.
 */
USTRUCT(BlueprintType)
struct CRICKETSIM_API FCricketPlayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	ECricketRole Role = ECricketRole::BatterMiddle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Batting = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Bowling = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Fielding = 0.5f;

	/** Typical release pace (km/h) for a bowler; 0 for a non-bowler. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player")
	float PaceKmh = 0.f;
};

/**
 * UCricketTeamDataAsset — a team roster. MVP ships India and Australia as two
 * instances of this asset under /Game/Data/Teams.
 */
UCLASS(BlueprintType)
class CRICKETSIM_API UCricketTeamDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FString TeamName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	FString ShortCode; // "IND", "AUS"

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Team")
	TArray<FCricketPlayer> Players;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("TeamData"), GetFName());
	}
};
