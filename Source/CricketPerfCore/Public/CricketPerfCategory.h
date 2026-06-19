#pragma once

#include "CoreMinimal.h"
#include "CricketPerfCategory.generated.h"

/**
 * ECricketPerfCategory — the simulation cost buckets the framework profiles and
 * budgets independently. These mirror the engineering subsystems of the sim so a
 * frame-time spike can be attributed to a specific area (physics vs AI vs render).
 *
 * Frame/GameThread/RenderThread/GPU are whole-frame measures sampled from the
 * engine; the rest are gameplay scopes accumulated via CRICKET_PERF_SCOPE.
 *
 * Count MUST stay last — it sizes the per-category arrays.
 */
UENUM(BlueprintType)
enum class ECricketPerfCategory : uint8
{
	Frame        UMETA(DisplayName = "Frame"),          // whole-frame wall time
	GameThread   UMETA(DisplayName = "Game Thread"),    // engine game-thread time
	RenderThread UMETA(DisplayName = "Render Thread"),  // engine render-thread time
	GPU          UMETA(DisplayName = "GPU"),            // engine GPU frame time
	Physics      UMETA(DisplayName = "Ball Physics"),   // integrator + contact
	Prediction   UMETA(DisplayName = "Prediction"),     // trajectory/fielding forecast
	AI           UMETA(DisplayName = "AI"),             // batter/bowler/captain/fielding brains
	Animation    UMETA(DisplayName = "Animation"),      // state machines + character updates
	Replay       UMETA(DisplayName = "Replay"),         // recording capture + playback
	Other        UMETA(DisplayName = "Other"),
	Count         UMETA(Hidden)
};

/** Number of real categories (excludes the Count sentinel). */
static constexpr int32 CricketPerfCategoryCount = static_cast<int32>(ECricketPerfCategory::Count);

/** Human label for a category (UI/logging). */
CRICKETPERFCORE_API const TCHAR* LexToString(ECricketPerfCategory Category);
