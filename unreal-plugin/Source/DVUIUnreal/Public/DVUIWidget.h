// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "DVUIWidget.generated.h"

class SDVUIWidget;
class FDVUIRenderer;

/**
 * UMG widget that hosts a DVUI frame.
 *
 * Drop this onto any UMG widget blueprint (palette category "DVUI"). The
 * embedded DVUI app rebuilds at the widget's actual on-screen size each
 * frame, so anchors / fills work as expected.
 */
UCLASS()
class DVUIUNREAL_API UDVUIWidget : public UWidget
{
	GENERATED_BODY()

public:
	UDVUIWidget(const FObjectInitializer& ObjectInitializer);

	/** Hint for the widget's preferred (desired) size used by Slate layout
	 *  when nothing else constrains it. Does NOT control the dvui canvas
	 *  size — dvui always renders at the widget's actual rendered area so
	 *  clicks map to the visible pixels. To resize: change the widget's
	 *  slot in UMG, or anchor-fill on a CanvasPanel slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DVUI")
	float CanvasWidth = 1920.0f;

	/** See CanvasWidth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DVUI")
	float CanvasHeight = 1080.0f;

	/** Multiplier on dvui's content_scale — affects font rasterization
	 *  size, icon size, padding, and other "natural-units" measurements
	 *  inside the dvui frame. Does NOT change canvas pixel dimensions
	 *  or mouse-coordinate mapping. 1.0 = native; 2.0 = roughly 2× size
	 *  text/icons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DVUI", meta = (ClampMin = "0.1", UIMin = "0.25", UIMax = "4.0"))
	float Scale = 1.0f;

	UFUNCTION(BlueprintPure, Category = "DVUI")
	FVector2D GetCanvasSize() const;

	UFUNCTION(BlueprintCallable, Category = "DVUI")
	void SetCanvasSize(FVector2D NewSize);

	UFUNCTION(BlueprintCallable, Category = "DVUI")
	void SetScale(float NewScale);

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

#if WITH_EDITOR
	virtual const FText GetPaletteCategory() override;
#endif

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	TSharedPtr<SDVUIWidget> SlateWidget;
	TSharedPtr<FDVUIRenderer> Renderer;
};
