#!/usr/bin/env bash
# Build & relink the test project against the graphl-flavored backend .a.
#
# Why this exists separately from build-test.sh: build-test.sh re-runs the
# bundled `zig build` in dvui-unreal-backend, which produces the sample-app
# .a and overwrites whatever was there. This script:
#   1. builds the graphl flavor at ide/zig-out/unreal_plugin/graphl_ide/
#   2. copies it into the UE plugin's expected path
#   3. relinks the UE editor module ONLY (no zig build, no overwrite)

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
GRAPHL_IDE="$(realpath "$PLUGIN_ROOT/../../ide")"
TEST_PROJECT="$PLUGIN_ROOT/ThirdParty/TestProject/DVUITest.uproject"
UNREAL_ENGINE="${UNREAL_ENGINE:-$HOME/.local/share/unreal/Engine}"

DEST="$PLUGIN_ROOT/ThirdParty/dvui-unreal-backend/zig-out/lib/libdvui_unreal.a"

echo "[build-graphl] building graphl unreal-plugin (ReleaseFast)..."
(cd "$GRAPHL_IDE" && zig build unreal-plugin -Doptimize=ReleaseFast)

SRC="$GRAPHL_IDE/zig-out/unreal_plugin/graphl_ide/lib/libgraphl_ide_dvui.a"
if [[ ! -f "$SRC" ]]; then
	echo "ERROR: graphl plugin .a not produced at $SRC" >&2
	exit 1
fi

mkdir -p "$(dirname "$DEST")"
cp "$SRC" "$DEST"
echo "[build-graphl] copied $(du -h "$DEST" | cut -f1) -> $DEST"

echo "[build-graphl] relinking UE plugin..."
"$UNREAL_ENGINE/Build/BatchFiles/Linux/Build.sh" \
	DVUITestEditor Linux Development \
	-Project="$TEST_PROJECT" \
	-WaitMutex \
	-FromMsBuild \
	-NoHotReload

echo "[build-graphl] done"
