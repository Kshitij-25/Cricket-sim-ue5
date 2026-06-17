#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketCameraTypes.h"
#include "CricketCameraDirectorComponent.generated.h"

class UCameraComponent;

/**
 * UCricketCameraDirectorComponent — the Camera Manager.
 *
 * Owns the active camera MODE, computes the desired pose each frame from the live
 * subjects via the pure FCricketCameraModel, smoothly BLENDS on mode changes
 * (camera transitions), and applies the result to a UCameraComponent. It only
 * frames the simulation; it never moves anything in it.
 *
 * The owner (a rig/pawn) calls ApplyToCamera(subjects, dt) each tick with the live
 * positions; switching modes, adjusting batting distance/height, and the free/orbit
 * controls are all exposed here.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketCameraDirectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketCameraDirectorComponent();

	/** Camera to drive. If null, the owner's first UCameraComponent is used. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Camera")
	void SetCamera(UCameraComponent* InCamera) { Camera = InCamera; }

	/** Switch mode with a blended transition (seconds). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Camera")
	void SetMode(ECricketCameraMode NewMode, float BlendSeconds = 0.6f);

	UFUNCTION(BlueprintPure, Category = "Cricket|Camera")
	ECricketCameraMode GetMode() const { return Mode; }

	UFUNCTION(BlueprintPure, Category = "Cricket|Camera")
	bool IsBlending() const { return bBlending; }

	/** Cycle among the four gameplay modes (Batting/Bowling/Fielding/Spectator). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Camera")
	void CycleGameplayMode(int32 Dir);

	UFUNCTION(BlueprintCallable, Category = "Cricket|Camera")
	void AdjustDistance(double DeltaCm) { Config.DistanceCm = FMath::Clamp(Config.DistanceCm + DeltaCm, 150.0, 2000.0); }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Camera")
	void AdjustHeight(double DeltaCm) { Config.HeightCm = FMath::Clamp(Config.HeightCm + DeltaCm, 40.0, 1200.0); }

	/** Compute, blend and apply the pose for the current mode. Call once per tick. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Camera")
	void ApplyToCamera(const FCricketCameraSubjects& Subjects, float DeltaSeconds);

	/** The current applied pose (for debug). */
	const FCricketCameraPose& GetCurrentPose() const { return CurrentPose; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Camera")
	FCricketCameraConfig Config;

	/** Tracking lag (higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Camera")
	double LagSpeed = 12.0;

private:
	UPROPERTY() TObjectPtr<UCameraComponent> Camera;

	ECricketCameraMode Mode = ECricketCameraMode::Spectator;
	FCricketCameraPose CurrentPose;
	bool bInitialized = false;

	// Transition state.
	bool bBlending = false;
	float BlendAlpha = 0.f;
	float BlendDuration = 0.6f;
	FCricketCameraPose BlendFrom;
};
