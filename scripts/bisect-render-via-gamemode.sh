#!/usr/bin/env bash
# Bisect the lightweight CVar bundle by KEEPING ONE feature at a time.
# Lower FPS == that feature is the expensive one we're killing.

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
TEST_PROJECT="$PLUGIN_ROOT/ThirdParty/TestProject/DVUITest.uproject"
UNREAL_ENGINE="${UNREAL_ENGINE:-$HOME/.local/share/unreal/Engine}"
EDITOR_CMD="$UNREAL_ENGINE/Binaries/Linux/UnrealEditor-Cmd"
RUN_SECONDS="${RUN_SECONDS:-4}"

LOG_DIR="$PLUGIN_ROOT/ThirdParty/TestProject/Saved/Logs"
mkdir -p "$LOG_DIR"

run_one() {
	local label="$1"
	local keep_flag="$2"
	local log_file="$LOG_DIR/bisect2-$label.log"
	"$EDITOR_CMD" \
		"$TEST_PROJECT" \
		-game -RenderOffScreen -ResX=1280 -ResY=720 -windowed -unattended -NoSound \
		-stdout -FullStdOutLogOutput -AllowStdOutLogVerbosity \
		-Log="$(basename "$log_file")" \
		-DvuiRunSeconds="$RUN_SECONDS" \
		-DvuiEnableWorldRendering \
		$keep_flag \
		>/dev/null 2>&1 || true
	local fps_avg
	fps_avg=$(grep -oE "FPS=[0-9.]+" "$log_file" | tail -n +2 | sed 's/FPS=//' \
		| awk '{ sum += $1; n++ } END { if (n > 0) printf "%.1f", sum / n; else print "?" }')
	printf "  %-45s %s\n" "$label" "$fps_avg"
}

echo "=== keep-one bisect (world ON, lightweight bundle ON, ${RUN_SECONDS}s each) ==="
echo "=== LOWER FPS = that single feature is the cost driver ==="
echo "  KEEP                                        FPS"
echo "  -------------------------------------------- ----"
run_one "(nothing kept = full lightweight bundle)" ""
run_one "Lumen Diffuse"          "-DvuiKeepLumenDiffuse"
run_one "Lumen Reflections"      "-DvuiKeepLumenReflections"
run_one "Both Lumen"             "-DvuiKeepLumenDiffuse -DvuiKeepLumenReflections"
run_one "AntiAliasing (TAA)"     "-DvuiKeepAA"
run_one "MotionBlur"             "-DvuiKeepMotionBlur"
run_one "SSAO"                   "-DvuiKeepSSAO"
run_one "Bloom"                  "-DvuiKeepBloom"
run_one "DepthOfField"           "-DvuiKeepDOF"
run_one "LightShafts"            "-DvuiKeepLightShafts"
run_one "LensFlares"             "-DvuiKeepLensFlares"
run_one "SceneColorFringe"       "-DvuiKeepSceneFringe"
run_one "EyeAdaptation"          "-DvuiKeepEyeAdapt"
run_one "SkyAtmosphere"          "-DvuiKeepSkyAtmos"
run_one "VolumetricFog"          "-DvuiKeepVolFog"
run_one "Tonemapper"             "-DvuiKeepTonemapper"
run_one "ALL kept (= heavy mode equivalent)" "-DvuiKeepLumenDiffuse -DvuiKeepLumenReflections -DvuiKeepAA -DvuiKeepMotionBlur -DvuiKeepSSAO -DvuiKeepBloom -DvuiKeepDOF -DvuiKeepLightShafts -DvuiKeepLensFlares -DvuiKeepSceneFringe -DvuiKeepEyeAdapt -DvuiKeepSkyAtmos -DvuiKeepVolFog -DvuiKeepTonemapper"
