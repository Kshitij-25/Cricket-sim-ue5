#include "CricketBallPhysicsComponent.h"
#include "CricketBallProfileAsset.h"
#include "CricketPhysicsConstants.h"
#include "CricketBatCollision.h"
#include "CricketShotGenerator.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

using namespace CricketPhysics;

UCricketBallPhysicsComponent::UCricketBallPhysicsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCricketBallPhysicsComponent::BeginPlay()
{
	Super::BeginPlay();
	Integrator.SetCoefficients(Coefficients);
	Integrator.SetSurface(Surface);
	Integrator.SetEnvironment(Environment);
}

void UCricketBallPhysicsComponent::ApplyProfile(const UCricketBallProfileAsset* Profile)
{
	if (!Profile)
	{
		return;
	}
	Coefficients = Profile->Coefficients;
	Surface = Profile->InitialSurface;
	Integrator.SetCoefficients(Coefficients);
	Integrator.SetSurface(Surface);
}

void UCricketBallPhysicsComponent::Release(const FVector& WorldPositionCm, const FVector& VelocityMS,
	const FVector& AngularVelocityRadS, const FVector& SeamNormal)
{
	// Default release: a fully-held, gyroscopically stable seam.
	ReleaseEx(WorldPositionCm, VelocityMS, AngularVelocityRadS, SeamNormal, 1.0);
}

void UCricketBallPhysicsComponent::ReleaseEx(const FVector& WorldPositionCm, const FVector& VelocityMS,
	const FVector& AngularVelocityRadS, const FVector& SeamNormal, double SeamStability)
{
	State = FCricketBallState();
	State.Position = WorldToMeters(WorldPositionCm);
	State.Velocity = VelocityMS;
	State.AngularVelocity = AngularVelocityRadS;
	State.SeamNormal = SeamNormal.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 1, 0));
	State.SeamStability = FMath::Clamp(SeamStability, 0.0, 1.0);

	// Push the latest authoring state into the integrator at release.
	Integrator.SetCoefficients(Coefficients);
	Integrator.SetSurface(Surface);
	Integrator.SetEnvironment(Environment);

	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocation(WorldPositionCm);
	}
	bActive = true;

	// Refresh the cached aero breakdown so debug/telemetry is valid from frame 0.
	LastAero = FCricketAerodynamics::Evaluate(State, Integrator.GetSurface(),
		Integrator.GetEnvironment(), Integrator.GetCoefficients());
}

float UCricketBallPhysicsComponent::GetSpeedKmh() const
{
	return static_cast<float>(MsToKmh(State.Speed()));
}

float UCricketBallPhysicsComponent::GetSpinRPM() const
{
	return static_cast<float>(RadSToRpm(State.SpinRateRadS()));
}

FCricketBatImpactReport UCricketBallPhysicsComponent::ApplyBatImpact(const FCricketBatState& Bat,
	const FCricketBatProfile& BatProfile, const FVector& ContactPointWorldCm)
{
	// The collision solver works in SI; convert the world contact point to metres.
	// The bat state is already authored in SI world axes by the caller/generator.
	const FVector ContactM = WorldToMeters(ContactPointWorldCm);

	FCricketBallState BallOut;
	FCricketBatImpactReport Report;
	if (FCricketBatCollision::Resolve(State, Bat, BatProfile, ContactM, BallOut, Report))
	{
		State = BallOut;          // the ball is now in flight off the bat
		bActive = true;
		LastBatImpact = Report;
		LastBatState = Bat;
		LastBatState.Orthonormalize();
		LastBatContactCm = ContactPointWorldCm;

		// Refresh the cached aero breakdown and snap the actor to the contact point.
		LastAero = FCricketAerodynamics::Evaluate(State, Integrator.GetSurface(),
			Integrator.GetEnvironment(), Integrator.GetCoefficients());
		if (AActor* Owner = GetOwner())
		{
			Owner->SetActorLocation(MetersToWorld(State.Position), /*bSweep*/ false);
		}
		OnBatImpact.Broadcast(Report);
	}
	return Report;
}

FCricketBatImpactReport UCricketBallPhysicsComponent::PlayShot(const FCricketShotIntent& Intent,
	const FCricketBatProfile& BatProfile)
{
	FCricketBatState Bat;
	FVector ContactM;
	FCricketShotGenerator::GenerateBatState(Intent, State, BatProfile, Bat, ContactM);
	return ApplyBatImpact(Bat, BatProfile, MetersToWorld(ContactM));
}

void UCricketBallPhysicsComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bActive)
	{
		return;
	}

	const FVector PrevPosM = State.Position;
	Integrator.Advance(State, DeltaTime);

	FVector PosM = State.Position;
	HandlePitchContact(PrevPosM, PosM);
	State.Position = PosM;

	// Cache the current aerodynamic breakdown for debug/telemetry consumers.
	LastAero = FCricketAerodynamics::Evaluate(State, Integrator.GetSurface(),
		Integrator.GetEnvironment(), Integrator.GetCoefficients());

	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocation(MetersToWorld(State.Position), /*bSweep*/ false);
	}
}

void UCricketBallPhysicsComponent::HandlePitchContact(const FVector& PrevPosM, FVector& InOutPosM)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float RadiusCm = static_cast<float>(BallRadiusM * MetersToUE);
	const FVector StartCm = MetersToWorld(PrevPosM);
	const FVector EndCm = MetersToWorld(InOutPosM);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CricketBallSweep), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(GetOwner());

	const bool bHit = World->SweepSingleByChannel(
		Hit, StartCm, EndCm, FQuat::Identity, ContactChannel,
		FCollisionShape::MakeSphere(RadiusCm), Params);

	if (!bHit)
	{
		return;
	}

	// Move the ball to the contact point (SI) and resolve the bounce there.
	const FVector ContactPosM = WorldToMeters(Hit.Location);
	State.Position = ContactPosM;

	FCricketImpact Impact;
	Impact.ContactNormal = Hit.ImpactNormal.GetSafeNormal(KINDA_SMALL_NUMBER, FVector(0, 0, 1));
	Impact.Variance = DeterministicVariance(ContactPosM);

	// How flush is the seam to the surface? 1 when the seam plane is parallel to
	// the pitch (seam strikes), 0 when the seam normal lies in the pitch plane.
	const double SeamDotN = FMath::Abs(FVector::DotProduct(
		State.SeamNormal.GetSafeNormal(), Impact.ContactNormal));
	Impact.SeamContact = SeamDotN; // crude but emergent; refined with seam-up detection later

	const FCricketBounceReport Report =
		FCricketPitchInteraction::ResolveBounce(State, PitchSurface, Impact);

	OnBounce.Broadcast(Report);

	InOutPosM = State.Position;
}

double UCricketBallPhysicsComponent::DeterministicVariance(const FVector& PosM)
{
	// Hash the cm-quantised landing position to a stable value in [-1,1] so the
	// same spot always behaves the same way (repeatable bounce variation).
	const int32 X = FMath::RoundToInt(PosM.X * 100.0);
	const int32 Y = FMath::RoundToInt(PosM.Y * 100.0);
	uint32 H = HashCombine(GetTypeHash(X), GetTypeHash(Y));
	// Map to [-1,1].
	return (static_cast<double>(H % 20001) / 10000.0) - 1.0;
}
