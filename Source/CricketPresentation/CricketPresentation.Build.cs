// CricketPresentation: the broadcast-quality PRESENTATION LAYER. A pure CONSUMER of
// the simulation — it reads the Match Engine, Replay, Camera and Audio systems and
// directs cameras, replays, crowd atmosphere and on-screen broadcast beats on top of
// them. It NEVER writes into a gameplay/physics system and NEVER alters an outcome.
//
// It sits at the very top of the one-directional module graph, alongside CricketUI
// and CricketAudio:
//
//   CricketPresentation -> CricketAudio / CricketUI
//                       -> CricketSim -> CricketGameplay -> CricketPhysics -> Engine/Core
//
// The decision cores (event classifier, broadcast/replay directors, crowd & score
// models, match-flow sequences) are pure structs with no UWorld, so the whole
// "what should we show, which camera, replay or not, how loud is the crowd" logic is
// unit-tested headlessly. The UCricketPresentationSubsystem is a thin world wrapper
// that discovers the live systems, runs the cores, and commands the camera/replay.

using UnrealBuildTool;

public class CricketPresentation : ModuleRules
{
	public CricketPresentation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",          // UTickableWorldSubsystem, UCameraComponent, debug draw
			"CricketPhysics",  // camera + replay data types
			"CricketGameplay", // camera director, replay component, ball physics
			"CricketSim",      // the Match Engine (delegates + read-back), scoring types
			"CricketAudio"     // crowd controller / audio subsystem (read-only)
		});
	}
}
