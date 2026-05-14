using UnrealBuildTool;

public class DVUITest : ModuleRules
{
	public DVUITest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore",
			"ImageWrapper",
			"ImageCore",
			"DVUIUnreal",
		});
	}
}
