#!/usr/bin/env bash
# Dev iteration build for the DVUIUnreal plugin.
#
# Builds the Zig backend static library, then builds the test project's
# editor target via UnrealBuildTool. Much faster than RunUAT BuildPlugin
# (which packages for distribution).

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
TEST_PROJECT="$PLUGIN_ROOT/ThirdParty/TestProject/DVUITest.uproject"
UNREAL_ENGINE="${UNREAL_ENGINE:-$HOME/.local/share/unreal/Engine}"

if [[ ! -d "$UNREAL_ENGINE" ]]; then
	echo "ERROR: UNREAL_ENGINE not found at: $UNREAL_ENGINE" >&2
	echo "Set UNREAL_ENGINE to your Unreal Engine install's Engine directory" >&2
	exit 1
fi

if [[ ! -f "$TEST_PROJECT" ]]; then
	echo "ERROR: test project not found at: $TEST_PROJECT" >&2
	exit 1
fi

echo "[build-test] building Zig backend..."
(cd "$PLUGIN_ROOT/ThirdParty/dvui-unreal-backend" && zig build -Doptimize=ReleaseFast)

if [[ ! -f "$PLUGIN_ROOT/ThirdParty/dvui-unreal-backend/zig-out/lib/libdvui_unreal.a" ]]; then
	echo "ERROR: Zig backend did not produce libdvui_unreal.a" >&2
	exit 1
fi
echo "[build-test] Zig backend OK"

# Determine target. Editor target is what we use for headless dev iteration —
# UnrealEditor-Cmd <project> -game runs the editor binary in game mode using
# uncooked content, no separate game target build needed.
TARGET="${1:-DVUITestEditor}"

echo "[build-test] building $TARGET (Linux Development) for $TEST_PROJECT ..."
"$UNREAL_ENGINE/Build/BatchFiles/Linux/Build.sh" \
	"$TARGET" Linux Development \
	-Project="$TEST_PROJECT" \
	-WaitMutex \
	-FromMsBuild \
	-NoHotReload

echo "[build-test] done"
