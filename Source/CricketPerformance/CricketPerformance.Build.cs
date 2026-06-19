// CricketPerformance: the top-of-graph profiling & optimization framework. It sits
// ABOVE the simulation modules (like UI/Audio) so it can observe and benchmark all of
// them; nothing depends on it, so depending on the full sim stack creates no cycle.
//   - The Performance Manager subsystem + on-screen dashboard
//   - The Replay Optimization Layer (operates on CricketPhysics replay clips)
//   - The benchmark / stress harness (drives the real CricketAI match simulator)
//   - Project settings (budgets, FPS targets, optimizer knobs)
// Low-level instrumentation lives in CricketPerfCore, which the sim modules emit into.

using UnrealBuildTool;

public class CricketPerformance : ModuleRules
{
	public CricketPerformance(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",  // UCricketPerformanceSettings : UDeveloperSettings
			"CricketPerfCore",
			"CricketPhysics",   // replay clip types optimized by the Replay Optimization Layer
			"CricketAI"         // FCricketAIMatchSimulator, driven by the benchmark harness
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CricketGameplay",  // discover ball/replay/fielder/anim components to attribute memory
			"CricketSim",       // match rules/types for the long-match stress benchmark
			"RHI",              // GGPUFrameTime
			"RenderCore"        // GRenderThreadTime / GGameThreadTime
		});
	}
}
