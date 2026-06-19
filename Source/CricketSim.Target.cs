// Copyright Epic Games standard project template. CricketSim primary game target.

using UnrealBuildTool;
using System.Collections.Generic;

public class CricketSimTarget : TargetRules
{
	public CricketSimTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		// C++20 across the project. Determinism-friendly defaults for physics.
		CppStandard = CppStandardVersion.Cpp20;

		// All runtime modules linked into the packaged game. These top-of-graph
		// modules (UI/Audio/AI/Performance/Presentation) are not dependencies of
		// CricketSim, so they must be named explicitly to be cooked into a Game/
		// Shipping build — the editor only picks them up via the .uproject.
		ExtraModuleNames.AddRange(new string[]
		{
			"CricketPerfCore",
			"CricketPhysics",
			"CricketGameplay",
			"CricketSim",
			"CricketAI",
			"CricketUI",
			"CricketAudio",
			"CricketPerformance",
			"CricketPresentation"
		});
	}
}
