// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FDVUIRenderer;
class SDVUIWidget;

/**
 * DVUI Unreal Engine plugin module.
 *
 * Loads at PostConfigInit so the global shader registrations run before
 * the engine's shader compile pass. The renderer is constructed lazily on
 * first GetRenderer() call to avoid touching the UObject system before it
 * is ready.
 */
class DVUIUNREAL_API FDVUIUnrealModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FDVUIUnrealModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FDVUIUnrealModule>("DVUIUnreal");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("DVUIUnreal");
	}

	/** Lazily construct (on first call) and return the singleton renderer. */
	TSharedPtr<FDVUIRenderer> GetRenderer();

	/** Convenience accessor for the renderer's default Slate widget. */
	TSharedPtr<SDVUIWidget> GetWidget();

private:
	TSharedPtr<FDVUIRenderer> Renderer;
};
