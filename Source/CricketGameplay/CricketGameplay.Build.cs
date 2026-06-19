// CricketGameplay: actors, pawns, and components that bind the physics core to
// the world (ball actor, bowling/batting input, basic fielding, bat-ball collision).

using UnrealBuildTool;

public class CricketGameplay : ModuleRules
{
	public CricketGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"CricketPhysics",
			"CricketPerfCore"   // CRICKET_PERF_SCOPE instrumentation in hot ticks
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Chaos",
			"EnhancedInput",
			"InputCore"   // EKeys::* used by the bowling rig's polled control scheme
		});
	}
}
