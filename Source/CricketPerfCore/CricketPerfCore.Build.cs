// CricketPerfCore: the lowest-level instrumentation layer. It deliberately depends
// on nothing else in the project so EVERY higher module (Physics, Gameplay, AI, ...)
// can include the profiler and emit timing scopes without creating a dependency
// cycle. Pure, header-light, headlessly unit-testable.

using UnrealBuildTool;

public class CricketPerfCore : ModuleRules
{
	public CricketPerfCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});
	}
}
