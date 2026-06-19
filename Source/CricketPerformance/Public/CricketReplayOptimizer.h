#pragma once

#include "CoreMinimal.h"
#include "CricketReplayTypes.h"
#include "CricketReplayOptimizer.generated.h"

/** Tunables for the replay optimizer (sourced from UCricketPerformanceSettings). */
USTRUCT(BlueprintType)
struct CRICKETPERFORMANCE_API FCricketReplayOptimizerSettings
{
	GENERATED_BODY()

	/** Quantize ball position to this resolution (mm). 0 = no quantization. */
	UPROPERTY(BlueprintReadWrite, Category = "Cricket|Perf") int32 PositionQuantizeMm = 5;

	/** Drop a frame if the ball moved less than this (m) since the last kept frame. */
	UPROPERTY(BlueprintReadWrite, Category = "Cricket|Perf") float MinMotionM = 0.02f;

	/** Always keep frames within this window (s) of a recorded event. */
	UPROPERTY(BlueprintReadWrite, Category = "Cricket|Perf") float EventGuardSeconds = 0.05f;
};

/**
 * FCricketReplayOptimizationReport — what an Optimize() pass achieved: frame/byte
 * counts before & after, the resulting compression ratio, and the maximum spatial
 * error (m) the lossy steps introduced so quality can be asserted in tests.
 */
USTRUCT(BlueprintType)
struct CRICKETPERFORMANCE_API FCricketReplayOptimizationReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int32 OriginalFrames = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int32 OptimizedFrames = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int64 OriginalBytes = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") int64 OptimizedBytes = 0;
	/** OptimizedBytes / OriginalBytes (lower is better; 1.0 = no saving). */
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float CompressionRatio = 1.0f;
	/** Largest ball-position deviation (m) introduced by decimation + quantization. */
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float MaxPositionErrorM = 0.0f;
	UPROPERTY(BlueprintReadOnly, Category = "Cricket|Perf") float SavedMB = 0.0f;
};

/**
 * FCricketReplayOptimizer — the Replay Optimization Layer.
 *
 * The recorder stores a dense, fixed-rate ring of frames; that is simple but a
 * 2-minute clip at 60 Hz holds 7200 frames, most of which are nearly redundant
 * (a stationary fielder, a ball arcing predictably between samples). This layer
 * losslessly-where-it-can / lossily-within-tolerance shrinks a finished clip:
 *
 *   1. ADAPTIVE SAMPLING — drop frames where nothing moved enough to matter,
 *      while always preserving the frames around recorded events (bounce/impact).
 *      Playback already interpolates (FCricketReplayClip::SampleAtTime), so the
 *      dropped frames are reconstructed within the configured tolerance.
 *   2. QUANTIZATION — snap positions to a millimetre grid, which both bounds error
 *      and (in a real serializer) makes the stream far more compressible.
 *
 * Pure and deterministic: same clip + settings → same result, so the savings and
 * the worst-case error are unit-testable. It never re-simulates.
 */
class CRICKETPERFORMANCE_API FCricketReplayOptimizer
{
public:
	/** In-memory footprint of a clip (frame structs + heap actor/event arrays). */
	static int64 EstimateClipBytes(const FCricketReplayClip& Clip);

	/** Produce an optimized copy of In into Out and return what was achieved. */
	static FCricketReplayOptimizationReport Optimize(
		const FCricketReplayClip& In,
		const FCricketReplayOptimizerSettings& Settings,
		FCricketReplayClip& Out);
};
