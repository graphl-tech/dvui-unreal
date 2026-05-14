// Copyright Epic Games, Inc. All Rights Reserved.

#include "DVUIUnrealModule.h"
#include "DVUIRenderer.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FDVUIUnrealModule"

void FDVUIUnrealModule::StartupModule()
{
	// Register the plugin's shader directory under /DVUIUnreal so the
	// global shader's #include paths resolve. Must happen before any
	// FGlobalShader subclass tries to load — i.e. before the renderer's
	// first frame.
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DVUIUnreal"));
	if (Plugin.IsValid())
	{
		const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/DVUIUnreal"), ShaderDir);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DVUIUnreal: plugin not found in IPluginManager — shader files won't load"));
	}
}

TSharedPtr<FDVUIRenderer> FDVUIUnrealModule::GetRenderer()
{
	if (!Renderer.IsValid())
	{
		Renderer = MakeShared<FDVUIRenderer>();
		if (!Renderer->Initialize())
		{
			UE_LOG(LogTemp, Error, TEXT("DVUIUnreal: Failed to initialize renderer"));
			Renderer.Reset();
		}
	}
	return Renderer;
}

void FDVUIUnrealModule::ShutdownModule()
{
	if (Renderer)
	{
		Renderer->Shutdown();
		Renderer.Reset();
	}
}

TSharedPtr<SDVUIWidget> FDVUIUnrealModule::GetWidget()
{
	if (TSharedPtr<FDVUIRenderer> R = GetRenderer())
	{
		return R->GetWidget();
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDVUIUnrealModule, DVUIUnreal)
