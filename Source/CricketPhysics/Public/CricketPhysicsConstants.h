#pragma once

#include "CoreMinimal.h"

/**
 * CricketPhysicsConstants
 *
 * Single source of truth for physical constants and unit conventions used by
 * the aerodynamics and pitch-interaction models.
 *
 * UNIT CONVENTION
 * ---------------
 * The physics core works entirely in SI: metres, kilograms, seconds, radians.
 * Unreal world space is centimetres (Z-up, left-handed). Conversion happens
 * ONLY at the gameplay boundary (the ball actor) via the helpers below — never
 * inside the model. This keeps every coefficient comparable to published
 * wind-tunnel literature and free of cm/m confusion.
 *
 * AXES (shared with UE world space so no rotation is needed at the boundary)
 *   +X = down the pitch toward the striker (bowling direction)
 *   +Y = to the off side for a right-hander facing the bowler (UE "right")
 *   +Z = up
 * Cross products use UE's left-handed FVector::CrossProduct throughout; the
 * Magnus sign is derived and unit-tested against this convention.
 */
namespace CricketPhysics
{
	// ---- Standard men's cricket ball (MCC Law 4.1) -----------------------
	/** Mass in kg. Law allows 155.9–163.0 g; 156 g is a representative mean. */
	inline constexpr double BallMassKg = 0.156;

	/** Radius in m. Circumference 22.4–22.9 cm => r ~= 0.0359 m. */
	inline constexpr double BallRadiusM = 0.0359;

	/** Reference cross-sectional area pi*r^2 (m^2). */
	inline constexpr double BallCrossSectionM2 = PI * BallRadiusM * BallRadiusM;

	/** Diameter (m), used for Reynolds number. */
	inline constexpr double BallDiameterM = 2.0 * BallRadiusM;

	/** Moment of inertia of a thin-shell-ish ball ~ (2/3) m r^2 (kg*m^2). */
	inline constexpr double BallInertia =
		(2.0 / 3.0) * BallMassKg * BallRadiusM * BallRadiusM;

	// ---- Atmosphere (sea level, 20C, dry) reference ----------------------
	/** Air density (kg/m^3) at the reference condition. Recomputed per env. */
	inline constexpr double AirDensitySeaLevel = 1.225;

	/** Kinematic viscosity of air (m^2/s) ~20C. Drives Reynolds number. */
	inline constexpr double AirKinematicViscosity = 1.5e-5;

	/** Gravity magnitude (m/s^2). */
	inline constexpr double GravityMS2 = 9.81;

	// ---- Reynolds / swing transition regime ------------------------------
	/**
	 * Approximate critical speed (m/s) at which a SMOOTH new-ball boundary
	 * layer transitions to turbulent. Below: conventional swing. Above (and
	 * for a roughened ball): reverse-swing regime. ~30 m/s ~= 108 km/h.
	 * Tunable per ball profile; this is the model default.
	 */
	inline constexpr double ConventionalSwingMaxSpeed = 30.0;

	/** Speed (m/s) above which reverse swing becomes the dominant mode. */
	inline constexpr double ReverseSwingOnsetSpeed = 33.0;

	// ---- Unit conversion at the gameplay boundary ------------------------
	inline constexpr double MetersToUE = 100.0;   // m  -> cm
	inline constexpr double UEToMeters = 0.01;    // cm -> m

	FORCEINLINE FVector MetersToWorld(const FVector& Meters)  { return Meters * MetersToUE; }
	FORCEINLINE FVector WorldToMeters(const FVector& World)   { return World  * UEToMeters; }

	/** km/h <-> m/s, handy for release-speed authoring. */
	FORCEINLINE double KmhToMs(double Kmh) { return Kmh / 3.6; }
	FORCEINLINE double MsToKmh(double Ms)  { return Ms * 3.6; }

	/** rpm <-> rad/s for spin authoring. */
	FORCEINLINE double RpmToRadS(double Rpm) { return Rpm * 2.0 * PI / 60.0; }
	FORCEINLINE double RadSToRpm(double Rad) { return Rad * 60.0 / (2.0 * PI); }
}
