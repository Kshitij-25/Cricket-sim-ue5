#pragma once

#include "CoreMinimal.h"
#include "CricketPitchTypes.h"

/**
 * FCricketPitchMaterialLibrary — the SURFACE MATERIAL SYSTEM.
 *
 * The single source of truth for how each ECricketPitchType maps to physical
 * surface parameters. Code and data assets both build their patches here, so the
 * three pitch personalities stay consistent everywhere (gameplay, prediction,
 * tests). Pure, stateless, deterministic — no engine objects, no allocation
 * beyond the returned arrays.
 *
 * The numbers are tuned so the SAME delivery is clearly different per type:
 *   - Hard  : springy + true  -> high, fast bounce; carries to the keeper.
 *   - Dry   : abrasive + soft -> low bounce, big grip/turn, pace off.
 *   - Green : grassy + damp   -> strong seam movement, decent carry.
 */
class CRICKETPHYSICS_API FCricketPitchMaterialLibrary
{
public:
	/** The canonical base surface for a pitch type (the deck "as prepared"). */
	static FCricketSurfacePatch MakePatch(ECricketPitchType Type);

	/**
	 * A representative length-zone layout for a pitch type: a slightly more worn
	 * "good length" band, and (for dry decks) rough outside the right-hander's
	 * off stump. Distances are metres from the batter's stumps. Returned empty
	 * for Balanced/Custom (use the base patch everywhere).
	 */
	static TArray<FCricketPitchZone> MakeZones(ECricketPitchType Type);

	/** Human-readable name for tooling / debug overlays. */
	static FName DisplayName(ECricketPitchType Type);
};
