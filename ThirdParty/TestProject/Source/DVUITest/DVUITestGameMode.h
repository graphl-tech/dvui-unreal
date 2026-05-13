#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DVUITestGameMode.generated.h"

class SDVUIWidget;

/**
 * Headless test GameMode for the DVUIUnreal plugin.
 *
 * Default: interactive — adds the dvui widget to the viewport and ticks.
 * `-DvuiAutoScreenshot`: take a screenshot after WarmupFrames, then quit.
 * `-DvuiClickTest`: take a before-shot, inject a synthetic click via the
 * input ABI, take an after-shot. The two PNGs verify input plumbing.
 * `-DvuiRunSeconds=N`: bench mode — tick for N seconds, log FPS, quit.
 */
UCLASS()
class DVUITEST_API ADVUITestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADVUITestGameMode();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "DVUITest")
	int32 WarmupFrames = 30;

	/** Frames to wait between requesting a screenshot and quitting. */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	int32 QuitDelayFrames = 60;

	/** If true, take screenshot(s) then quit. Off by default so the
	 *  interactive windowed run keeps the IDE up until you close it.
	 *  Enable from the command line with `-DvuiAutoScreenshot`. */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	bool bAutoScreenshotAndQuit = false;

	/** If true (only meaningful with bAutoScreenshotAndQuit), inject a
	 *  click between two screenshots. Enable with `-DvuiClickTest`. */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	bool bSyntheticClickTest = false;

	/** Click position in widget-local pixels (matches dvui canvas size).
	 *  Defaults match the sample app pinning its window to (100,100,400,400)
	 *  with a 200x60 button. */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	FVector2D ClickPosition = FVector2D(200, 220);

	UPROPERTY(EditAnywhere, Category = "DVUITest")
	FString ScreenshotBaseName = TEXT("smoke");

	/** If > 0, runs for this many seconds then quits. Useful for FPS
	 *  measurement runs without screenshots. Set via -DvuiRunSeconds=N. */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	int32 BenchSeconds = 0;

	/** Skip UE's 3D world render pass — we're a UI-only app and don't need
	 *  Lumen / atmosphere / post-process running on an empty scene. Off by
	 *  default skips the cost. Pass -DvuiEnableWorldRendering to turn it
	 *  back on (e.g. to A/B test the FPS impact). */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	bool bEnableWorldRendering = false;

	/** Disable r.OneFrameThreadLag (default-on by UE) so the game thread
	 *  syncs with render thread each frame. Costs throughput, gains
	 *  latency — input → display drops by one frame. Recommended when
	 *  rendering a world (otherwise the UI input lag compounds with the
	 *  world's render time). Pass -DvuiHighLatencyMode to disable. */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	bool bLowLatencyMode = true;

	/** Apply a CVar bundle that strips Lumen / heavy post-process / TAA
	 *  etc. so the world render pass stays cheap. Pass with
	 *  -DvuiEnableWorldRendering to keep the world visible but fast. */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	bool bLightweightWorldRender = true;

	/** If true, instead of attaching the SDVUIWidget directly to the
	 *  viewport, construct a UDVUIWidget (UMG wrapper) inside a host
	 *  UUserWidget and add THAT to the viewport. Verifies the UMG /
	 *  blueprint integration path. Pass -DvuiUMGPath. */
	UPROPERTY(EditAnywhere, Category = "DVUITest")
	bool bUseUMGPath = false;

private:
	int32 FramesSeen = 0;
	int32 PhaseFrame = 0;

	// FPS / timing instrumentation
	double FpsWindowStartSeconds = 0.0;
	int32  FpsWindowFrames = 0;

	enum class EPhase : uint8
	{
		Warmup,            // ticking, waiting for warmup
		ScreenshotBefore,  // request screenshot 1
		WaitAfterShot1,    // wait for capture
		InjectClick,       // synthesize input
		WaitAfterClick,    // let dvui apply click + repaint
		ScreenshotAfter,   // request screenshot 2
		WaitAfterShot2,
		Quit,
	};
	EPhase Phase = EPhase::Warmup;
	int32 ShotCount = 0;
	bool bQuitRequested = false;

	void OnPostRenderTick();
	void OnScreenshotCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors);
	void RequestScreenshotWithSuffix(const FString& Suffix);

	FString PendingScreenshotName;
	bool bAwaitingCapture = false;

	FDelegateHandle PostRenderHandle;
	FDelegateHandle OnScreenshotCapturedHandle;
	FDelegateHandle BeginFrameHandle;
	double          LastBeginFrameSec = 0.0;
};
