// Copyright Epic Games, Inc. All Rights Reserved.

#include "DVUIRenderer.h"
#include "SDVUIWidget.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Texture2D.h"
#include "Rendering/SlateRenderer.h"
#include "RenderUtils.h"
#include "TextureResource.h"

FDVUIRenderer::FDVUIRenderer()
	: Backend(nullptr)
	, Widget(nullptr)
	, CurrentTargetWidget(nullptr)
	, Width(1920)
	, Height(1080)
	, DpiScale(1.0f)
	, bInitialized(false)
{
}

FDVUIRenderer::~FDVUIRenderer()
{
	Shutdown();
}

void FDVUIRenderer::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (auto& Pair : TextureCache)
	{
		if (Pair.Value.Texture)
		{
			Collector.AddReferencedObject(Pair.Value.Texture);
		}
	}
}

FTextureRHIRef FDVUIRenderer::GetRHITextureForDvuiTex(uintptr_t TexturePtr) const
{
	if (TexturePtr == 0)
	{
		return FTextureRHIRef();
	}
	if (const FTextureEntry* Entry = TextureCache.Find(TexturePtr))
	{
		if (Entry->Texture && Entry->Texture->GetResource())
		{
			return Entry->Texture->GetResource()->TextureRHI;
		}
	}
	return FTextureRHIRef();
}

bool FDVUIRenderer::Initialize()
{
	if (bInitialized)
	{
		return true;
	}

	if (!CreateWidget())
	{
		UE_LOG(LogTemp, Error, TEXT("DVUIRenderer: Failed to create widget"));
		return false;
	}

	if (!SetupBackend())
	{
		UE_LOG(LogTemp, Error, TEXT("DVUIRenderer: Failed to setup DVUI backend"));
		Widget.Reset();
		return false;
	}

	bInitialized = true;
	return true;
}

void FDVUIRenderer::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	// Let dvui widgets run close handlers before tearing the backend down.
	if (Backend)
	{
		dvui_event_window_close(Backend);
	}

	ShutdownBackend();
	Widget.Reset();

	bInitialized = false;
}

void FDVUIRenderer::BeginFrame(SDVUIWidget* TargetWidget)
{
	if (!bInitialized || !Backend || !TargetWidget)
	{
		return;
	}

	CurrentTargetWidget = TargetWidget;
	CurrentTargetWidget->ClearDrawData();
}

void FDVUIRenderer::EndFrame()
{
}

void FDVUIRenderer::RenderFrame()
{
	if (!bInitialized || !Backend)
	{
		return;
	}
	dvui_backend_render_frame(Backend);
}

bool FDVUIRenderer::CreateWidget()
{
	Widget = SNew(SDVUIWidget)
		.DVUIRenderer(SharedThis(this));
	if (!Widget.IsValid())
	{
		return false;
	}
	Widget->SetCanvasSize(FVector2D(Width, Height));
	return true;
}

bool FDVUIRenderer::SetupBackend()
{
	DVUIUnrealCallbacks callbacks;
	callbacks.render_triangles = &FDVUIRenderer::RenderTrianglesCallback;
	callbacks.get_time_ns = &FDVUIRenderer::GetTimeNsCallback;
	callbacks.get_clipboard = &FDVUIRenderer::GetClipboardCallback;
	callbacks.set_clipboard = &FDVUIRenderer::SetClipboardCallback;
	callbacks.get_dpi_scale = &FDVUIRenderer::GetDpiScaleCallback;
	callbacks.get_pixel_size = &FDVUIRenderer::GetPixelSizeCallback;
	callbacks.texture_create = &FDVUIRenderer::TextureCreateCallback;
	callbacks.texture_destroy = &FDVUIRenderer::TextureDestroyCallback;

	Backend = dvui_unreal_backend_create(
		reinterpret_cast<DVUIUnrealContext*>(this),
		&callbacks
	);
	return Backend != nullptr;
}

void FDVUIRenderer::ShutdownBackend()
{
	if (Backend)
	{
		dvui_unreal_backend_destroy(Backend);
		Backend = nullptr;
	}
}

void FDVUIRenderer::RenderTriangles(
	const DVUIVertex* vertices,
	uint32_t vertex_count,
	const uint32_t* indices,
	uint32_t index_count,
	uintptr_t texture_ptr,
	const float* clip_rect)
{
	if (!bInitialized || !CurrentTargetWidget)
	{
		return;
	}
	CurrentTargetWidget->QueueDrawData(vertices, vertex_count, indices, index_count, texture_ptr, clip_rect);
}

void FDVUIRenderer::RenderTrianglesCallback(
	DVUIUnrealContext* ctx,
	const DVUIVertex* vertices,
	uint32_t vertex_count,
	const uint32_t* indices,
	uint32_t index_count,
	uintptr_t texture_ptr,
	const float* clip_rect)
{
	if (auto* renderer = reinterpret_cast<FDVUIRenderer*>(ctx))
	{
		renderer->RenderTriangles(vertices, vertex_count, indices, index_count, texture_ptr, clip_rect);
	}
}

uint64_t FDVUIRenderer::GetTimeNsCallback(DVUIUnrealContext*)
{
	return static_cast<uint64_t>(FPlatformTime::Seconds() * 1000000000.0);
}

const char* FDVUIRenderer::GetClipboardCallback(DVUIUnrealContext* ctx)
{
	auto* renderer = reinterpret_cast<FDVUIRenderer*>(ctx);
	if (!renderer) return "";
	FPlatformApplicationMisc::ClipboardPaste(renderer->ClipboardText);
	auto Utf8 = StringCast<ANSICHAR>(*renderer->ClipboardText);
	renderer->ClipboardUTF8.Reset();
	renderer->ClipboardUTF8.Append(Utf8.Get(), Utf8.Length());
	renderer->ClipboardUTF8.Add('\0');
	return renderer->ClipboardUTF8.GetData();
}

void FDVUIRenderer::SetClipboardCallback(DVUIUnrealContext*, const char* text)
{
	if (text)
	{
		FPlatformApplicationMisc::ClipboardCopy(*FString(UTF8_TO_TCHAR(text)));
	}
}

float FDVUIRenderer::GetDpiScaleCallback(DVUIUnrealContext* ctx)
{
	auto* renderer = reinterpret_cast<FDVUIRenderer*>(ctx);
	if (!renderer) return 1.0f;
	// Per-widget Scale (set from UMG Details panel) wins over the
	// renderer-global default. Same pattern as GetPixelSizeCallback.
	SDVUIWidget* Active = renderer->CurrentTargetWidget
		? renderer->CurrentTargetWidget
		: renderer->Widget.Get();
	if (Active)
	{
		const float S = Active->GetScale();
		if (S > 0.0f) return S;
	}
	return renderer->DpiScale;
}

void FDVUIRenderer::GetPixelSizeCallback(DVUIUnrealContext* ctx, uint32_t* width, uint32_t* height)
{
	auto* renderer = reinterpret_cast<FDVUIRenderer*>(ctx);
	if (!renderer || !width || !height)
	{
		return;
	}

	// Read from the widget that's CURRENTLY rendering (set by BeginFrame).
	// In the UMG path each UDVUIWidget creates its own SDVUIWidget — the
	// renderer's default Widget is never painted there, so its size would
	// be stale and dvui would extend past the actual UMG bounds.
	FVector2D Size(renderer->Width, renderer->Height);
	SDVUIWidget* Active = renderer->CurrentTargetWidget
		? renderer->CurrentTargetWidget
		: renderer->Widget.Get();
	if (Active)
	{
		const FVector2D Painted = Active->GetLastPaintedSize();
		if (Painted.X > 0.0f && Painted.Y > 0.0f)
		{
			Size = Painted;
		}
	}
	*width = static_cast<uint32_t>(FMath::Max(1.0, Size.X));
	*height = static_cast<uint32_t>(FMath::Max(1.0, Size.Y));
}

void FDVUIRenderer::TextureCreateCallback(
	DVUIUnrealContext* ctx,
	uintptr_t texture_id,
	const uint8_t* pixels,
	uint32_t width,
	uint32_t height)
{
	if (auto* renderer = reinterpret_cast<FDVUIRenderer*>(ctx))
	{
		renderer->OnTextureCreate(texture_id, pixels, width, height);
	}
}

void FDVUIRenderer::TextureDestroyCallback(DVUIUnrealContext* ctx, uintptr_t texture_id)
{
	if (auto* renderer = reinterpret_cast<FDVUIRenderer*>(ctx))
	{
		renderer->OnTextureDestroy(texture_id);
	}
}

void FDVUIRenderer::OnTextureCreate(uintptr_t texture_id, const uint8_t* pixels, uint32_t width, uint32_t height)
{
	if (texture_id == 0 || !pixels || width == 0 || height == 0)
	{
		return;
	}

	// dvui sends RGBA bytes (R first); UE wants BGRA8. Swizzle while uploading.
	UTexture2D* Tex = UTexture2D::CreateTransient(
		static_cast<int32>(width),
		static_cast<int32>(height),
		PF_B8G8R8A8);
	if (!Tex)
	{
		UE_LOG(LogTemp, Error, TEXT("[DVUI] CreateTransient failed for %ux%u"), width, height);
		return;
	}

	Tex->Filter = TF_Bilinear;
	// dvui packs font/icon atlases as PMA-white (R=G=B=A=alpha) using raw
	// stb_truetype anti-aliasing values — must sample as raw bytes, not
	// sRGB-decoded. SRGB=false alone isn't always honored for PF_B8G8R8A8;
	// TC_VectorDisplacementmap forces non-sRGB sampling.
	Tex->SRGB = false;
	Tex->CompressionSettings = TC_VectorDisplacementmap;
	Tex->AddToRoot();

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	uint8* Dest = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	const uint64 PixelCount = static_cast<uint64>(width) * height;
	for (uint64 i = 0; i < PixelCount; ++i)
	{
		const uint8 R = pixels[i * 4 + 0];
		const uint8 G = pixels[i * 4 + 1];
		const uint8 B = pixels[i * 4 + 2];
		const uint8 A = pixels[i * 4 + 3];
		Dest[i * 4 + 0] = B;
		Dest[i * 4 + 1] = G;
		Dest[i * 4 + 2] = R;
		Dest[i * 4 + 3] = A;
	}
	Mip.BulkData.Unlock();
	Tex->UpdateResource();

	// UpdateResource queues GPU init asynchronously on the render thread.
	// dvui starts using the texture in this same frame so without a sync
	// the first frame samples an empty FRHITexture → white square.
	FlushRenderingCommands();

	FTextureEntry Entry;
	Entry.Texture = Tex;
	Entry.Brush = MakeShared<FSlateBrush>();
	Entry.Brush->SetResourceObject(Tex);
	Entry.Brush->ImageSize = FVector2D(width, height);
	Entry.Brush->DrawAs = ESlateBrushDrawType::Image;
	Entry.Brush->TintColor = FSlateColor(FLinearColor::White);

	TextureCache.Add(texture_id, Entry);
}

void FDVUIRenderer::OnTextureDestroy(uintptr_t texture_id)
{
	if (FTextureEntry* Entry = TextureCache.Find(texture_id))
	{
		// Filters the shutdown path where UObject hash tables are torn down
		// before our module's ShutdownModule.
		if (Entry->Texture && !IsEngineExitRequested())
		{
			Entry->Texture->RemoveFromRoot();
		}
		TextureCache.Remove(texture_id);
	}
}

void FDVUIRenderer::SendMouseMotion(float x, float y)
{
	if (Backend) { dvui_event_mouse_motion(Backend, x, y); }
}

void FDVUIRenderer::SendMouseButton(int button, bool pressed)
{
	if (Backend) { dvui_event_mouse_button(Backend, button, pressed ? 1 : 0); }
}

void FDVUIRenderer::SendMouseWheel(float dx, float dy)
{
	if (Backend) { dvui_event_mouse_wheel(Backend, dx, dy); }
}

void FDVUIRenderer::SendKey(int key, bool pressed, int mods)
{
	if (Backend) { dvui_event_key(Backend, key, pressed ? 1 : 0, mods); }
}

void FDVUIRenderer::SendText(const FString& Text)
{
	if (!Backend) return;
	const FTCHARToUTF8 Utf8(*Text);
	dvui_event_text(Backend, Utf8.Get(), static_cast<uint32_t>(Utf8.Length()));
}

void FDVUIRenderer::SendWindowClose()
{
	if (Backend) { dvui_event_window_close(Backend); }
}

EMouseCursor::Type FDVUIRenderer::GetRequestedCursor() const
{
	if (!Backend)
	{
		return EMouseCursor::Default;
	}
	const int Code = dvui_cursor_requested(const_cast<DVUIUnrealBackend*>(Backend));
	switch (Code)
	{
	case DVUI_CURSOR_ARROW:      return EMouseCursor::Default;
	case DVUI_CURSOR_IBEAM:      return EMouseCursor::TextEditBeam;
	case DVUI_CURSOR_WAIT:       return EMouseCursor::Default;
	case DVUI_CURSOR_WAIT_ARROW: return EMouseCursor::Default;
	case DVUI_CURSOR_CROSSHAIR:  return EMouseCursor::Crosshairs;
	case DVUI_CURSOR_NW_SE:      return EMouseCursor::ResizeSouthEast;
	case DVUI_CURSOR_NE_SW:      return EMouseCursor::ResizeSouthWest;
	case DVUI_CURSOR_W_E:        return EMouseCursor::ResizeLeftRight;
	case DVUI_CURSOR_N_S:        return EMouseCursor::ResizeUpDown;
	case DVUI_CURSOR_ALL:        return EMouseCursor::CardinalCross;
	case DVUI_CURSOR_BAD:        return EMouseCursor::SlashedCircle;
	case DVUI_CURSOR_HAND:       return EMouseCursor::Hand;
	case DVUI_CURSOR_HIDDEN:     return EMouseCursor::None;
	default:                     return EMouseCursor::Default;
	}
}

bool FDVUIRenderer::IsTextInputActive() const
{
	if (!Backend) return false;
	return dvui_text_input_active(const_cast<DVUIUnrealBackend*>(Backend)) != 0;
}
