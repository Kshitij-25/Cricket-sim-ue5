// Copyright Epic Games standard project template. CricketSim editor target.

using UnrealBuildTool;
using System.Collections.Generic;

public class CricketSimEditorTarget : TargetRules
{
	public CricketSimEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		// Pinned to V6 (project targets UE 5.7); allow building against a newer
		// installed engine (e.g. 5.8) without inheriting its stricter shared defaults.
		bOverrideBuildEnvironment = true;

		ExtraModuleNames.AddRange(new string[]
		{
			"CricketPhysics",
			"CricketGameplay",
			"CricketSim",
			"CricketSimEditor"
		});
	}
}
