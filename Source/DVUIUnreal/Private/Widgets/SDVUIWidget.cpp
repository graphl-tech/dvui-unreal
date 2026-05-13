// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDVUIWidget.h"
#include "DVUIRenderer.h"
#include "DvuiCustomElement.h"
#include "DvuiShader.h"
#include "Rendering/DrawElements.h"
#include "Input/Events.h"
#include "GenericPlatform/GenericApplication.h"

namespace
{
	// Mirror DVUI_KEY_* / DVUI_MOD_* / DVUI_BTN_* in DVUI.h.
	constexpr int DvuiKeyBackspace = 1;
	constexpr int DvuiKeyTab = 2;
	constexpr int DvuiKeyEnter = 3;
	constexpr int DvuiKeyEscape = 4;
	constexpr int DvuiKeySpace = 5;
	constexpr int DvuiKeyLeft = 10;
	constexpr int DvuiKeyRight = 11;
	constexpr int DvuiKeyUp = 12;
	constexpr int DvuiKeyDown = 13;
	constexpr int DvuiKeyHome = 14;
	constexpr int DvuiKeyEnd = 15;
	constexpr int DvuiKeyPageUp = 16;
	constexpr int DvuiKeyPageDown = 17;
	constexpr int DvuiKeyInsert = 18;
	constexpr int DvuiKeyDelete = 19;
	constexpr int DvuiKeyF1 = 30;
	constexpr int DvuiKeyLShift = 50;
	constexpr int DvuiKeyRShift = 51;
	constexpr int DvuiKeyLCtrl  = 52;
	constexpr int DvuiKeyRCtrl  = 53;
	constexpr int DvuiKeyLAlt   = 54;
	constexpr int DvuiKeyRAlt   = 55;
	constexpr int DvuiKeyLCmd   = 56;
	constexpr int DvuiKeyRCmd   = 57;
	constexpr int DvuiKeyABase = 100;
	constexpr int DvuiKeyNumBase = 200;

	constexpr int DvuiModShift = 1;
	constexpr int DvuiModCtrl = 2;
	constexpr int DvuiModAlt = 4;
	constexpr int DvuiModCmd = 8;

	constexpr int DvuiBtnLeft = 1;
	constexpr int DvuiBtnMiddle = 2;
	constexpr int DvuiBtnRight = 3;

	int MapMouseButton(const FKey& Key)
	{
		if (Key == EKeys::LeftMouseButton)   { return DvuiBtnLeft; }
		if (Key == EKeys::MiddleMouseButton) { return DvuiBtnMiddle; }
		if (Key == EKeys::RightMouseButton)  { return DvuiBtnRight; }
		return 0;
	}

	int MapKeyToDvui(const FKey& Key)
	{
		if (Key == EKeys::BackSpace) return DvuiKeyBackspace;
		if (Key == EKeys::Tab)       return DvuiKeyTab;
		if (Key == EKeys::Enter)     return DvuiKeyEnter;
		if (Key == EKeys::Escape)    return DvuiKeyEscape;
		if (Key == EKeys::SpaceBar)  return DvuiKeySpace;
		if (Key == EKeys::Left)      return DvuiKeyLeft;
		if (Key == EKeys::Right)     return DvuiKeyRight;
		if (Key == EKeys::Up)        return DvuiKeyUp;
		if (Key == EKeys::Down)      return DvuiKeyDown;
		if (Key == EKeys::Home)      return DvuiKeyHome;
		if (Key == EKeys::End)       return DvuiKeyEnd;
		if (Key == EKeys::PageUp)    return DvuiKeyPageUp;
		if (Key == EKeys::PageDown)  return DvuiKeyPageDown;
		if (Key == EKeys::Insert)    return DvuiKeyInsert;
		if (Key == EKeys::Delete)    return DvuiKeyDelete;

		// Modifier keys must be forwarded — without them dvui's internal
		// modifier state stays at 0 and Ctrl+click / Shift+select don't work.
		if (Key == EKeys::LeftShift)    return DvuiKeyLShift;
		if (Key == EKeys::RightShift)   return DvuiKeyRShift;
		if (Key == EKeys::LeftControl)  return DvuiKeyLCtrl;
		if (Key == EKeys::RightControl) return DvuiKeyRCtrl;
		if (Key == EKeys::LeftAlt)      return DvuiKeyLAlt;
		if (Key == EKeys::RightAlt)     return DvuiKeyRAlt;
		if (Key == EKeys::LeftCommand)  return DvuiKeyLCmd;
		if (Key == EKeys::RightCommand) return DvuiKeyRCmd;

		for (int i = 0; i < 12; ++i)
		{
			static const FKey FKeys[12] = {
				EKeys::F1,  EKeys::F2,  EKeys::F3,  EKeys::F4,
				EKeys::F5,  EKeys::F6,  EKeys::F7,  EKeys::F8,
				EKeys::F9,  EKeys::F10, EKeys::F11, EKeys::F12,
			};
			if (Key == FKeys[i]) { return DvuiKeyF1 + i; }
		}

		const FString Name = Key.GetFName().ToString();
		if (Name.Len() == 1)
		{
			const TCHAR Ch = Name[0];
			if (Ch >= TEXT('A') && Ch <= TEXT('Z'))
			{
				return DvuiKeyABase + (Ch - TEXT('A'));
			}
			if (Ch >= TEXT('0') && Ch <= TEXT('9'))
			{
				return DvuiKeyNumBase + (Ch - TEXT('0'));
			}
		}
		return 0;
	}

	int PackMods(const FInputEvent& Event)
	{
		int mods = 0;
		if (Event.IsShiftDown())   mods |= DvuiModShift;
		if (Event.IsControlDown()) mods |= DvuiModCtrl;
		if (Event.IsAltDown())     mods |= DvuiModAlt;
		if (Event.IsCommandDown()) mods |= DvuiModCmd;
		return mods;
	}

	// dvui packs colors as 0xAABBGGRR (R first byte on LE). VET_Color reads
	// memory as B,G,R,A — so we swizzle once on copy.
	FORCEINLINE uint32 SwizzleRGBAtoBGRA(uint32 Packed)
	{
		const uint8 R = (Packed >>  0) & 0xFF;
		const uint8 G = (Packed >>  8) & 0xFF;
		const uint8 B = (Packed >> 16) & 0xFF;
		const uint8 A = (Packed >> 24) & 0xFF;
		return (uint32(A) << 24) | (uint32(R) << 16) | (uint32(G) << 8) | uint32(B);
	}
}

void SDVUIWidget::Construct(const FArguments& InArgs)
{
	DVUIRenderer = InArgs._DVUIRenderer;
	CanvasSize = FVector2D(1920.0f, 1080.0f);
	// MUST be Visible (not SelfHitTestInvisible). SelfHitTestInvisible
	// excludes this widget from hit-testing — only children would receive
	// events, and we have none. Mouse moves and OnCursorQuery would never
	// fire without this.
	SetVisibility(EVisibility::Visible);
	bCanSupportFocus = true;
	// Slate caches non-volatile widgets and only repaints on invalidation,
	// so dvui-driven hover effects, scrolling, blinking carets, etc. would
	// never show up. Force volatility so OnPaint runs every frame.
	ForceVolatile(true);
	// Clip to widget bounds — dvui content should never extend past the
	// allotted area even on the very first paint when LastPaintedSize
	// hasn't updated yet.
	SetClipping(EWidgetClipping::ClipToBounds);
}

int32 SDVUIWidget::OnPaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyClippingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	// Report the widget's ABSOLUTE pixel size as the dvui canvas. Mouse
	// events (below) also use absolute pixels, so all coordinate systems
	// stay aligned. Using GetLocalSize would introduce a DPI-scale mismatch:
	// clicks at the visible right edge would land past the visible canvas.
	LastPaintedSize = AllottedGeometry.GetAbsoluteSize();

	// Render dvui JUST-IN-TIME inside OnPaint so any draw cmds emitted by
	// this frame's events reach the screen this same frame. Running it
	// from an OnEndFrame hook would push commands one frame behind paint,
	// adding an input-to-display latency frame.
	FrameVertexBytes.Reset();
	FrameIndices.Reset();
	FrameCommands.Reset();
	if (DVUIRenderer.IsValid())
	{
		DVUIRenderer->BeginFrame(const_cast<SDVUIWidget*>(this));
		DVUIRenderer->RenderFrame();
		DVUIRenderer->EndFrame();
	}

	if (FrameCommands.Num() == 0 || FrameVertexBytes.Num() == 0)
	{
		return LayerId;
	}

	// Build the per-cmd struct the render-thread element needs. Texture
	// pointer → FRHITexture* lookup happens here on the game thread; the
	// element captures FRHITexture* refs that live in our texture cache
	// (kept alive by FGCObject).
	TArray<FDvuiDrawCmd> RenderCmds;
	RenderCmds.Reserve(FrameCommands.Num());
	for (const FFrameDrawCmd& Cmd : FrameCommands)
	{
		FDvuiDrawCmd Out;
		Out.VertexOffset = Cmd.VertexOffset;
		Out.VertexCount  = Cmd.VertexCount;
		Out.IndexOffset  = Cmd.IndexOffset;
		Out.IndexCount   = Cmd.IndexCount;
		Out.Texture      = DVUIRenderer.IsValid()
			? DVUIRenderer->GetRHITextureForDvuiTex(Cmd.TexturePtr) : nullptr;
		Out.bHasClipRect = Cmd.bHasClipRect;

		// Convert clip from canvas-local to slate-output pixels.
		const FSlateRect WidgetRectF = AllottedGeometry.GetLayoutBoundingRect();
		const FIntPoint Origin(FMath::FloorToInt(WidgetRectF.Left), FMath::FloorToInt(WidgetRectF.Top));
		if (Cmd.bHasClipRect)
		{
			Out.ClipRect = FIntRect(
				Origin.X + FMath::FloorToInt(Cmd.ClipRect[0]),
				Origin.Y + FMath::FloorToInt(Cmd.ClipRect[1]),
				Origin.X + FMath::CeilToInt (Cmd.ClipRect[0] + Cmd.ClipRect[2]),
				Origin.Y + FMath::CeilToInt (Cmd.ClipRect[1] + Cmd.ClipRect[3]));
		}
		RenderCmds.Add(Out);
	}

	// Snapshot bytes into a typed array the element will own.
	check(FrameVertexBytes.Num() % sizeof(FDvuiHostVertex) == 0);
	const int32 VtxCount = FrameVertexBytes.Num() / sizeof(FDvuiHostVertex);
	TArray<FDvuiHostVertex> VBSnapshot;
	VBSnapshot.SetNumUninitialized(VtxCount);
	FMemory::Memcpy(VBSnapshot.GetData(), FrameVertexBytes.GetData(), FrameVertexBytes.Num());

	TArray<uint32> IBSnapshot = FrameIndices;

	const FSlateRect WidgetRectF = AllottedGeometry.GetLayoutBoundingRect();
	const FIntRect WidgetRect(
		FMath::FloorToInt(WidgetRectF.Left),
		FMath::FloorToInt(WidgetRectF.Top),
		FMath::CeilToInt (WidgetRectF.Right),
		FMath::CeilToInt (WidgetRectF.Bottom));

	const FIntPoint CanvasPx(
		FMath::Max(1, FMath::CeilToInt(LastPaintedSize.X)),
		FMath::Max(1, FMath::CeilToInt(LastPaintedSize.Y)));

	auto Element = MakeShared<FDvuiCustomElement, ESPMode::ThreadSafe>(
		MoveTemp(VBSnapshot),
		MoveTemp(IBSnapshot),
		MoveTemp(RenderCmds),
		CanvasPx,
		WidgetRect);

	// MakeCustom only stores a TWeakPtr to the element, so hold a strong
	// ref ourselves until the render thread has consumed it. Keep the last
	// few frames so any pipelining the renderer does still sees a live ptr.
	InflightElements.Add(Element);
	while (InflightElements.Num() > 4)
	{
		InflightElements.RemoveAt(0);
	}

	FSlateDrawElement::MakeCustom(OutDrawElements, LayerId, Element);

	// Slate caches the cursor result and only re-queries on real OS mouse
	// events. dvui can change its requested cursor at any time (hovering
	// into a text input → ibeam, into a resize edge → arrow_w_e, etc.)
	// without OS input. So when dvui's cursor differs from what we last
	// told Slate, force a re-query.
	if (DVUIRenderer.IsValid() && FSlateApplication::IsInitialized())
	{
		const int32 NowDvuiCursor = (int32)DVUIRenderer->GetRequestedCursor();
		if (NowDvuiCursor != LastDvuiCursor)
		{
			LastDvuiCursor = NowDvuiCursor;
			FSlateApplication::Get().QueryCursor();
		}
	}

	return LayerId + 1;
}

FVector2D SDVUIWidget::ComputeDesiredSize(float) const
{
	return CanvasSize;
}

void SDVUIWidget::SetCanvasSize(FVector2D NewSize)
{
	CanvasSize = NewSize;
}

void SDVUIWidget::QueueDrawData(
	const DVUIVertex* Vertices,
	uint32 VertexCount,
	const uint32_t* Indices,
	uint32 IndexCount,
	uintptr_t TexturePtr,
	const float* ClipRect)
{
	if (!Vertices || VertexCount == 0 || !Indices || IndexCount == 0)
	{
		return;
	}

	// Called synchronously from dvui's frame loop on the game thread,
	// which itself runs inside OnPaint. No mutex needed.
	const uint32 BaseVertex = FrameVertexBytes.Num() / sizeof(FDvuiHostVertex);
	const uint32 BaseIndex  = FrameIndices.Num();

	const int32 NewBytes = (int32)VertexCount * (int32)sizeof(FDvuiHostVertex);
	FrameVertexBytes.AddUninitialized(NewBytes);
	FDvuiHostVertex* Out = reinterpret_cast<FDvuiHostVertex*>(
		FrameVertexBytes.GetData() + FrameVertexBytes.Num() - NewBytes);
	for (uint32 i = 0; i < VertexCount; ++i)
	{
		Out[i].Position[0] = Vertices[i].x;
		Out[i].Position[1] = Vertices[i].y;
		Out[i].UV[0]       = Vertices[i].u;
		Out[i].UV[1]       = Vertices[i].v;
		Out[i].PackedColor = SwizzleRGBAtoBGRA(Vertices[i].color);
	}

	FrameIndices.Reserve(FrameIndices.Num() + IndexCount);
	for (uint32 i = 0; i < IndexCount; ++i)
	{
		FrameIndices.Add(Indices[i]);
	}

	FFrameDrawCmd Cmd;
	Cmd.VertexOffset = BaseVertex;
	Cmd.VertexCount  = VertexCount;
	Cmd.IndexOffset  = BaseIndex;
	Cmd.IndexCount   = IndexCount;
	Cmd.TexturePtr   = TexturePtr;
	if (ClipRect)
	{
		Cmd.bHasClipRect = true;
		Cmd.ClipRect[0] = ClipRect[0];
		Cmd.ClipRect[1] = ClipRect[1];
		Cmd.ClipRect[2] = ClipRect[2];
		Cmd.ClipRect[3] = ClipRect[3];
	}
	FrameCommands.Add(Cmd);
}

void SDVUIWidget::ClearDrawData()
{
	FrameVertexBytes.Reset();
	FrameIndices.Reset();
	FrameCommands.Reset();
}

FReply SDVUIWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DVUIRenderer.IsValid())
	{
		// Use absolute pixel space — matches the canvas size we report to
		// dvui (GetAbsoluteSize, see OnPaint).
		const FVector2D Local = MouseEvent.GetScreenSpacePosition() - MyGeometry.GetAbsolutePosition();
		DVUIRenderer->SendMouseMotion(Local.X, Local.Y);
	}
	return FReply::Handled();
}

FReply SDVUIWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DVUIRenderer.IsValid())
	{
		const FVector2D Local = MouseEvent.GetScreenSpacePosition() - MyGeometry.GetAbsolutePosition();
		DVUIRenderer->SendMouseMotion(Local.X, Local.Y);
		const int Btn = MapMouseButton(MouseEvent.GetEffectingButton());
		if (Btn != 0)
		{
			DVUIRenderer->SendMouseButton(Btn, true);
		}
	}
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SDVUIWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DVUIRenderer.IsValid())
	{
		const FVector2D Local = MouseEvent.GetScreenSpacePosition() - MyGeometry.GetAbsolutePosition();
		DVUIRenderer->SendMouseMotion(Local.X, Local.Y);
		const int Btn = MapMouseButton(MouseEvent.GetEffectingButton());
		if (Btn != 0)
		{
			DVUIRenderer->SendMouseButton(Btn, false);
		}
	}
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SDVUIWidget::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (DVUIRenderer.IsValid())
	{
		constexpr float PixelsPerTick = 80.0f;
		DVUIRenderer->SendMouseWheel(0.0f, MouseEvent.GetWheelDelta() * PixelsPerTick);
	}
	return FReply::Handled();
}

FReply SDVUIWidget::OnKeyDown(const FGeometry&, const FKeyEvent& InKeyEvent)
{
	const int Code = MapKeyToDvui(InKeyEvent.GetKey());
	if (Code != 0 && DVUIRenderer.IsValid())
	{
		DVUIRenderer->SendKey(Code, true, PackMods(InKeyEvent));
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SDVUIWidget::OnKeyUp(const FGeometry&, const FKeyEvent& InKeyEvent)
{
	const int Code = MapKeyToDvui(InKeyEvent.GetKey());
	if (Code != 0 && DVUIRenderer.IsValid())
	{
		DVUIRenderer->SendKey(Code, false, PackMods(InKeyEvent));
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SDVUIWidget::OnKeyChar(const FGeometry&, const FCharacterEvent& InCharacterEvent)
{
	if (DVUIRenderer.IsValid())
	{
		const TCHAR Ch = InCharacterEvent.GetCharacter();
		const FString Text(1, &Ch);
		DVUIRenderer->SendText(Text);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FCursorReply SDVUIWidget::OnCursorQuery(const FGeometry&, const FPointerEvent&) const
{
	if (DVUIRenderer.IsValid())
	{
		return FCursorReply::Cursor(DVUIRenderer->GetRequestedCursor());
	}
	return FCursorReply::Unhandled();
}
