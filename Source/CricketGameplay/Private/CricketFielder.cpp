#include "CricketFielder.h"
#include "CricketFielderComponent.h"
#include "CricketFielderDebugComponent.h"
#include "CricketCharacterAnimComponent.h"
#include "CricketAnimDebugComponent.h"

ACricketFielder::ACricketFielder()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Fielder = CreateDefaultSubobject<UCricketFielderComponent>(TEXT("Fielder"));
	FielderDebug = CreateDefaultSubobject<UCricketFielderDebugComponent>(TEXT("FielderDebug"));

	// Follows the fielder's state machine: derives Run/Catch/Pickup/Throw anim
	// state and fires CatchAttempt/PickupContact/ThrowRelease exactly on cue.
	Anim = CreateDefaultSubobject<UCricketCharacterAnimComponent>(TEXT("Anim"));
	AnimDebug = CreateDefaultSubobject<UCricketAnimDebugComponent>(TEXT("AnimDebug"));
}
