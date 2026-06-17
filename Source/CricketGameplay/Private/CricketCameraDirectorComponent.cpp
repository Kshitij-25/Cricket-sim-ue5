#include "CricketCameraDirectorComponent.h"
#include "CricketCameraModel.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Actor.h"

UCricketCameraDirectorComponent::UCricketCameraDirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // driven by the owner via ApplyToCamera
}

void UCricketCameraDirectorComponent::SetMode(ECricketCameraMode NewMode, float BlendSeconds)
{
	if (NewMode == Mode && bInitialized) { return; }
	BlendFrom = CurrentPose;
	Mode = NewMode;
	bBlending = bInitialized && BlendSeconds > KINDA_SMALL_NUMBER;
	BlendAlpha = 0.f;
	BlendDuration = FMath::Max(BlendSeconds, 0.01f);
}

void UCricketCameraDirectorComponent::CycleGameplayMode(int32 Dir)
{
	static const ECricketCameraMode Gameplay[] = {
		ECricketCameraMode::Batting, ECricketCameraMode::Bowling,
		ECricketCameraMode::Fielding, ECricketCameraMode::Spectator };
	const int32 Count = UE_ARRAY_COUNT(Gameplay);
	int32 Cur = 0;
	for (int32 i = 0; i < Count; ++i) { if (Gameplay[i] == Mode) { Cur = i; break; } }
	SetMode(Gameplay[((Cur + Dir) % Count + Count) % Count]);
}

void UCricketCameraDirectorComponent::ApplyToCamera(const FCricketCameraSubjects& Subjects, float DeltaSeconds)
{
	if (!Camera)
	{
		if (AActor* Owner = GetOwner()) { Camera = Owner->FindComponentByClass<UCameraComponent>(); }
		if (!Camera) { return; }
	}

	const FCricketCameraPose Target = FCricketCameraModel::ComputePose(Mode, Subjects, Config);

	FCricketCameraPose Desired;
	if (bBlending)
	{
		BlendAlpha += DeltaSeconds / BlendDuration;
		Desired = FCricketCameraModel::Blend(BlendFrom, Target, BlendAlpha);
		if (BlendAlpha >= 1.f) { bBlending = false; }
	}
	else
	{
		Desired = Target;
	}

	if (!bInitialized)
	{
		CurrentPose = Desired; // snap on the first frame
		bInitialized = true;
	}
	else if (bBlending)
	{
		CurrentPose = Desired; // the blend is already smooth
	}
	else
	{
		// Mild positional/rotational lag for smooth tracking.
		const double A = 1.0 - FMath::Exp(-LagSpeed * FMath::Max((double)DeltaSeconds, 0.0));
		CurrentPose.LocationCm = FMath::Lerp(CurrentPose.LocationCm, Desired.LocationCm, A);
		CurrentPose.Rotation = FMath::Lerp(CurrentPose.Rotation, Desired.Rotation, (float)A);
		CurrentPose.FOVDeg = FMath::Lerp(CurrentPose.FOVDeg, Desired.FOVDeg, A);
	}

	Camera->SetWorldLocationAndRotation(CurrentPose.LocationCm, CurrentPose.Rotation);
	Camera->SetFieldOfView((float)CurrentPose.FOVDeg);
}
