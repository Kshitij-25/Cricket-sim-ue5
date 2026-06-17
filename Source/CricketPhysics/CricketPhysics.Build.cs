// CricketPhysics: the deterministic ball-flight + aerodynamics core.
// Deliberately depends on as little of the engine as possible so the model
// stays portable, unit-testable, and free of gameplay/rendering concerns.

using UnrealBuildTool;

public class CricketPhysics : ModuleRules
{
	public CricketPhysics(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",       // Chaos types for collision queries / surface materials
			"DeveloperSettings"  // UCricketPhysicsSettings (Project Settings config)
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Chaos"
		});

		// Physics math is hot; favor speed in shipping, keep checks in dev.
		bUseUnity = true;
	}
}
