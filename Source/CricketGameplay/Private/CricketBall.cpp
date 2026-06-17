#include "CricketBall.h"
#include "CricketBallPhysicsComponent.h"
#include "CricketBallDebugComponent.h"
#include "CricketBatDebugComponent.h"
#include "CricketPitchDebugComponent.h"
#include "CricketPhysicsConstants.h"
#include "Components/StaticMeshComponent.h"

ACricketBall::ACricketBall()
{
	PrimaryActorTick.bCanEverTick = false; // the physics component ticks itself

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	// The mesh is cosmetic only; world contact is done via component sweeps.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Scale a unit-sphere mesh (assigned in a derived BP) to the real ball size.
	const float DiameterCm = static_cast<float>(CricketPhysics::BallDiameterM * CricketPhysics::MetersToUE);
	Mesh->SetWorldScale3D(FVector(DiameterCm / 100.0f)); // assumes 100cm source sphere

	BallPhysics = CreateDefaultSubobject<UCricketBallPhysicsComponent>(TEXT("BallPhysics"));
	BallDebug = CreateDefaultSubobject<UCricketBallDebugComponent>(TEXT("BallDebug"));
	BatDebug = CreateDefaultSubobject<UCricketBatDebugComponent>(TEXT("BatDebug"));
	PitchDebug = CreateDefaultSubobject<UCricketPitchDebugComponent>(TEXT("PitchDebug"));
}
