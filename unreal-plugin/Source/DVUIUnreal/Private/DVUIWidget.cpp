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

	Renderer = FDVUIUnrealModule::Get().GetRenderer();
	if (!Renderer.IsValid())
	{
		return SNew(STextBlock).Text(LOCTEXT("DVUIRendererNotAvailable", "DVUI renderer not available"));
	}

	// Each UDVUIWidget gets its own SDVUIWidget but they all share the
	// single FDVUIRenderer (and thus the single dvui.Window). dvui isn't
	// designed for multiple concurrent windows — place one UDVUIWidget per
	// app; multiples will interleave events and overwrite each other.
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
	Renderer.Reset();
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
