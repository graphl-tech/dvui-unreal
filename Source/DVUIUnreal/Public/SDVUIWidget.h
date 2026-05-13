// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Rendering/SlateDrawBuffer.h"
#include "DVUI.h"

class FDVUIRenderer;

/**
 * Slate Widget hosting the dvui frame.
 *
 * Each frame, dvui's drawClippedTriangles fires our `QueueDrawData` callback,
 * which appends raw vertex/index/cmd data to per-frame buffers. OnPaint then
 * snapshots those buffers into an FDvuiCustomElement and dispatches via
 * FSlateDrawElement::MakeCustom — render-thread Draw runs our own shader
 * with the canonical dvui PMA pipeline.
 */
class DVUIUNREAL_API SDVUIWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDVUIWidget)
		: _DVUIRenderer(nullptr)
		{}
		SLATE_ARGUMENT(TSharedPtr<FDVUIRenderer>, DVUIRenderer)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyClippingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	void SetCanvasSize(FVector2D NewSize);
	FVector2D GetCanvasSize() const { return CanvasSize; }

	void SetScale(float NewScale) { Scale = NewScale; }
	float GetScale() const { return Scale; }

	FVector2D GetLastPaintedSize() const { return LastPaintedSize; }

	/** Append one indexed-triangle batch from dvui. Vertex format matches the
	 *  C-ABI DVUIVertex (x,y,u,v,color). */
	void QueueDrawData(
		const DVUIVertex* Vertices,
		uint32 VertexCount,
		const uint32_t* Indices,
		uint32 IndexCount,
		uintptr_t TexturePtr,
		const float* ClipRect
	);

	/** Drop accumulated draw data — called at the start of each dvui frame. */
	void ClearDrawData();

private:
	TSharedPtr<FDVUIRenderer> DVUIRenderer;
	FVector2D CanvasSize;
	float Scale = 1.0f;
	mutable FVector2D LastPaintedSize = FVector2D::ZeroVector;

	// Raw per-frame draw data (mirrors dvui's output exactly). Accumulated by
	// QueueDrawData on the game thread, snapshotted in OnPaint.
	struct FFrameDrawCmd
	{
		uint32 VertexOffset = 0;
		uint32 VertexCount  = 0;
		uint32 IndexOffset  = 0;
		uint32 IndexCount   = 0;
		uintptr_t TexturePtr = 0;
		bool bHasClipRect = false;
		float ClipRect[4]  = { 0, 0, 0, 0 }; // x, y, w, h in canvas pixels
	};

	mutable TArray<uint8>          FrameVertexBytes;  // packed FDvuiHostVertex array
	mutable TArray<uint32>         FrameIndices;
	mutable TArray<FFrameDrawCmd>  FrameCommands;
	mutable FCriticalSection       FrameDataMutex;
	// Last cursor we asked Slate to query for. Used to skip
	// FSlateApplication::QueryCursor() calls when nothing changed —
	// querying every paint would be wasteful. Initialized to a sentinel
	// (-1) so the first paint always triggers a query.
	mutable int32                  LastDvuiCursor = -1;

	// Slate's MakeCustom only stores a TWeakPtr to the custom element, so we
	// must hold a strong ref ourselves until the render thread has consumed
	// it. Keep a small ring of recent frames' elements.
	mutable TArray<TSharedPtr<class FDvuiCustomElement, ESPMode::ThreadSafe>> InflightElements;
};
