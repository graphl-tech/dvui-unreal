#!/usr/bin/env bash
# Bisect which "lightweight" rendering CVar contributes most to FPS when
# the world is rendered. Baseline = world ON + UE defaults (heavy).
# For each CVar in turn we override JUST THAT ONE and measure FPS.

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
TEST_PROJECT="$PLUGIN_ROOT/ThirdParty/TestProject/DVUITest.uproject"
UNREAL_ENGINE="${UNREAL_ENGINE:-$HOME/.local/share/unreal/Engine}"
EDITOR_CMD="$UNREAL_ENGINE/Binaries/Linux/UnrealEditor-Cmd"
RUN_SECONDS="${RUN_SECONDS:-5}"

LOG_DIR="$PLUGIN_ROOT/ThirdParty/TestProject/Saved/Logs"
mkdir -p "$LOG_DIR"

run_one() {
	local label="$1"
	local extra_cvars="$2"
	local log_file="$LOG_DIR/bisect-$label.log"
	# Heavy mode (no lightweight bundle) + just the one override under test
	"$EDITOR_CMD" \
		"$TEST_PROJECT" \
		-game -RenderOffScreen -ResX=1280 -ResY=720 -windowed -unattended -NoSound \
		-stdout -FullStdOutLogOutput -AllowStdOutLogVerbosity \
		-LogCmds="LogTemp Verbose" \
		-Log="$(basename "$log_file")" \
		-DvuiRunSeconds="$RUN_SECONDS" \
		-DvuiEnableWorldRendering \
		-DvuiHeavyWorldRender \
		-ExecCmds="t.MaxFPS 1000;r.VSync 0;t.IdleWhenNotForeground 0;Slate.AllowSlateToSleep 0;$extra_cvars" \
		>/dev/null 2>&1 || true
	# Average FPS (skip first sample which includes startup spike).
	local fps_avg
	fps_avg=$(grep -oE "FPS=[0-9.]+" "$log_file" | tail -n +2 | sed 's/FPS=//' \
		| awk '{ sum += $1; n++ } END { if (n > 0) printf "%.1f", sum / n; else print "?" }')
	printf "  %-50s %s\n" "$label" "$fps_avg"
}

echo "=== bisecting lightweight render CVars (world ON, ${RUN_SECONDS}s each) ==="
echo "  CVar                                                FPS"
echo "  ---------------------------------------------------- ----"
run_one "baseline (heavy, all default)"          ""
run_one "ONE_FRAME_THREAD_LAG_OFF only"           "r.OneFrameThreadLag 0"
run_one "lumen_diffuse_off"                      "r.Lumen.DiffuseIndirect.Allow 0"
run_one "lumen_reflections_off"                  "r.Lumen.Reflections.Allow 0"
run_one "lumen_both_off"                         "r.Lumen.DiffuseIndirect.Allow 0;r.Lumen.Reflections.Allow 0"
run_one "antialiasing_off"                       "r.AntiAliasingMethod 0"
run_one "motionblur_off"                         "r.MotionBlurQuality 0"
run_one "ssao_off"                               "r.AmbientOcclusionLevels 0"
run_one "bloom_off"                              "r.BloomQuality 0"
run_one "depthoffield_off"                       "r.DepthOfFieldQuality 0"
run_one "lightshafts_off"                        "r.LightShaftQuality 0"
run_one "lensflares_off"                         "r.LensFlareQuality 0"
run_one "scenecolorfringe_off"                   "r.SceneColorFringeQuality 0"
run_one "eyeadapt_off"                           "r.EyeAdaptationQuality 0"
run_one "skyatmosphere_off"                      "r.SkyAtmosphere 0"
run_one "volumetricfog_off"                      "r.VolumetricFog 0"
run_one "tonemapper_off"                         "r.Tonemapper.Quality 0"
run_one "screenpct_50"                           "r.ScreenPercentage 50"
run_one "ALL lightweight bundle"                 "r.Lumen.DiffuseIndirect.Allow 0;r.Lumen.Reflections.Allow 0;r.AntiAliasingMethod 0;r.MotionBlurQuality 0;r.AmbientOcclusionLevels 0;r.BloomQuality 0;r.DepthOfFieldQuality 0;r.LightShaftQuality 0;r.LensFlareQuality 0;r.SceneColorFringeQuality 0;r.EyeAdaptationQuality 0;r.SkyAtmosphere 0;r.VolumetricFog 0;r.Tonemapper.Quality 0"
run_one "ALL lightweight bundle + 1FTL_OFF"      "r.OneFrameThreadLag 0;r.Lumen.DiffuseIndirect.Allow 0;r.Lumen.Reflections.Allow 0;r.AntiAliasingMethod 0;r.MotionBlurQuality 0;r.AmbientOcclusionLevels 0;r.BloomQuality 0;r.DepthOfFieldQuality 0;r.LightShaftQuality 0;r.LensFlareQuality 0;r.SceneColorFringeQuality 0;r.EyeAdaptationQuality 0;r.SkyAtmosphere 0;r.VolumetricFog 0;r.Tonemapper.Quality 0"
echo
echo "Higher = bigger improvement vs baseline."
