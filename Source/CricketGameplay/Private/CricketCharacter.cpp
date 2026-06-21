#include "CricketCharacter.h"
#include "CricketCharacterAnimComponent.h"
#include "CricketTeamKitDataAsset.h"
#include "CricketBattingComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

ACricketCharacter::ACricketCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetMesh()->SetCollisionProfileName(TEXT("CharacterMesh"));

	AnimController = CreateDefaultSubobject<UCricketCharacterAnimComponent>(TEXT("AnimController"));
}

void ACricketCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (DefaultLoadout.Num() > 0)
	{
		ApplyCosmeticLoadout(DefaultLoadout);
	}

	if (!KitMID && !DefaultKitMaterial.IsNull())
	{
		if (UMaterialInterface* Base = DefaultKitMaterial.LoadSynchronous())
		{
			KitMID = UMaterialInstanceDynamic::Create(Base, this);
			GetMesh()->SetMaterial(0, KitMID);
		}
	}
}

void ACricketCharacter::SetArchetype(ECricketPlayerArchetype NewArchetype)
{
	if (Archetype == NewArchetype)
	{
		return;
	}

	Archetype = NewArchetype;
	OnArchetypeChanged(Archetype);
}

void ACricketCharacter::ApplyTeamKit(UCricketTeamKitDataAsset* Kit)
{
	if (!Kit)
	{
		return;
	}

	UMaterialInterface* Base = !Kit->KitMaterial.IsNull() ? Kit->KitMaterial.LoadSynchronous() : DefaultKitMaterial.LoadSynchronous();
	if (!Base)
	{
		return;
	}

	KitMID = UMaterialInstanceDynamic::Create(Base, this);
	KitMID->SetVectorParameterValue(TEXT("PrimaryColor"), Kit->PrimaryColor);
	KitMID->SetVectorParameterValue(TEXT("SecondaryColor"), Kit->SecondaryColor);
	KitMID->SetVectorParameterValue(TEXT("TrimColor"), Kit->TrimColor);
	GetMesh()->SetMaterial(0, KitMID);

	if (USkeletalMesh* BodyOverride = Kit->BodyMeshOverride.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(BodyOverride);
		GetMesh()->SetMaterial(0, KitMID);
	}
}

void ACricketCharacter::ApplyCosmeticLoadout(const TArray<FCricketCosmeticAttachment>& NewLoadout)
{
	ClearCosmeticLoadout();

	for (const FCricketCosmeticAttachment& Attachment : NewLoadout)
	{
		UStaticMesh* Mesh = Attachment.Mesh.LoadSynchronous();
		if (!Mesh || Attachment.SocketName.IsNone())
		{
			continue;
		}

		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Mesh);
		Component->RegisterComponent();
		Component->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Attachment.SocketName);
		CosmeticComponents.Add(Component);

		if (UCricketBattingComponent* Batting = FindComponentByClass<UCricketBattingComponent>())
		{
			if (Attachment.SocketName == TEXT("socket_bat_grip"))
			{
				Batting->SetBatVisual(Component);
			}
		}
	}
}

void ACricketCharacter::ClearCosmeticLoadout()
{
	for (UStaticMeshComponent* Component : CosmeticComponents)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	CosmeticComponents.Reset();
}
