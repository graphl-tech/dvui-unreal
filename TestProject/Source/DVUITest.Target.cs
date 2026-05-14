using UnrealBuildTool;

public class DVUITestTarget : TargetRules
{
	public DVUITestTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DVUITest");
	}
}
