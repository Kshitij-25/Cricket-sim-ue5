#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CricketPlayerArchetype.h"
#include "CricketCharacter.generated.h"

class UCricketCharacterAnimComponent;
class UCricketTeamKitDataAsset;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UStaticMesh;

/**
 * FCricketCosmeticAttachment — one piece of gear (cap, helmet, pads, bat, ...)
 * attached to a named skeleton socket. A data-driven array rather than fixed
 * UPROPERTY slots so new gear/teams/custom players never require a C++ change.
 */
USTRUCT(BlueprintType)
struct FCricketCosmeticAttachment
{
	GENERATED_BODY()

	/** Skeleton socket to attach to, e.g. "socket_head", "socket_bat_grip". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic")
	TSoftObjectPtr<UStaticMesh> Mesh;
};

/**
 * ACricketCharacter — the shared base for every on-field human in the simulation
 * (batter, bowler, fielder; India, Australia, or any future team/custom player).
 *
 * One skeletal mesh + one shared skeleton drives every archetype and every team:
 *   - Archetype (ECricketPlayerArchetype) selects the presentation-layer Animation
 *     Layer (idle stance, run-up style) an Anim Blueprint links via
 *     OnArchetypeChanged; it is NOT a separate body or skeleton.
 *   - Team identity (UCricketTeamKitDataAsset) is a material + colour swap via
 *     ApplyTeamKit, never a separate mesh, so adding a team is data-only.
 *   - Gear (bat, cap, pads, helmet) is a list of socket-attached cosmetic meshes
 *     (ApplyCosmeticLoadout), so career-mode custom-player loadouts are data too.
 *
 * The actual locomotion/action state is still derived entirely by the existing
 * UCricketCharacterAnimComponent from the physics-driving components on this
 * actor (UCricketBattingComponent / UCricketFielderComponent / UCricketBowlingComponent,
 * found via FindComponentByClass) — this class only adds the VISUAL layer on top.
 */
UCLASS()
class CRICKETGAMEPLAY_API ACricketCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACricketCharacter();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "Cricket|Character")
	ECricketPlayerArchetype GetArchetype() const { return Archetype; }

	/** Switches presentation archetype; fires OnArchetypeChanged for the Anim Blueprint to re-link its Anim Layer. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Character")
	void SetArchetype(ECricketPlayerArchetype NewArchetype);

	/** Applies a team's colours/material to the shared body mesh. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Character")
	void ApplyTeamKit(UCricketTeamKitDataAsset* Kit);

	/** Replaces all socket-attached cosmetic gear (bat, cap, pads, ...). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Character")
	void ApplyCosmeticLoadout(const TArray<FCricketCosmeticAttachment>& NewLoadout);

	UCricketCharacterAnimComponent* GetAnimController() const { return AnimController.Get(); }

	/** Fired when SetArchetype runs; Blueprint Anim instances bind here to call Link Anim Class Layers. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Cricket|Character")
	void OnArchetypeChanged(ECricketPlayerArchetype NewArchetype);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cricket|Character")
	ECricketPlayerArchetype Archetype = ECricketPlayerArchetype::Fielder;

	/** Default kit material to instance when no UCricketTeamKitDataAsset is supplied yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Character")
	TSoftObjectPtr<UMaterialInterface> DefaultKitMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Character")
	TArray<FCricketCosmeticAttachment> DefaultLoadout;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cricket|Character")
	TObjectPtr<UCricketCharacterAnimComponent> AnimController;

private:
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> KitMID;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> CosmeticComponents;

	void ClearCosmeticLoadout();
};
