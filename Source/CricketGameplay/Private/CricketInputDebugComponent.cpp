#include "CricketInputDebugComponent.h"
#include "CricketPlayerInputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

namespace
{
	TAutoConsoleVariable<int32> CVarInputDebug(TEXT("cricket.Debug.Input"), 1,
		TEXT("Player control / intent debug visualization. 0=off, 1=on"));

	template <typename TEnum>
	FString EnumName(TEnum Value)
	{
		const UEnum* E = StaticEnum<TEnum>();
		return E ? E->GetDisplayNameTextByValue((int64)Value).ToString() : TEXT("?");
	}
}

UCricketInputDebugComponent::UCricketInputDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCricketInputDebugComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Input = Owner->FindComponentByClass<UCricketPlayerInputComponent>();
	}
}

void UCricketInputDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Input || !GEngine || CVarInputDebug.GetValueOnGameThread() == 0) { return; }

	const auto& BS = Input->GetBattingState();
	const auto& SI = Input->GetLastShotIntent();

	GEngine->AddOnScreenDebugMessage(10000, 0.f, FColor::Yellow,
		FString::Printf(TEXT("=== INPUT === active context: %s"), *EnumName(Input->GetContext())));

	GEngine->AddOnScreenDebugMessage(10001, 0.f, FColor::Green,
		FString::Printf(TEXT("Batting held: %s%s%s%s  dir=%s"),
			BS.bFrontFoot ? TEXT("[Front] ") : TEXT(""),
			BS.bBackFoot ? TEXT("[Back] ") : TEXT(""),
			BS.bDefensive ? TEXT("[Defend] ") : TEXT(""),
			BS.bLofted ? TEXT("[Loft] ") : TEXT(""),
			*EnumName(BS.Direction)));

	if (Input->HasPlayedShot())
	{
		GEngine->AddOnScreenDebugMessage(10002, 0.f, FColor::Cyan,
			FString::Printf(TEXT("Shot intent: %s  foot=%s  loft=%d  aim %+.0f  power %.0f%%"),
				*EnumName(SI.Shot), *EnumName(SI.Footwork), SI.bLofted ? 1 : 0, SI.AimYawDeg, SI.PowerScale * 100.0));
	}

	const auto& BWS = Input->GetBowlingState();
	GEngine->AddOnScreenDebugMessage(10003, 0.f, FColor(0, 200, 255),
		FString::Printf(TEXT("Bowling intent: %s  line=%+d length=%+d swing=%d spin=%d"),
			*EnumName(BWS.Delivery), BWS.LineStep, BWS.LengthStep, BWS.bSwingMod ? 1 : 0, BWS.bSpinMod ? 1 : 0));

	GEngine->AddOnScreenDebugMessage(10004, 0.f, FColor::White,
		FString::Printf(TEXT("Running: %s   |   Fielding: %s"),
			*EnumName(Input->GetLastRunCall()), *EnumName(Input->GetLastFieldAction())));

	GEngine->AddOnScreenDebugMessage(10005, 0.f, FColor::Silver,
		TEXT("Bat: D/W=foot S=defend Shift=loft arrows/Q/E=dir Space=play | C/B/F=camera V=replay"));
}
