#include "CricketPitchMaterial.h"

FCricketSurfacePatch FCricketPitchMaterialLibrary::MakePatch(ECricketPitchType Type)
{
	FCricketSurfacePatch P; // struct defaults == a fair, balanced Test surface

	switch (Type)
	{
	case ECricketPitchType::Hard:
		// Perth-like: rock-hard, very springy, true and quick. A little live
		// grass binds it; barely any moisture; smooth so spin slides on.
		P.Hardness      = 0.92;
		P.Restitution   = 0.62;
		P.Friction      = 0.44;
		P.Moisture      = 0.05;
		P.GrassCoverage = 0.25;
		P.Roughness     = 0.10;
		P.Wear          = 0.0;
		P.Unevenness    = 0.02;
		break;

	case ECricketPitchType::Dry:
		// Subcontinent-like: soft, dusty, abrasive. Low springy bounce, lots of
		// grip for the spinners, pace comes off the surface. Already a touch worn.
		P.Hardness      = 0.55;
		P.Restitution   = 0.44;
		P.Friction      = 0.62;
		P.Moisture      = 0.04;
		P.GrassCoverage = 0.03;
		P.Roughness     = 0.60;
		P.Wear          = 0.25;
		P.Unevenness    = 0.12;
		break;

	case ECricketPitchType::Green:
		// Hobart/England-morning-like: firm, grassy, tinged with moisture. Holds
		// together (decent carry) but the grassy, damp top grips a landing seam
		// hard — seam movement is the headline. Smooth, so little spin grip.
		P.Hardness      = 0.75;
		P.Restitution   = 0.52;
		P.Friction      = 0.50;
		P.Moisture      = 0.30;
		P.GrassCoverage = 0.80;
		P.Roughness     = 0.12;
		P.Wear          = 0.0;
		P.Unevenness    = 0.04;
		break;

	case ECricketPitchType::Balanced:
	case ECricketPitchType::Custom:
	default:
		break; // leave the defaults
	}

	return P;
}

TArray<FCricketPitchZone> FCricketPitchMaterialLibrary::MakeZones(ECricketPitchType Type)
{
	TArray<FCricketPitchZone> Zones;

	// The "good length" band for a right-hander is roughly 4–7 m in front of the
	// stumps; it gets the most traffic so it wears first.
	auto AddGoodLengthWear = [&](double ExtraWear, double ExtraRough, double ExtraUneven)
	{
		FCricketPitchZone Z;
		Z.MinDistanceM = 4.0;
		Z.MaxDistanceM = 7.0;
		Z.Patch = MakePatch(Type);
		Z.Patch.Wear       = FMath::Clamp(Z.Patch.Wear + ExtraWear, 0.0, 1.0);
		Z.Patch.Roughness  = FMath::Clamp(Z.Patch.Roughness + ExtraRough, 0.0, 1.0);
		Z.Patch.Unevenness = FMath::Clamp(Z.Patch.Unevenness + ExtraUneven, 0.0, 1.0);
		Zones.Add(Z);
	};

	switch (Type)
	{
	case ECricketPitchType::Hard:
		AddGoodLengthWear(0.05, 0.05, 0.01);
		break;

	case ECricketPitchType::Dry:
		// Dry decks wear hard on a length and crack up — the spinner's gold.
		AddGoodLengthWear(0.25, 0.20, 0.10);
		break;

	case ECricketPitchType::Green:
		AddGoodLengthWear(0.05, 0.05, 0.02);
		break;

	case ECricketPitchType::Balanced:
	case ECricketPitchType::Custom:
	default:
		break; // no zoning — the base patch applies everywhere
	}

	return Zones;
}

FName FCricketPitchMaterialLibrary::DisplayName(ECricketPitchType Type)
{
	switch (Type)
	{
	case ECricketPitchType::Hard:     return TEXT("Hard");
	case ECricketPitchType::Dry:      return TEXT("Dry");
	case ECricketPitchType::Green:    return TEXT("Green");
	case ECricketPitchType::Custom:   return TEXT("Custom");
	case ECricketPitchType::Balanced:
	default:                          return TEXT("Balanced");
	}
}
