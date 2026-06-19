#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CricketPhysicsTypes.h"
#include "CricketPhysicsSettings.generated.h"

/**
 * UCricketPhysicsSettings — project-wide configuration for the ball physics
 * system, surfaced under Project Settings ▸ Game ▸ "Cricket Physics" and saved
 * to DefaultGame.ini. This is the single configuration entry point: global
 * model defaults plus master debug toggles. Per-delivery data still comes from
 * ball/pitch profile assets; this is the fallback + global switch layer.
 *
 * Access from anywhere via GetDefault<UCricketPhysicsSettings>().
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Cricket Physics"))
class CRICKETPHYSICS_API UCricketPhysicsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ---- Model defaults ---------------------------------------------------
	/** Default aerodynamic coefficients used when a ball has no profile. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Model")
	FCricketAeroCoefficients DefaultCoefficients;

	/** Default atmosphere when a level/match supplies none. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Model")
	FCricketEnvironment DefaultEnvironment;

	/** Integration sub-step (seconds). Lower = more accurate, more cost. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Model", meta = (ClampMin = "0.0001", ClampMax = "0.01"))
	double IntegrationSubstep = 0.001;

	// ---- Debug visualization master toggles ------------------------------
	// CVars (cricket.Debug.*) override these at runtime; these are the editor
	// defaults / shipping baseline.
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bEnableDebugByDefault = false;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawVelocity = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawSpinAxis = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawSeam = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawForces = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawActualTrajectory = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawPredictedTrajectory = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDrawBouncePoints = true;

	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bShowReadout = true;

	// ---- Pitch-system debug master toggles (cricket.Debug.Pitch.*) -------
	/** Master switch for the pitch debug overlay (bounce angle/surface/turn/seam). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug|Pitch")
	bool bEnablePitchDebug = false;

	/** Draw incoming vs outgoing arrows at each bounce (the bounce angle). */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug|Pitch")
	bool bDrawPitchBounceAngle = true;

	/** Floating per-bounce readout: restitution, friction, surface props. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug|Pitch")
	bool bDrawPitchSurfaceInfo = true;

	/** Draw the turn (spin) and seam-deviation lateral kicks at each bounce. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug|Pitch")
	bool bDrawPitchTurnSeam = true;

	/** Draw the predicted first-bounce point and its error vs the actual bounce. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug|Pitch")
	bool bDrawPitchPredictedVsActual = true;

	/** Visual scale (cm of arrow per m/s) for the velocity debug arrow. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0.1"))
	float VelocityArrowScale = 2.0f;

	/** Visual scale (cm of arrow per m/s^2) for force/acceleration debug arrows. */
	UPROPERTY(config, EditAnywhere, BlueprintReadOnly, Category = "Debug", meta = (ClampMin = "0.1"))
	float ForceArrowScale = 5.0f;
};
