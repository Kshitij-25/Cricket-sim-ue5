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

		ExtraModuleNames.AddRange(new string[]
		{
			"CricketPhysics",
			"CricketGameplay",
			"CricketSim",
			"CricketSimEditor"
		});
	}
}
