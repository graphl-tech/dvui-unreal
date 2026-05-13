#!/usr/bin/env bash
# Build the graphl-flavored UE plugin and rsync the generated tree into a
# target UE project's Plugins/ directory.
#
# After running this, regenerate project files in the UE project (right-
# click .uproject -> 'Generate Project Files') and rebuild the editor —
# UE picks up the new GraphlIde plugin and UGraphlIdeWidget appears in
# the UMG palette under "DVUI".
#
# Usage:
#   ./install-graphl-flavor.sh /path/to/YourProject
#
# Or set TARGET_PROJECT_DIR in the env. With neither set, the script just
# builds and prints where to find the generated tree.

set -euo pipefail

SCRIPT_DIR="$(realpath "$(dirname "$0")")"
PLUGIN_ROOT="$(realpath "$SCRIPT_DIR/..")"
GRAPHL_IDE="$(realpath "$PLUGIN_ROOT/../../ide")"

TARGET="${1:-${TARGET_PROJECT_DIR:-}}"

echo "[install-graphl-flavor] building graphl unreal-plugin (ReleaseFast)..."
(cd "$GRAPHL_IDE" && zig build unreal-plugin -Doptimize=ReleaseFast)

GENERATED="$GRAPHL_IDE/zig-out/unreal_plugin/GraphlIde"
if [[ ! -d "$GENERATED" ]]; then
	echo "ERROR: generated plugin tree not found at $GENERATED" >&2
	exit 1
fi

echo "[install-graphl-flavor] generated plugin tree:"
echo "  $GENERATED"

if [[ -z "$TARGET" ]]; then
	cat <<-EOF

	No target UE project specified. Pass the project root as \$1, e.g.:
	  ./install-graphl-flavor.sh /path/to/YourProject

	Or rsync the tree manually:
	  rsync -a --delete '$GENERATED/' '/path/to/YourProject/Plugins/GraphlIde/'

	Then in your UE project:
	  1. Right-click the .uproject -> 'Generate Project Files'.
	  2. Rebuild the editor target.
	  3. In UMG, find 'UGraphlIdeWidget' in the palette under 'DVUI'.
	EOF
	exit 0
fi

if [[ ! -d "$TARGET" ]]; then
	echo "ERROR: target UE project dir not found: $TARGET" >&2
	exit 1
fi

DEST_PLUGIN="$TARGET/Plugins/GraphlIde"
mkdir -p "$(dirname "$DEST_PLUGIN")"

echo "[install-graphl-flavor] syncing -> $DEST_PLUGIN"
rsync -a --delete "$GENERATED/" "$DEST_PLUGIN/"

echo
echo "Done. Next steps in $TARGET:"
echo "  1. Right-click <YourProject>.uproject -> 'Generate Project Files'."
echo "  2. Rebuild the editor target."
echo "  3. In UMG, find 'UGraphlIdeWidget' in the palette under 'DVUI'."
