#!/usr/bin/env bash
# Launch the test project with a real window (uses your $DISPLAY) so you
# can interact with graphl: click nodes, type into search, drag the canvas,
# watch the cursor change, etc.
#
# Use scripts/run-screenshot.sh for the headless variant (no window, just
# writes a PNG and quits).

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
TEST_PROJECT="$PLUGIN_ROOT/ThirdParty/TestProject/DVUITest.uproject"
UNREAL_ENGINE="${UNREAL_ENGINE:-$HOME/.local/share/unreal/Engine}"
EDITOR_CMD="$UNREAL_ENGINE/Binaries/Linux/UnrealEditor-Cmd"

if [[ -z "${DISPLAY:-}" ]]; then
	echo "WARNING: \$DISPLAY is unset — pass -RenderOffScreen or run via xvfb" >&2
fi

# Always rebuild + relink graphl before launching so the .a doesn't drift
# back to the sample-app version (build-test.sh / bench scripts overwrite
# the same .a path with sample-app content). Set DVUI_SKIP_BUILD=1 to skip.
if [[ "${DVUI_SKIP_BUILD:-0}" != "1" ]]; then
	echo "[run-graphl-windowed] (re)building graphl... set DVUI_SKIP_BUILD=1 to skip"
	"$SCRIPT_DIR/build-graphl.sh" >&2
fi

# Disable the auto-quit / auto-screenshot so you can play with the IDE.
# Stop the loop with Ctrl+C in the terminal.
exec "$EDITOR_CMD" \
	"$TEST_PROJECT" \
	-game \
	-windowed \
	-stdout \
	-FullStdOutLogOutput \
	-preferNvidia \
	"$@"
