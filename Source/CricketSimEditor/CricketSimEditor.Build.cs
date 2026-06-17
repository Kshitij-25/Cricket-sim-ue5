// CricketSimEditor: editor-only tooling — data validation, physics tuning
// utilities, and custom asset/detail panels. Never shipped in the game build.

using UnrealBuildTool;

public class CricketSimEditor : ModuleRules
{
	public CricketSimEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"CricketPhysics",
			"CricketGameplay",
			"CricketSim"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"ToolMenus",
			"EditorSubsystem"
		});
	}
}
