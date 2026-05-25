// Copyright Epic Games, Inc. All Rights Reserved.

#include "DVUIWidget.h"
#include "DVUIUnrealModule.h"
#include "DVUIRenderer.h"
#include "SDVUIWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DVUI"

UDVUIWidget::UDVUIWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;
}

TSharedRef<SWidget> UDVUIWidget::RebuildWidget()
{
	if (!FDVUIUnrealModule::IsAvailable())
	{
		return SNew(STextBlock).Text(LOCTEXT("DVUINotAvailable", "DVUI module not available"));
	}

	// Each UDVUIWidget owns its own renderer — and thus its own dvui.Window
	// and app instance (the backend runs app.init on create, app.deinit on
	// destroy). This keeps DVUI state isolated per widget instance and per
	// play session; ReleaseSlateResources tears it down. The module-level
	// renderer is left for the non-UMG GameMode path only.
	if (Renderer.IsValid())
	{
		Renderer->Shutdown();
		Renderer.Reset();
	}
	Renderer = MakeShared<FDVUIRenderer>();
	if (!Renderer->Initialize())
	{
		Renderer.Reset();
		return SNew(STextBlock).Text(LOCTEXT("DVUIRendererNotAvailable", "DVUI renderer not available"));
	}

	SlateWidget = SNew(SDVUIWidget).DVUIRenderer(Renderer);
	SlateWidget->SetCanvasSize(FVector2D(CanvasWidth, CanvasHeight));
	SlateWidget->SetScale(Scale);
	return SlateWidget.ToSharedRef();
}

void UDVUIWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (SlateWidget.IsValid())
	{
		SlateWidget->SetCanvasSize(FVector2D(CanvasWidth, CanvasHeight));
		SlateWidget->SetScale(Scale);
	}
}

void UDVUIWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	SlateWidget.Reset();
	// We own this renderer (see RebuildWidget) — tear down its dvui.Window
	// and app so state doesn't leak into the next widget or play session.
	if (Renderer.IsValid())
	{
		Renderer->Shutdown();
		Renderer.Reset();
	}
}

#if WITH_EDITOR
const FText UDVUIWidget::GetPaletteCategory()
{
	return LOCTEXT("DVUIPaletteCategory", "DVUI");
}
#endif

FVector2D UDVUIWidget::GetCanvasSize() const
{
	return FVector2D(CanvasWidth, CanvasHeight);
}

void UDVUIWidget::SetCanvasSize(FVector2D NewSize)
{
	CanvasWidth = NewSize.X;
	CanvasHeight = NewSize.Y;

	if (SlateWidget.IsValid())
	{
		SlateWidget->SetCanvasSize(NewSize);
	}
}

void UDVUIWidget::SetScale(float NewScale)
{
	Scale = NewScale;
	if (SlateWidget.IsValid())
	{
		SlateWidget->SetScale(NewScale);
	}
}

#undef LOCTEXT_NAMESPACE
