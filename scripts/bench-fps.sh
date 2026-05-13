#!/usr/bin/env bash
# Run the test project headlessly for N seconds (default 10), then dump
# FPS / timing log lines for analysis.

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
TEST_PROJECT="$PLUGIN_ROOT/ThirdParty/TestProject/DVUITest.uproject"
UNREAL_ENGINE="${UNREAL_ENGINE:-$HOME/.local/share/unreal/Engine}"
EDITOR_CMD="$UNREAL_ENGINE/Binaries/Linux/UnrealEditor-Cmd"
SECONDS_TO_RUN="${1:-10}"

LOG_FILE="$PLUGIN_ROOT/ThirdParty/TestProject/Saved/Logs/bench-fps.log"
mkdir -p "$(dirname "$LOG_FILE")"

echo "[bench-fps] running for $SECONDS_TO_RUN seconds..."
EXTRA_FLAGS="${EXTRA_FLAGS:-}"
"$EDITOR_CMD" \
	"$TEST_PROJECT" \
	-game \
	-RenderOffScreen \
	-ResX=1280 \
	-ResY=720 \
	-windowed \
	-unattended \
	-NoSound \
	-stdout \
	-FullStdOutLogOutput \
	-AllowStdOutLogVerbosity \
	-LogCmds="LogTemp Verbose" \
	-Log="$(basename "$LOG_FILE")" \
	-DvuiRunSeconds="$SECONDS_TO_RUN" \
	-ExecCmds="t.MaxFPS 1000;r.VSync 0;t.IdleWhenNotForeground 0;Slate.AllowSlateToSleep 0;Slate.ThrottleWhenMouseIsMoving 0" \
	-preferNvidia \
	$EXTRA_FLAGS \
	>/dev/null 2>&1 || true

echo
echo "=== FPS samples ==="
grep -E "FPS=" "$LOG_FILE" | head -20
echo
echo "=== OnPaint timing ==="
grep -E "OnPaint timing" "$LOG_FILE" | head -10
echo
echo "=== RT upload timing ==="
grep -E "RT upload" "$LOG_FILE" | head -10
echo
echo "=== Texture creates ==="
grep -E "textureCreate rate|texture_create id" "$LOG_FILE" | head -20
echo
echo "=== Bench done ==="
grep -E "bench done" "$LOG_FILE"
