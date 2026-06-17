#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CricketPhysicsTypes.h"
#include "CricketBallIntegrator.h"
#include "CricketPitchInteraction.h"
#include "CricketAerodynamics.h"
#include "CricketBatTypes.h"
#include "CricketBallPhysicsComponent.generated.h"

class UCricketBallProfileAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketBallBounce, FCricketBounceReport, Report);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCricketBatImpact, FCricketBatImpactReport, Report);

/**
 * UCricketBallPhysicsComponent
 *
 * The gameplay-side driver for the deterministic ball physics core. It:
 *   - owns the SI FCricketBallState and the RK4 integrator
 *   - advances flight each tick and mirrors the result onto the owning actor's
 *     world transform (SI metres -> UE centimetres at this boundary only)
 *   - sweeps the world for pitch/ground contact and routes bounces through
 *     FCricketPitchInteraction, sampling the surface where the ball lands
 *
 * Chaos is NOT used for ball flight (we need a controllable aerodynamic field
 * and determinism). Chaos/world collision is used only to find contact points.
 */
UCLASS(ClassGroup = (Cricket), meta = (BlueprintSpawnableComponent))
class CRICKETGAMEPLAY_API UCricketBallPhysicsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCricketBallPhysicsComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Release the ball: set SI state from a world-space launch (cm) + spin/seam. */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Ball")
	void Release(const FVector& WorldPositionCm, const FVector& VelocityMS,
		const FVector& AngularVelocityRadS, const FVector& SeamNormal);

	/**
	 * Full-control release used by the bowling system. Identical to Release() but
	 * also seeds the initial seam stability (low => wobble seam). The surface,
	 * coefficients and environment currently set on this component are used — the
	 * bowling controller writes them (via the delivery's ball condition/coeffs)
	 * immediately before calling this.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Ball")
	void ReleaseEx(const FVector& WorldPositionCm, const FVector& VelocityMS,
		const FVector& AngularVelocityRadS, const FVector& SeamNormal, double SeamStability);

	/** Stop integrating (ball is dead / caught / settled). */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Ball")
	void Freeze() { bActive = false; }

	UFUNCTION(BlueprintCallable, Category = "Cricket|Ball")
	void ApplyProfile(const UCricketBallProfileAsset* Profile);

	UFUNCTION(BlueprintPure, Category = "Cricket|Ball")
	FVector GetVelocityMS() const { return State.Velocity; }

	UFUNCTION(BlueprintPure, Category = "Cricket|Ball")
	float GetSpeedKmh() const;

	UFUNCTION(BlueprintPure, Category = "Cricket|Ball")
	float GetSpinRPM() const;

	UFUNCTION(BlueprintPure, Category = "Cricket|Ball")
	bool IsBallInFlight() const { return bActive; }

	/** Read-only access to the live SI state (position in metres). */
	const FCricketBallState& GetState() const { return State; }

	/** Most recent aerodynamic breakdown (forces, coefficients, regime) for debug. */
	const FCricketAeroResult& GetLastAero() const { return LastAero; }

	/** Access the integrator (e.g. to seed a trajectory prediction with the same model). */
	const FCricketBallIntegrator& GetIntegrator() const { return Integrator; }

	/** Fired after each resolved bounce. */
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Ball")
	FOnCricketBallBounce OnBounce;

	/** Fired after a bat-ball impact is resolved. */
	UPROPERTY(BlueprintAssignable, Category = "Cricket|Bat")
	FOnCricketBatImpact OnBatImpact;

	/**
	 * Resolve a bat impact against the live ball state and launch the ball off
	 * the bat. ContactPointWorldCm is the world contact point (cm). Returns the
	 * deterministic impact report.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bat")
	FCricketBatImpactReport ApplyBatImpact(const FCricketBatState& Bat,
		const FCricketBatProfile& BatProfile, const FVector& ContactPointWorldCm);

	/**
	 * Generate a shot from intent and resolve it against the live ball, launching
	 * the ball off the bat. The bat is positioned at the current ball location.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cricket|Bat")
	FCricketBatImpactReport PlayShot(const FCricketShotIntent& Intent,
		const FCricketBatProfile& BatProfile);

	/** Last bat-impact report (for debug/telemetry). */
	const FCricketBatImpactReport& GetLastBatImpact() const { return LastBatImpact; }

	/** Bat kinematics from the last impact (for debug visualization). */
	const FCricketBatState& GetLastBatState() const { return LastBatState; }

	/** World-space contact point (cm) of the last impact. */
	FVector GetLastBatContactCm() const { return LastBatContactCm; }

	// --- Authoring defaults (overridden by a profile / release call) ----------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Ball")
	FCricketAeroCoefficients Coefficients;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Ball")
	FCricketBallSurface Surface;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Ball")
	FCricketEnvironment Environment;

	/** Pitch surface used when a bounce is detected (sampled per impact later). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Pitch")
	FCricketSurfacePatch PitchSurface;

	/** Collision channel used to sweep for pitch/ground contact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cricket|Pitch")
	TEnumAsByte<ECollisionChannel> ContactChannel = ECC_WorldStatic;

protected:
	virtual void BeginPlay() override;

private:
	/** Sweep from previous to current position; resolve a bounce if we hit. */
	void HandlePitchContact(const FVector& PrevPosM, FVector& InOutPosM);

	/** Hash a world position into a deterministic [-1,1] bounce variance. */
	static double DeterministicVariance(const FVector& PosM);

	FCricketBallState State;
	FCricketBallIntegrator Integrator;
	FCricketAeroResult LastAero;
	FCricketBatImpactReport LastBatImpact;
	FCricketBatState LastBatState;
	FVector LastBatContactCm = FVector::ZeroVector;
	bool bActive = false;
};
