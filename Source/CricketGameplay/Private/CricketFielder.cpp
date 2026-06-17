#include "CricketFielder.h"
#include "CricketFielderComponent.h"
#include "CricketFielderDebugComponent.h"

ACricketFielder::ACricketFielder()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Fielder = CreateDefaultSubobject<UCricketFielderComponent>(TEXT("Fielder"));
	FielderDebug = CreateDefaultSubobject<UCricketFielderDebugComponent>(TEXT("FielderDebug"));
}
