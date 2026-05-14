// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System.Collections.Generic;

public class DVUIUnreal : ModuleRules
{
    public DVUIUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(new string[]
            {
                "Runtime/Engine/Classes",
                // ... add public include paths required here ...
            }
        );

        PrivateIncludePaths.AddRange(new string[]
            {
                // ... add private include paths if needed ...
            }
        );

        PublicDependencyModuleNames.AddRange(new string[]
                {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "ApplicationCore",
                "RHI",
                "RenderCore",
                "Renderer",
                "Slate",
                "SlateCore",
                "UMG"
                });

        PrivateDependencyModuleNames.AddRange(new string[]
                {
                "Projects"
                });

        // Add DVUI library paths
        string ThirdPartyPath = Path.Combine(ModuleDirectory, "../../ThirdParty");
        string DVUIPath = Path.Combine(ThirdPartyPath, "dvui-unreal-backend");
        string DVUILibPath = Path.Combine(DVUIPath, "zig-out/lib");

        // Include DVUI headers
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

        // Link the Zig-built DVUI library.
        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(Path.Combine(DVUILibPath, "libdvui_unreal.a"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(Path.Combine(DVUILibPath, "dvui_unreal.lib"));
            var win10KitBase = "C:/Program Files (x86)/Windows Kits/10/Lib";
            var win10KitVersions = new List<string>(Directory.EnumerateDirectories(win10KitBase));
            if (win10KitVersions.Count >= 1)
            {
                PublicAdditionalLibraries.Add(Path.Combine(win10KitBase, win10KitVersions[0], "um/x64/ntdll.lib"));
            }
            else
            {
                throw new System.NotSupportedException($"Only windows versions containing ntdll.lib are supported");
            }
        }


        // Ensure library is built before linking
        PublicDefinitions.Add("WITH_DVUI=1");

        DynamicallyLoadedModuleNames.AddRange(new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
        );

    }
}
