#include "CricketPitchProfileAsset.h"

void UCricketPitchProfileAsset::ConfigureFromType(ECricketPitchType Type)
{
	PitchType = Type;
	BasePatch = FCricketPitchMaterialLibrary::MakePatch(Type);
	Zones = FCricketPitchMaterialLibrary::MakeZones(Type);
	ProfileName = FCricketPitchMaterialLibrary::DisplayName(Type);
}

void UCricketPitchProfileAsset::ApplyDayProgression()
{
	// Deterioration hook: today's wear is the max of any authored baseline and
	// what the day/session implies, so the surface only ever ages.
	Wear = FMath::Max(Wear, DayProgression.ComputeWear());
}

FCricketSurfacePatch UCricketPitchProfileAsset::SamplePatch(double DistanceDownPitchM) const
{
	FCricketSurfacePatch Patch = BasePatch;

	for (const FCricketPitchZone& Zone : Zones)
	{
		if (DistanceDownPitchM >= Zone.MinDistanceM && DistanceDownPitchM <= Zone.MaxDistanceM)
		{
			Patch = Zone.Patch;
			break;
		}
	}

	// Apply global wear: an old pitch is softer, less springy, more abrasive,
	// grippier and more uneven.
	const double W = FMath::Clamp(Wear, 0.0, 1.0);
	Patch.Hardness    = FMath::Clamp(Patch.Hardness    * (1.0 - 0.35 * W), 0.0, 1.0);
	Patch.Restitution = FMath::Clamp(Patch.Restitution * (1.0 - 0.25 * W), 0.0, 0.95);
	Patch.Friction    = Patch.Friction * (1.0 + 0.5 * W);
	Patch.Roughness   = FMath::Clamp(Patch.Roughness   + 0.5 * W, 0.0, 1.0);
	Patch.Wear        = FMath::Clamp(Patch.Wear        + W, 0.0, 1.0);
	Patch.Unevenness  = FMath::Clamp(Patch.Unevenness  + 0.6 * W, 0.0, 1.0);

	// FUTURE: footmarks would locally raise Roughness/Wear/Unevenness here based
	// on DistanceDownPitchM (and lateral offset, once the sampler is 2D). Left
	// unimplemented per the brief — the data is carried on Footmarks.

	return Patch;
}

#if WITH_EDITOR
void UCricketPitchProfileAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName Changed = PropertyChangedEvent.GetPropertyName();
	if (Changed == GET_MEMBER_NAME_CHECKED(UCricketPitchProfileAsset, PitchType)
		&& PitchType != ECricketPitchType::Custom)
	{
		ConfigureFromType(PitchType);
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
