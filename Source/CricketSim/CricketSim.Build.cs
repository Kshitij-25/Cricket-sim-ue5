// CricketSim: primary game module. Owns match flow, rules (T20), game/state
// modes, and team/player data. The top of the runtime dependency graph.

using UnrealBuildTool;

public class CricketSim : ModuleRules
{
	public CricketSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CricketPhysics",
			"CricketGameplay"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput",
			"InputCore"   // EKeys::* used by the match runner's polled control scheme
		});
	}
}
