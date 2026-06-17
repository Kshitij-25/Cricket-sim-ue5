#pragma once

#include "CoreMinimal.h"
#include "CricketInputTypes.h"
#include "CricketBattingTypes.h"

/**
 * FCricketInputModel — the pure brain of the control layer. It turns held control
 * state (which keys are down) into INTENT, exactly mirroring Cricket 07's scheme,
 * and maps that intent onto the existing gameplay inputs. Headless-testable; no
 * Enhanced Input or actor dependency. It never decides an outcome.
 */
class CRICKETPHYSICS_API FCricketInputModel
{
public:
	// --- Batting ---

	/** Resolve the Cricket-07-style shot from the held control state. */
	static FCricketBattingShotIntent ResolveBattingShot(const FCricketBattingControlState& State);

	/** Map a resolved shot intent onto the existing batting component input. */
	static FCricketBattingInput ToBattingInput(const FCricketBattingShotIntent& Intent, bool bRightHanded);

	/** Aim yaw (deg, + toward off for RH) for a steered direction. */
	static double DirectionAimYawDeg(ECricketShotDirection Direction);

	// --- Bowling ---

	/** Resolve bowling intent deltas (line/length step, swing/spin amount, pace). */
	static FCricketBowlingControlIntent ResolveDelivery(const FCricketBowlingControlState& State);

	// --- Running ---

	/** The run call for a pressed running key (D take / A send back / W dive). */
	static ECricketRunCall ResolveRunCall(bool bTake, bool bSendBack, bool bDive);

	// --- Fielding ---

	/** The highest-priority fielding action from the held keys. */
	static ECricketFieldAction ResolveFieldAction(bool bCatch, bool bDive, bool bThrow, bool bRelay, bool bMove);

	// --- Context switching (the Input State Manager's pure rule) ---

	/**
	 * The input context that should be active given the player's role and state.
	 * Replay overrides everything; otherwise the role's layer is active.
	 */
	static ECricketInputContext ResolveContext(
		bool bReplayActive, bool bIsBatting, bool bIsBowling, bool bIsFielding);
};
