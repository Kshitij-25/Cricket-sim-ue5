// Headless automation tests for the PITCH simulation system.
// These guard the things the brief promises: the three pitch types must produce
// noticeably DIFFERENT ball behaviour (bounce / pace / turn / seam) for the SAME
// delivery, the solvers must behave monotonically with the surface parameters,
// and the whole thing must stay deterministic (no hidden RNG).
//
// Run via the editor Automation panel or:
//   UnrealEditor-Cmd CricketSim.uproject -ExecCmds="Automation RunTests CricketSim.Pitch" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "CricketPitchInteraction.h"
#include "CricketPitchMaterial.h"
#include "CricketPitchProfileAsset.h"
#include "CricketPhysicsConstants.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace CricketPhysics;

namespace
{
	// A stock descending delivery hitting the pitch at a good-length-ish angle.
	FCricketBallState IncomingState()
	{
		FCricketBallState S;
		S.Position = FVector(0, 0, BallRadiusM);
		S.Velocity = FVector(30.0, 0.0, -8.0); // ~33 m/s, descending
		S.SeamNormal = FVector(0, 1, 0);
		S.SeamStability = 1.0;
		return S;
	}

	FCricketImpact FlatImpact(double Variance = 0.0)
	{
		FCricketImpact I;
		I.ContactNormal = FVector(0, 0, 1);
		I.SeamContact = 0.0;
		I.Variance = Variance;
		return I;
	}

	// Resolve one bounce on a given pitch type and hand back the report (+ out state).
	FCricketBounceReport BounceOn(ECricketPitchType Type, FCricketBallState& State, const FCricketImpact& Impact)
	{
		const FCricketSurfacePatch Patch = FCricketPitchMaterialLibrary::MakePatch(Type);
		return FCricketPitchInteraction::ResolveBounce(State, Patch, Impact);
	}
}

// 1. SAME DELIVERY, DIFFERENT PITCHES — bounce height and pace off the surface
//    must differ markedly: a hard deck bounces higher and keeps more pace than a
//    dry deck; all three are distinct.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchSameDeliveryTest,
	"CricketSim.Pitch.SameDeliveryDifferentPitches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchSameDeliveryTest::RunTest(const FString&)
{
	const FCricketImpact Impact = FlatImpact();

	FCricketBallState Hard = IncomingState();
	FCricketBallState Dry  = IncomingState();
	FCricketBallState Green = IncomingState();
	const FCricketBounceReport RH = BounceOn(ECricketPitchType::Hard,  Hard,  Impact);
	const FCricketBounceReport RD = BounceOn(ECricketPitchType::Dry,   Dry,   Impact);
	const FCricketBounceReport RG = BounceOn(ECricketPitchType::Green, Green, Impact);

	TestTrue(TEXT("Hard pitch bounces higher than dry"), RH.BounceHeightM > RD.BounceHeightM);
	TestTrue(TEXT("Hard pitch bounces higher than green"), RH.BounceHeightM > RG.BounceHeightM);
	TestTrue(TEXT("Hard pitch keeps more pace than dry"), RH.SpeedRetainedFrac > RD.SpeedRetainedFrac);
	TestTrue(TEXT("Hard restitution exceeds dry"), RH.RestitutionUsed > RD.RestitutionUsed);
	TestTrue(TEXT("All three bounce heights are distinct"),
		!FMath::IsNearlyEqual(RH.BounceHeightM, RD.BounceHeightM, 1e-4) &&
		!FMath::IsNearlyEqual(RD.BounceHeightM, RG.BounceHeightM, 1e-4));
	return true;
}

// 2. SAME SPIN, DIFFERENT PITCHES — the dry (abrasive/worn) deck must turn the
//    ball more than the hard or green decks for identical revs.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchSameSpinTest,
	"CricketSim.Pitch.SameSpinDifferentPitches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchSameSpinTest::RunTest(const FString&)
{
	const FCricketImpact Impact = FlatImpact();

	auto TurnOn = [&](ECricketPitchType Type)
	{
		FCricketBallState S = IncomingState();
		S.AngularVelocity = FVector(250.0, 0.0, 0.0); // spin about the line of flight
		const FCricketBounceReport R = BounceOn(Type, S, Impact);
		return FMath::Abs(R.TurnMS);
	};

	const double THard  = TurnOn(ECricketPitchType::Hard);
	const double TDry   = TurnOn(ECricketPitchType::Dry);
	const double TGreen = TurnOn(ECricketPitchType::Green);

	TestTrue(TEXT("Dry pitch turns more than hard"), TDry > THard);
	TestTrue(TEXT("Dry pitch turns more than green"), TDry > TGreen);
	TestTrue(TEXT("Dry pitch produces meaningful turn"), TDry > 0.1);
	return true;
}

// 3. SAME SEAM ORIENTATION, DIFFERENT PITCHES — the green (grassy) deck must seam
//    the ball more than the hard or dry decks for an identical flush seam strike.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchSameSeamTest,
	"CricketSim.Pitch.SameSeamDifferentPitches", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchSameSeamTest::RunTest(const FString&)
{
	FCricketImpact Impact = FlatImpact();
	Impact.SeamContact = 1.0; // flush seam strike

	auto SeamOn = [&](ECricketPitchType Type)
	{
		FCricketBallState S = IncomingState(); // seam normal +Y, held (stable)
		const FCricketBounceReport R = BounceOn(Type, S, Impact);
		return FMath::Abs(R.SeamDeviationMS);
	};

	const double SHard  = SeamOn(ECricketPitchType::Hard);
	const double SDry   = SeamOn(ECricketPitchType::Dry);
	const double SGreen = SeamOn(ECricketPitchType::Green);

	TestTrue(TEXT("Green pitch seams more than hard"), SGreen > SHard);
	TestTrue(TEXT("Green pitch seams more than dry"), SGreen > SDry);
	TestTrue(TEXT("Green pitch produces meaningful seam movement"), SGreen > 0.1);
	return true;
}

// 4. Restitution rises with hardness and falls with moisture (solver monotonicity).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchRestitutionTest,
	"CricketSim.Pitch.RestitutionMonotonic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchRestitutionTest::RunTest(const FString&)
{
	const FCricketImpact Impact = FlatImpact();

	auto BounceE = [&](double Hardness, double Moisture)
	{
		FCricketSurfacePatch P; // balanced defaults
		P.Hardness = Hardness;
		P.Moisture = Moisture;
		FCricketBallState S = IncomingState();
		return FCricketPitchInteraction::ResolveBounce(S, P, Impact).RestitutionUsed;
	};

	TestTrue(TEXT("Harder surface => higher restitution"), BounceE(0.9, 0.1) > BounceE(0.4, 0.1));
	TestTrue(TEXT("Wetter surface => lower restitution"),  BounceE(0.7, 0.6) < BounceE(0.7, 0.0));
	return true;
}

// 5. Grip/skid threshold: a high-friction surface grips a spinning ball; a
//    low-friction one lets it skid on.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchGripSkidTest,
	"CricketSim.Pitch.GripSkidThreshold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchGripSkidTest::RunTest(const FString&)
{
	const FCricketImpact Impact = FlatImpact();

	auto Gripped = [&](double Friction, double Roughness)
	{
		FCricketSurfacePatch P;
		P.Friction = Friction;
		P.Roughness = Roughness;
		P.Moisture = 0.0;
		FCricketBallState S = IncomingState();
		S.AngularVelocity = FVector(200.0, 0.0, 0.0);
		return FCricketPitchInteraction::ResolveBounce(S, P, Impact).bGripped;
	};

	TestTrue(TEXT("High-friction abrasive surface grips"), Gripped(0.9, 0.8));
	TestFalse(TEXT("Low-friction smooth surface skids"),   Gripped(0.1, 0.0));
	return true;
}

// 6. Wobble seam: a scrambled seam (low stability) produces DIFFERENT movement
//    than a held seam for the same strike + variance — inconsistent by design.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchWobbleSeamTest,
	"CricketSim.Pitch.WobbleSeam", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchWobbleSeamTest::RunTest(const FString&)
{
	FCricketImpact Impact = FlatImpact(/*Variance*/ 0.7);
	Impact.SeamContact = 1.0;
	const FCricketSurfacePatch Patch = FCricketPitchMaterialLibrary::MakePatch(ECricketPitchType::Green);

	FCricketBallState Held = IncomingState();   Held.SeamStability = 1.0;
	FCricketBallState Wobble = IncomingState(); Wobble.SeamStability = 0.0;

	const double DevHeld   = FCricketPitchInteraction::ResolveBounce(Held, Patch, Impact).SeamDeviationMS;
	const double DevWobble = FCricketPitchInteraction::ResolveBounce(Wobble, Patch, Impact).SeamDeviationMS;

	TestTrue(TEXT("Wobble seam deviates differently from a held seam"),
		!FMath::IsNearlyEqual(DevHeld, DevWobble, 1e-3));
	return true;
}

// 7. Determinism: identical inputs reproduce the bounce bit-for-bit (no RNG).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchDeterminismTest,
	"CricketSim.Pitch.Determinism", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchDeterminismTest::RunTest(const FString&)
{
	FCricketImpact Impact = FlatImpact(/*Variance*/ -0.4);
	Impact.SeamContact = 0.6;
	const FCricketSurfacePatch Patch = FCricketPitchMaterialLibrary::MakePatch(ECricketPitchType::Dry);

	FCricketBallState A = IncomingState(); A.AngularVelocity = FVector(180, 0, 40);
	FCricketBallState B = IncomingState(); B.AngularVelocity = FVector(180, 0, 40);

	const FCricketBounceReport RA = FCricketPitchInteraction::ResolveBounce(A, Patch, Impact);
	const FCricketBounceReport RB = FCricketPitchInteraction::ResolveBounce(B, Patch, Impact);

	TestTrue(TEXT("Identical inputs reproduce velocity bit-for-bit"), A.Velocity == B.Velocity);
	TestTrue(TEXT("Identical inputs reproduce spin bit-for-bit"), A.AngularVelocity == B.AngularVelocity);
	TestTrue(TEXT("Identical inputs reproduce restitution"), RA.RestitutionUsed == RB.RestitutionUsed);
	return true;
}

// 8. Profile sampling: a Dry profile built from its type turns more on the worn
//    good-length band than on the fresh base surface, and wear deadens bounce.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCricketPitchProfileTest,
	"CricketSim.Pitch.ProfileSampling", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCricketPitchProfileTest::RunTest(const FString&)
{
	UCricketPitchProfileAsset* Profile = NewObject<UCricketPitchProfileAsset>();
	Profile->ConfigureFromType(ECricketPitchType::Dry);

	const FCricketSurfacePatch Fresh = Profile->SamplePatch(15.0); // off the worn band
	const FCricketSurfacePatch Worn  = Profile->SamplePatch(5.5);  // in the 4-7 m band

	TestTrue(TEXT("Worn good-length band is rougher than the base surface"),
		Worn.Roughness > Fresh.Roughness);

	// Global wear deadens the surface.
	Profile->Wear = 1.0;
	const FCricketSurfacePatch Aged = Profile->SamplePatch(15.0);
	TestTrue(TEXT("Global wear lowers hardness"), Aged.Hardness < Fresh.Hardness);
	TestTrue(TEXT("Global wear raises unevenness"), Aged.Unevenness > Fresh.Unevenness);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
