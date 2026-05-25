// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DVUI.h"
#include "Brushes/SlateImageBrush.h"
#include "UObject/GCObject.h"
#include "GenericPlatform/ICursor.h"

class SDVUIWidget;
class UTexture2D;

/**
 * Bridges the Zig DVUI backend with the Slate widget(s) that paint it.
 *
 * Owns one dvui.Window (and one app instance) plus the texture cache. Each
 * UDVUIWidget creates and owns its own renderer so DVUI state is isolated
 * per widget instance; the module also keeps a shared singleton renderer for
 * the non-UMG GameMode path. Multiple SDVUIWidget instances may share a
 * renderer (the active one is set per BeginFrame), but a single renderer
 * drives a single dvui.Window.
 */
class DVUIUNREAL_API FDVUIRenderer : public TSharedFromThis<FDVUIRenderer>, public FGCObject
{
public:
	FDVUIRenderer();
	virtual ~FDVUIRenderer();

	bool Initialize();
	void Shutdown();

	/** Begin a new DVUI frame for a specific widget. */
	void BeginFrame(SDVUIWidget* TargetWidget);
	void EndFrame();

	/** The default Slate widget owned by the renderer (used when not
	 *  going through the UMG wrapper). */
	TSharedPtr<SDVUIWidget> GetWidget() const { return Widget; }

	/** Run one dvui frame: window.begin → app.frame → window.end. The
	 *  drawClippedTriangles callbacks fire synchronously from inside this
	 *  call and queue draw data on the active SDVUIWidget. */
	void RenderFrame();

	/** Resolve a dvui texture id (the opaque handle dvui hands us in
	 *  drawClippedTriangles) to the GPU resource. Returns an empty ref
	 *  when unknown. The ref keeps the underlying RHI texture alive even
	 *  if dvui evicts its own copy mid-flight. */
	FTextureRHIRef GetRHITextureForDvuiTex(uintptr_t TexturePtr) const;

	DVUIUnrealBackend* GetBackend() const { return Backend; }

	// Input forwarding — coordinates are in dvui canvas pixels.
	void SendMouseMotion(float x, float y);
	void SendMouseButton(int button, bool pressed);
	void SendMouseWheel(float dx, float dy);
	void SendKey(int key, bool pressed, int mods);
	void SendText(const FString& Text);
	void SendWindowClose();

	EMouseCursor::Type GetRequestedCursor() const;

	/** True when dvui has a focused text input — host should suppress its
	 *  own hotkeys / pass-through key handling while typing. */
	bool IsTextInputActive() const;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return TEXT("FDVUIRenderer"); }

	// Static C-ABI callbacks invoked from Zig.
	static void RenderTrianglesCallback(
		DVUIUnrealContext* ctx,
		const DVUIVertex* vertices,
		uint32_t vertex_count,
		const uint32_t* indices,
		uint32_t index_count,
		uintptr_t texture_ptr,
		const float* clip_rect
	);
	static uint64_t GetTimeNsCallback(DVUIUnrealContext* ctx);
	static const char* GetClipboardCallback(DVUIUnrealContext* ctx);
	static void SetClipboardCallback(DVUIUnrealContext* ctx, const char* text);
	static float GetDpiScaleCallback(DVUIUnrealContext* ctx);
	static void GetPixelSizeCallback(DVUIUnrealContext* ctx, uint32_t* width, uint32_t* height);
	static void TextureCreateCallback(
		DVUIUnrealContext* ctx,
		uintptr_t texture_id,
		const uint8_t* pixels,
		uint32_t width,
		uint32_t height
	);
	static void TextureDestroyCallback(DVUIUnrealContext* ctx, uintptr_t texture_id);

private:
	DVUIUnrealBackend* Backend;
	TSharedPtr<SDVUIWidget> Widget;
	SDVUIWidget* CurrentTargetWidget;

	uint32 Width;
	uint32 Height;
	float DpiScale;

	FString ClipboardText;
	TArray<char> ClipboardUTF8;

	bool bInitialized;

	struct FTextureEntry
	{
		// TObjectPtr (not raw UTexture2D*) so AddReferencedObject hits the
		// non-deprecated overload — the raw-pointer one is unsafe under
		// incremental GC.
		TObjectPtr<UTexture2D> Texture = nullptr;
		TSharedPtr<FSlateBrush> Brush;
	};
	TMap<uintptr_t, FTextureEntry> TextureCache;

	bool CreateWidget();
	bool SetupBackend();
	void ShutdownBackend();

	void RenderTriangles(
		const DVUIVertex* vertices,
		uint32_t vertex_count,
		const uint32_t* indices,
		uint32_t index_count,
		uintptr_t texture_ptr,
		const float* clip_rect
	);

	void OnTextureCreate(uintptr_t texture_id, const uint8_t* pixels, uint32_t width, uint32_t height);
	void OnTextureDestroy(uintptr_t texture_id);

	friend class SDVUIWidget;
};
