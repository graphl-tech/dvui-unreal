// ICustomSlateElement that renders one frame of dvui geometry on the
// render thread using our own shader and PMA blend pipeline.

#pragma once

#include "CoreMinimal.h"
#include "Rendering/RenderingCommon.h"
#include "RHI.h"
#include "RHIResources.h"

struct FDvuiHostVertex;

/** One indexed-triangle draw command, in dvui canvas pixel space. */
struct FDvuiDrawCmd
{
	uint32 VertexOffset = 0;   // index into the per-frame vertex buffer
	uint32 VertexCount  = 0;
	uint32 IndexOffset  = 0;
	uint32 IndexCount   = 0;
	// Hold a strong RHI ref, not a raw pointer. dvui rebuilds its font/icon
	// atlas periodically — when textureDestroy fires for the old atlas, the
	// UTexture2D is unrooted and may be GC'd before the render thread has
	// consumed in-flight elements that reference it. The refcounted ref
	// keeps the underlying GPU texture alive until the element is gone.
	FTextureRHIRef Texture;
	bool bHasClipRect = false;
	FIntRect ClipRect;             // in canvas pixels (origin top-left)
};

/**
 * One-shot custom Slate element built from the snapshot of dvui draw data
 * for a single frame. Lives until Draw_RenderThread runs (Slate keeps the
 * shared pointer alive).
 *
 * Render-thread-side state is owned and immutable; safe to pipeline.
 */
class FDvuiCustomElement : public ICustomSlateElement, public TSharedFromThis<FDvuiCustomElement, ESPMode::ThreadSafe>
{
public:
	FDvuiCustomElement(
		TArray<FDvuiHostVertex>&& InVertices,
		TArray<uint32>&& InIndices,
		TArray<FDvuiDrawCmd>&& InCommands,
		FIntPoint InCanvasSize,
		FIntRect InWidgetRect);

	virtual void Draw_RenderThread(FRDGBuilder& GraphBuilder, const FDrawPassInputs& Inputs) override;

private:
	TArray<FDvuiHostVertex> Vertices;
	TArray<uint32>          Indices;
	TArray<FDvuiDrawCmd>    Commands;
	FIntPoint CanvasSize;   // dvui canvas in pixels (matches widget local size)
	FIntRect  WidgetRect;   // widget rect in slate output pixels
};
