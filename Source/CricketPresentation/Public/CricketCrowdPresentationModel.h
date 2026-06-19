#pragma once

#include "CoreMinimal.h"
#include "CricketPresentationTypes.h"
#include "CricketMatchSnapshot.h"
#include "CricketCrowdPresentationModel.generated.h"

/**
 * FCricketCrowdPresentationModel — the crowd ATMOSPHERE ARC.
 *
 * This is deliberately distinct from the audio layer's FCricketCrowdController. That
 * one models the INSTANTANEOUS reaction bump-and-decay that gates one-shot cheers.
 * This one models the slower NARRATIVE arc the broadcast cares about: a baseline
 * tension that rises through a tight death-overs chase (and falls when the game drifts
 * out of reach), with event impulses layered on top. It outputs a single atmosphere
 * level [0,1] and a mood band the director uses for "match-closing tension" beats.
 *
 * Pure data + methods (like the audio crowd controller), so it is driven identically
 * by the live subsystem and the headless tests. It NEVER reads or writes gameplay —
 * it is fed a read-only snapshot.
 */
USTRUCT(BlueprintType)
struct CRICKETPRESENTATION_API FCricketCrowdPresentationModel
{
	GENERATED_BODY()

	/** Current atmosphere [0,1]. The blend of context baseline + decaying event impulse. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Crowd") float Atmosphere = 0.0f;

	/** The slow baseline the atmosphere relaxes toward (set from match context). */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Crowd") float ContextBaseline = 0.0f;

	/** The fast, event-driven component that decays back toward the baseline. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Crowd") float EventCharge = 0.0f;

	/** How fast the event charge bleeds off (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Crowd", meta = (ClampMin = "0.0")) float ChargeDecayPerSec = 0.18f;

	/** How fast the baseline tracks a change in match context (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation|Crowd", meta = (ClampMin = "0.0")) float BaselineTrackPerSec = 0.5f;

	/** Inject the excitement impulse of a classified moment. */
	void ApplyEvent(const FCricketPresentationEvent& Event)
	{
		EventCharge = FMath::Clamp(EventCharge + Event.CrowdImpulse, 0.0f, 1.0f);
	}

	/**
	 * Recompute the context baseline from the match situation. A close chase in the
	 * death overs is the loudest sustained atmosphere; a procession or a one-sided game
	 * is calm. Call when the snapshot changes (once per ball is plenty).
	 */
	void UpdateContext(const FCricketMatchSnapshot& Snapshot)
	{
		float Target = 0.15f; // a baseline murmur whenever play is live

		if (Snapshot.bChasing && Snapshot.RunsRequired > 0 && Snapshot.BallsRemaining > 0)
		{
			// Closeness: runs-required per ball near 1.5–2.0 with few balls left = tense.
			const float Rrb = (float)Snapshot.RunsRequired / FMath::Max(1, Snapshot.BallsRemaining);
			const float BallsFactor = FMath::Clamp(1.0f - (Snapshot.BallsRemaining / 60.0f), 0.0f, 1.0f); // ramps in the last 10 overs
			const float Tightness = FMath::Clamp(1.0f - FMath::Abs(Rrb - 1.6f) / 1.6f, 0.0f, 1.0f);       // 1 when ~10/over needed
			Target = FMath::Max(Target, 0.25f + 0.65f * BallsFactor * Tightness);
		}
		ContextBaseline = Target;
	}

	/** Decay the event charge and ease the atmosphere toward baseline + charge. */
	void Tick(float DeltaSeconds)
	{
		EventCharge = FMath::Clamp(EventCharge - ChargeDecayPerSec * DeltaSeconds, 0.0f, 1.0f);
		const float Desired = FMath::Clamp(ContextBaseline + EventCharge, 0.0f, 1.0f);
		// Ease so the bed swells/relaxes smoothly rather than snapping.
		const float Alpha = FMath::Clamp(BaselineTrackPerSec * DeltaSeconds, 0.0f, 1.0f);
		Atmosphere = FMath::Lerp(Atmosphere, Desired, Alpha);
	}

	/** The mood band the broadcast director reads for "tension" beats. */
	ECricketCrowdMood Mood() const
	{
		// A high baseline (a close chase) reads as Tense even at moderate atmosphere.
		if (ContextBaseline > 0.6f && Atmosphere > 0.45f) { return ECricketCrowdMood::Tense; }
		if (Atmosphere > 0.8f) { return ECricketCrowdMood::Electric; }
		if (Atmosphere > 0.55f) { return ECricketCrowdMood::Loud; }
		if (Atmosphere > 0.3f) { return ECricketCrowdMood::Building; }
		return ECricketCrowdMood::Calm;
	}

	static FString MoodName(ECricketCrowdMood M)
	{
		switch (M)
		{
		case ECricketCrowdMood::Calm:     return TEXT("Calm");
		case ECricketCrowdMood::Building: return TEXT("Building");
		case ECricketCrowdMood::Loud:     return TEXT("Loud");
		case ECricketCrowdMood::Electric: return TEXT("Electric");
		case ECricketCrowdMood::Tense:    return TEXT("Tense");
		default:                          return TEXT("Calm");
		}
	}
};
