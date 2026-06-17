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

		ExtraModuleNames.AddRange(new string[]
		{
			"CricketPhysics",
			"CricketGameplay",
			"CricketSim"
		});
	}
}
