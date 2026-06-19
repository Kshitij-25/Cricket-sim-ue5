// CricketAI: the cricket-intelligence layer. It is a DRIVER of the existing
// gameplay/physics systems, not a parallel simulation. Every decision it makes is
// expressed as the SAME intent a human supplies — a bowling intent fed to
// UCricketBowlingComponent, a batting intent fed to UCricketBattingComponent, a
// chaser nomination fed to UCricketFielderComponent — and the outcome then EMERGES
// from the very same physics + rules the player is subject to. The AI never writes
// a result; it only chooses an action.
//
// Position in the one-directional module graph: it sits above CricketSim (it reads
// the Match Engine for awareness) and drives CricketGameplay's controllers, so it
// depends on all three runtime cores but nothing above it (UI/Audio remain pure
// consumers and never depend on AI):
//
//   CricketAI -> CricketSim -> CricketGameplay -> CricketPhysics -> Engine/Core
//
// The reasoning cores (the "brains"), the difficulty model, the tactical evaluator,
// the contest model and the match simulator are all deliberately pure and
// UWorld-free so the whole intelligence layer is deterministic and unit-testable
// headlessly, exactly like CricketPhysics and the Match Engine.

using UnrealBuildTool;

public class CricketAI : ModuleRules
{
	public CricketAI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",             // UActorComponent, DrawDebug, FRandomStream
			"CricketPhysics",     // SI vocabulary: line/length/movement, shot/footwork, fielding predictor
			"CricketGameplay",    // the controllers it drives: bowling / batting / fielder components
			"CricketSim",         // the Match Engine + scoring/outcome types (match awareness)
			"CricketPerfCore"     // CRICKET_PERF_SCOPE instrumentation in AI controller ticks
		});
	}
}
