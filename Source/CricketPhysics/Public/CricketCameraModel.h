#pragma once

#include "CoreMinimal.h"
#include "CricketCameraTypes.h"

/**
 * FCricketCameraModel — pure camera framing + transition + physics-visualization
 * math. No UWorld/actor dependency, so the framing logic and the deviation
 * measurements are headless-testable and reused by the gameplay camera director.
 *
 *   - ComputePose: the desired pose for a mode from the subject positions + config.
 *   - Blend:       a smooth transition between two poses (camera transitions).
 *   - Swing/SpinDeviation & MaxLateralDeviation: measure the ACTUAL lateral
 *     movement in a recorded ball path, so the replay can draw the real swing/spin
 *     (not a label) — visualizing the simulation, not asserting it.
 */
class CRICKETPHYSICS_API FCricketCameraModel
{
public:
	/** Desired camera pose for Mode, framing Subjects, tuned by Config. */
	static FCricketCameraPose ComputePose(
		ECricketCameraMode Mode, const FCricketCameraSubjects& Subjects, const FCricketCameraConfig& Config);

	/** Smoothly blend From->To (Alpha 0..1, smoothstepped). Slerps rotation, lerps the rest. */
	static FCricketCameraPose Blend(const FCricketCameraPose& From, const FCricketCameraPose& To, double Alpha);

	// --- Physics visualization (measured from a recorded path, in metres) ---

	/** Max horizontal distance (m) of the path[Start..End] from the straight chord. */
	static double MaxLateralDeviationM(const TArray<FVector>& PathM, int32 StartIdx, int32 EndIdx);

	/** In-flight (pre-bounce) lateral movement = conventional/reverse swing. */
	static double SwingDeviationM(const TArray<FVector>& PathM, int32 BounceIdx);

	/** Post-bounce departure from the incoming line = seam/spin deviation off the pitch. */
	static double SpinDeviationM(const TArray<FVector>& PathM, int32 BounceIdx);
};
