#include "DVUITestGameMode.h"
#include "DVUIUnrealModule.h"
#include "DVUIRenderer.h"
#include "DVUI.h"
#include "DVUIWidget.h"
#include "DvuiTestHostWidget.h"
#include "SDVUIWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"

ADVUITestGameMode::ADVUITestGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ADVUITestGameMode::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[DVUITest] BeginPlay: no world"));
		return;
	}

	// UE's "smooth frame rate" feature dynamically slows tick rate when
	// nothing seems to be happening (defaults to a 22..62 FPS band).
	// dvui-driven UIs have no UE-side "activity", so smoothing pegs us
	// near the minimum — every frame is ~45ms, drag detection takes a
	// handful of frames, total perceived lag is ~0.5s. Disable smoothing
	// and uncap MaxFPS so dvui can repaint at full rate.
	if (GEngine)
	{
		GEngine->bSmoothFrameRate = false;
		if (IConsoleVariable* MaxFps = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
		{
			MaxFps->Set(0);
		}
		// THE big one: when the engine thinks we're "not foreground" it
		// hard-sleeps 100ms per frame in FEngineLoop::Tick (caps us at
		// 10 FPS regardless of anything else). For headless mode there's
		// no window so HasFocus() always returns false; for some windowed
		// setups Slate's focus check also fails. Just disable the throttle.
		if (IConsoleVariable* Idle = IConsoleManager::Get().FindConsoleVariable(TEXT("t.IdleWhenNotForeground")))
		{
			Idle->Set(0);
		}
	}

	UGameViewportClient* Viewport = World->GetGameViewport();
	if (!Viewport || !FDVUIUnrealModule::IsAvailable())
	{
		UE_LOG(LogTemp, Error, TEXT("[DVUITest] viewport or module missing"));
		return;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("DvuiUMGPath")))
	{
		bUseUMGPath = true;
	}

	TSharedPtr<FDVUIRenderer> R = FDVUIUnrealModule::Get().GetRenderer();
	TSharedPtr<SDVUIWidget> Slate = FDVUIUnrealModule::Get().GetWidget();
	if (!R.IsValid() || !Slate.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[DVUITest] renderer/widget not available"));
		return;
	}

	// Skip 3D world rendering — we're UI-only, no need to pay for the
	// sky/lumen/atmosphere/post-process stack on every frame. Pass
	// -DvuiEnableWorldRendering to turn it back on.
	if (FParse::Param(FCommandLine::Get(), TEXT("DvuiEnableWorldRendering")))
	{
		bEnableWorldRendering = true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("DvuiHighLatencyMode")))
	{
		bLowLatencyMode = false;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("DvuiHeavyWorldRender")))
	{
		bLightweightWorldRender = false;
	}

	Viewport->bDisableWorldRendering = !bEnableWorldRendering;
	UE_LOG(LogTemp, Display, TEXT("[DVUITest] world rendering: %s, low-latency: %s, lightweight world: %s"),
		bEnableWorldRendering ? TEXT("ON") : TEXT("OFF"),
		bLowLatencyMode ? TEXT("ON") : TEXT("OFF"),
		bLightweightWorldRender ? TEXT("ON") : TEXT("OFF"));

	auto SetCVar = [](const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value);
		}
	};
	auto SetCVarFloat = [](const TCHAR* Name, float Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value);
		}
	};

	if (bLowLatencyMode)
	{
		// Force game-thread to wait for render-thread completion each
		// frame. Drops throughput but cuts input → display latency by
		// one frame — important when the world render is slow.
		SetCVar(TEXT("r.OneFrameThreadLag"), 0);
	}

	if (bEnableWorldRendering && bLightweightWorldRender)
	{
		// Strip the most expensive default-on rendering features — keeps
		// the world visible (your meshes still render) but skips Lumen,
		// the post-process stack, TAA, atmosphere, etc. Each setter is
		// individually skippable via -DvuiKeep<name> for bisection.
		auto Maybe = [](const TCHAR* SkipFlag, const TCHAR* CVarName, int32 Value)
		{
			if (FParse::Param(FCommandLine::Get(), SkipFlag))
			{
				UE_LOG(LogTemp, Display, TEXT("[DVUITest] keeping %s (skipping override)"), CVarName);
				return;
			}
			if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName))
			{
				CVar->Set(Value);
			}
		};
		Maybe(TEXT("DvuiKeepLumenDiffuse"),     TEXT("r.Lumen.DiffuseIndirect.Allow"), 0);
		Maybe(TEXT("DvuiKeepLumenReflections"), TEXT("r.Lumen.Reflections.Allow"), 0);
		Maybe(TEXT("DvuiKeepAA"),               TEXT("r.AntiAliasingMethod"), 0);
		Maybe(TEXT("DvuiKeepMotionBlur"),       TEXT("r.MotionBlurQuality"), 0);
		Maybe(TEXT("DvuiKeepSSAO"),             TEXT("r.AmbientOcclusionLevels"), 0);
		Maybe(TEXT("DvuiKeepBloom"),            TEXT("r.BloomQuality"), 0);
		Maybe(TEXT("DvuiKeepDOF"),              TEXT("r.DepthOfFieldQuality"), 0);
		Maybe(TEXT("DvuiKeepLightShafts"),      TEXT("r.LightShaftQuality"), 0);
		Maybe(TEXT("DvuiKeepLensFlares"),       TEXT("r.LensFlareQuality"), 0);
		Maybe(TEXT("DvuiKeepSceneFringe"),      TEXT("r.SceneColorFringeQuality"), 0);
		Maybe(TEXT("DvuiKeepEyeAdapt"),         TEXT("r.EyeAdaptationQuality"), 0);
		Maybe(TEXT("DvuiKeepSkyAtmos"),         TEXT("r.SkyAtmosphere"), 0);
		Maybe(TEXT("DvuiKeepVolFog"),           TEXT("r.VolumetricFog"), 0);
		Maybe(TEXT("DvuiKeepTonemapper"),       TEXT("r.Tonemapper.Quality"), 0);
	}

	if (bUseUMGPath)
	{
		// Build a host UDvuiTestHostWidget (concrete UUserWidget subclass)
		// at runtime and drop a UDVUIWidget into its canvas — same path a
		// real user would do in the UMG editor.
		UDvuiTestHostWidget* Host = CreateWidget<UDvuiTestHostWidget>(World, UDvuiTestHostWidget::StaticClass());
		if (Host)
		{
			Host->WidgetTree = NewObject<UWidgetTree>(Host);
			UCanvasPanel* Root = Host->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
			Host->WidgetTree->RootWidget = Root;
			UDVUIWidget* DvuiUmg = Host->WidgetTree->ConstructWidget<UDVUIWidget>(UDVUIWidget::StaticClass(), TEXT("Dvui"));
			DvuiUmg->SetCanvasSize(FVector2D(1280, 720));
			UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(DvuiUmg);
			if (CanvasSlot)
			{
				CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
				CanvasSlot->SetOffsets(FMargin(0.f));
			}
			Host->AddToViewport(0);
			UE_LOG(LogTemp, Display, TEXT("[DVUITest] UMG path: UDVUIWidget added to viewport"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[DVUITest] failed to construct UDvuiTestHostWidget"));
		}
	}
	else
	{
		Viewport->AddViewportWidgetContent(Slate.ToSharedRef(), 1000);
	}

	// In -game mode UE hides the cursor and routes input as game-only by
	// default. Show the cursor and switch to UI-only input so the dvui
	// widget receives mouse moves, clicks, and our OnCursorQuery override
	// gets called when Slate needs to know what cursor to render.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;

		// UI-only mode: every mouse-move event flows to Slate (and our
		// SDVUIWidget) instead of being absorbed by the player controller's
		// game-input pipeline. With FInputModeGameAndUI the cursor only
		// updates on click because game-input mode steals mouse-move events
		// in between, so OnCursorQuery never fires for hover.
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Slate);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}

	// Ensure the platform cursor itself isn't suppressed by some other path.
	if (FSlateApplication::IsInitialized())
	{
		if (TSharedPtr<GenericApplication> App = FSlateApplication::Get().GetPlatformApplication())
		{
			if (App->Cursor.IsValid())
			{
				App->Cursor->Show(true);
			}
		}
	}

	// Command-line overrides for the test phases. Default = interactive
	// (no auto screenshot, no quit). Pass -DvuiAutoScreenshot to run the
	// headless screenshot harness; add -DvuiClickTest to also inject a
	// synthesized click in the middle.
	if (FParse::Param(FCommandLine::Get(), TEXT("DvuiAutoScreenshot")))
	{
		bAutoScreenshotAndQuit = true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("DvuiClickTest")))
	{
		bSyntheticClickTest = true;
		bAutoScreenshotAndQuit = true; // click test implies screenshots
	}
	int32 ParsedSec = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("DvuiRunSeconds="), ParsedSec) && ParsedSec > 0)
	{
		BenchSeconds = ParsedSec;
	}

	UE_LOG(LogTemp, Display,
		TEXT("[DVUITest] DVUI attached; auto_quit=%d click_test=%d warmup=%d quit_delay=%d click_pos=(%.0f,%.0f)"),
		bAutoScreenshotAndQuit ? 1 : 0, bSyntheticClickTest ? 1 : 0,
		WarmupFrames, QuitDelayFrames, ClickPosition.X, ClickPosition.Y);

	OnScreenshotCapturedHandle = Viewport->OnScreenshotCaptured().AddUObject(
		this, &ADVUITestGameMode::OnScreenshotCaptured);
	PostRenderHandle = FCoreDelegates::OnEndFrame.AddUObject(this, &ADVUITestGameMode::OnPostRenderTick);

	// Time OnBeginFrame too so we can split "engine work per frame" vs
	// "wait between frames".
	BeginFrameHandle = FCoreDelegates::OnBeginFrame.AddLambda([this]()
	{
		LastBeginFrameSec = FPlatformTime::Seconds();
	});
}

void ADVUITestGameMode::RequestScreenshotWithSuffix(const FString& Suffix)
{
	PendingScreenshotName = ScreenshotBaseName + Suffix;
	bAwaitingCapture = true;
	UE_LOG(LogTemp, Display, TEXT("[DVUITest] requesting screenshot: %s"), *PendingScreenshotName);
	FScreenshotRequest::RequestScreenshot(PendingScreenshotName, /*bShowUI=*/true, /*bAddUniqueSuffix=*/false);
}

void ADVUITestGameMode::OnPostRenderTick()
{
	// Per-second FPS logging + min/max inter-frame delta.
	const double NowSec = FPlatformTime::Seconds();
	static double LastTickSec = 0.0;
	static double MinDeltaMs = 1e9;
	static double MaxDeltaMs = 0.0;
	if (LastTickSec > 0.0)
	{
		const double DeltaMs = (NowSec - LastTickSec) * 1000.0;
		MinDeltaMs = FMath::Min(MinDeltaMs, DeltaMs);
		MaxDeltaMs = FMath::Max(MaxDeltaMs, DeltaMs);
	}
	LastTickSec = NowSec;
	if (FpsWindowStartSeconds == 0.0)
	{
		FpsWindowStartSeconds = NowSec;
		FpsWindowFrames = 0;
	}
	++FpsWindowFrames;
	// Time the FRAME WORK (BeginFrame to EndFrame) vs the GAP between frames.
	const double FrameWorkMs = (LastBeginFrameSec > 0.0) ? (NowSec - LastBeginFrameSec) * 1000.0 : 0.0;
	static double SumWorkMs = 0.0, SumGapMs = 0.0;
	static int32  SumCount = 0;
	if (FrameWorkMs > 0.0)
	{
		SumWorkMs += FrameWorkMs;
		++SumCount;
	}

	if (NowSec - FpsWindowStartSeconds >= 1.0)
	{
		const double Elapsed = NowSec - FpsWindowStartSeconds;
		const double Fps = FpsWindowFrames / Elapsed;
		const double AvgWorkMs = SumCount > 0 ? SumWorkMs / SumCount : 0;
		const double AvgFrameMs = FpsWindowFrames > 0 ? (Elapsed * 1000.0) / FpsWindowFrames : 0;
		const double AvgGapMs = AvgFrameMs - AvgWorkMs;
		UE_LOG(LogTemp, Display,
			TEXT("[DVUITest] FPS=%.1f frame=%.1fms (work=%.1fms gap=%.1fms) min=%.1f max=%.1f"),
			Fps, AvgFrameMs, AvgWorkMs, AvgGapMs, MinDeltaMs, MaxDeltaMs);
		FpsWindowStartSeconds = NowSec;
		FpsWindowFrames = 0;
		MinDeltaMs = 1e9;
		MaxDeltaMs = 0.0;
		SumWorkMs = 0;
		SumCount = 0;
	}

	++FramesSeen;
	++PhaseFrame;

	// Bench mode: tick for N seconds, log FPS, quit. No screenshots.
	if (BenchSeconds > 0 && !bQuitRequested)
	{
		static double BenchStart = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - BenchStart >= (double)BenchSeconds)
		{
			bQuitRequested = true;
			UE_LOG(LogTemp, Display, TEXT("[DVUITest] bench done after %ds, quitting"), BenchSeconds);
			FCoreDelegates::OnEndFrame.Remove(PostRenderHandle);
			if (UWorld* World = GetWorld())
			{
				GEngine->Exec(World, TEXT("quit"));
			}
		}
		return;
	}

	// Interactive mode: just keep ticking. No phases, no quit, no screenshot.
	if (!bAutoScreenshotAndQuit)
	{
		return;
	}

	switch (Phase)
	{
	case EPhase::Warmup:
		if (PhaseFrame >= WarmupFrames)
		{
			Phase = EPhase::ScreenshotBefore;
			PhaseFrame = 0;
		}
		break;

	case EPhase::ScreenshotBefore:
		RequestScreenshotWithSuffix(bSyntheticClickTest ? TEXT("_before") : TEXT(""));
		Phase = EPhase::WaitAfterShot1;
		PhaseFrame = 0;
		break;

	case EPhase::WaitAfterShot1:
		if (PhaseFrame >= QuitDelayFrames)
		{
			if (bSyntheticClickTest)
			{
				Phase = EPhase::InjectClick;
			}
			else
			{
				Phase = EPhase::Quit;
			}
			PhaseFrame = 0;
		}
		break;

	case EPhase::InjectClick:
	{
		TSharedPtr<FDVUIRenderer> R = FDVUIUnrealModule::Get().GetRenderer();
		if (R.IsValid())
		{
			UE_LOG(LogTemp, Display, TEXT("[DVUITest] injecting click at (%.0f, %.0f)"),
				ClickPosition.X, ClickPosition.Y);
			R->SendMouseMotion(ClickPosition.X, ClickPosition.Y);
			R->SendMouseButton(DVUI_BTN_LEFT, /*pressed=*/true);
			R->SendMouseButton(DVUI_BTN_LEFT, /*pressed=*/false);
		}
		Phase = EPhase::WaitAfterClick;
		PhaseFrame = 0;
		break;
	}

	case EPhase::WaitAfterClick:
		// Give dvui a few frames to process the click and repaint.
		if (PhaseFrame >= 10)
		{
			Phase = EPhase::ScreenshotAfter;
			PhaseFrame = 0;
		}
		break;

	case EPhase::ScreenshotAfter:
		RequestScreenshotWithSuffix(TEXT("_after"));
		Phase = EPhase::WaitAfterShot2;
		PhaseFrame = 0;
		break;

	case EPhase::WaitAfterShot2:
		if (PhaseFrame >= QuitDelayFrames)
		{
			Phase = EPhase::Quit;
			PhaseFrame = 0;
		}
		break;

	case EPhase::Quit:
		if (!bQuitRequested)
		{
			bQuitRequested = true;
			UE_LOG(LogTemp, Display, TEXT("[DVUITest] quitting at frame %d (shots=%d)"),
				FramesSeen, ShotCount);
			FCoreDelegates::OnEndFrame.Remove(PostRenderHandle);
			if (UWorld* World = GetWorld())
			{
				GEngine->Exec(World, TEXT("quit"));
			}
		}
		break;
	}
}

void ADVUITestGameMode::OnScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors)
{
	if (!bAwaitingCapture)
	{
		return;
	}
	bAwaitingCapture = false;
	++ShotCount;

	UE_LOG(LogTemp, Display, TEXT("[DVUITest] OnScreenshotCaptured(%s) %dx%d"),
		*PendingScreenshotName, Width, Height);

	const FString OutPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / (PendingScreenshotName + TEXT(".png"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), /*Tree=*/true);

	TArray64<uint8> Png;
	FImageView Image(Colors.GetData(), Width, Height);
	if (FImageUtils::CompressImage(Png, TEXT("png"), Image, /*Quality=*/0))
	{
		if (FFileHelper::SaveArrayToFile(Png, *OutPath))
		{
			UE_LOG(LogTemp, Display, TEXT("[DVUITest] wrote PNG to %s (%lld bytes)"),
				*OutPath, (int64)Png.Num());
		}
	}
}
