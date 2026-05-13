using UnrealBuildTool;

public class DVUITestEditorTarget : TargetRules
{
	public DVUITestEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DVUITest");
	}
}
