#!/usr/bin/env bash
# Launch the DVUITest project headlessly. The DVUITest GameMode adds the
# DVUIUnreal widget to the viewport on BeginPlay, waits N rendered frames,
# triggers HighResShot, then quits.
#
# Output: ThirdParty/TestProject/Saved/Screenshots/Linux*/smoke*.png

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
TEST_PROJECT="$PLUGIN_ROOT/ThirdParty/TestProject/DVUITest.uproject"
UNREAL_ENGINE="${UNREAL_ENGINE:-$HOME/.local/share/unreal/Engine}"
EDITOR_CMD="$UNREAL_ENGINE/Binaries/Linux/UnrealEditor-Cmd"

if [[ ! -x "$EDITOR_CMD" ]]; then
	echo "ERROR: UnrealEditor-Cmd not found at $EDITOR_CMD" >&2
	exit 1
fi

# Clean previous screenshots so we know if a new one was produced.
SCREENSHOT_DIR="$PLUGIN_ROOT/ThirdParty/TestProject/Saved/Screenshots"
if [[ -d "$SCREENSHOT_DIR" ]]; then
	find "$SCREENSHOT_DIR" -name 'smoke*.png' -delete 2>/dev/null || true
fi

LOG_FILE="$PLUGIN_ROOT/ThirdParty/TestProject/Saved/Logs/run-screenshot.log"
mkdir -p "$(dirname "$LOG_FILE")"

# -RenderOffScreen: lets the editor render without a visible window (works
#   with X but does not require a real display).
# -unattended: suppresses interactive dialogs.
# -nullrhi MUST NOT be passed — we need the renderer for the screenshot.
echo "[run-screenshot] launching editor in -game mode..."
set +e
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
	-DvuiAutoScreenshot \
	-DvuiClickTest \
	-preferNvidia \
	${EXTRA_FLAGS:-}
EXIT_CODE=$?
set -e

echo "[run-screenshot] editor exited with code $EXIT_CODE"

# Find produced screenshot(s). Click-test mode produces _before and _after.
SHOTS=$(find "$SCREENSHOT_DIR" -name 'smoke*.png' 2>/dev/null | sort || true)
if [[ -z "$SHOTS" ]]; then
	echo "[run-screenshot] WARNING: no smoke*.png produced under $SCREENSHOT_DIR" >&2
	echo "[run-screenshot] check log: $LOG_FILE" >&2
	exit 1
fi

echo "[run-screenshot] screenshots:"
echo "$SHOTS"

# Surface input-related log lines so we can see whether the click reached dvui.
echo "---"
echo "[run-screenshot] dvui input-related log lines:"
grep -E "DVUITest|dvui-unreal-sample.*click" "$PLUGIN_ROOT/ThirdParty/TestProject/Saved/Logs/run-screenshot.log" 2>/dev/null \
	| grep -E "click|injecting|sample.*click" | head -10 || echo "(none found)"
