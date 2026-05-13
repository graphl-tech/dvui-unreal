#!/usr/bin/env bash
# Same as bench-fps.sh but with a real on-screen window (uses $DISPLAY).
# Lets us tell whether the throttle is from headless / no-display mode.

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
TEST_PROJECT="$PLUGIN_ROOT/ThirdParty/TestProject/DVUITest.uproject"
UNREAL_ENGINE="${UNREAL_ENGINE:-$HOME/.local/share/unreal/Engine}"
EDITOR_CMD="$UNREAL_ENGINE/Binaries/Linux/UnrealEditor-Cmd"
SECONDS_TO_RUN="${1:-8}"

LOG_FILE="$PLUGIN_ROOT/ThirdParty/TestProject/Saved/Logs/bench-windowed.log"
mkdir -p "$(dirname "$LOG_FILE")"

echo "[bench-windowed] running for $SECONDS_TO_RUN seconds with on-screen window..."
"$EDITOR_CMD" \
	"$TEST_PROJECT" \
	-game \
	-ResX=1280 \
	-ResY=720 \
	-windowed \
	-NoSound \
	-stdout \
	-FullStdOutLogOutput \
	-AllowStdOutLogVerbosity \
	-LogCmds="LogTemp Verbose" \
	-Log="$(basename "$LOG_FILE")" \
	-DvuiRunSeconds="$SECONDS_TO_RUN" >/dev/null 2>&1 || true

echo
grep -E "FPS=" "$LOG_FILE" | head -10
