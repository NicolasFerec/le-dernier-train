#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
editor="${UNREAL_EDITOR:-$HOME/.local/opt/unreal-engine-5.8.4/Engine/Binaries/Linux/UnrealEditor}"
# Editor-only MCP/Python toolsets must not run their startup scripts in -game mode.
exec env -u DRI_PRIME "$editor" "$project_dir/LeDernierTrain.uproject" -game -NoSplash -DisablePlugins=ModelContextProtocol,EditorToolset,UMGToolSet,SlateInspectorToolset,AutomationTestToolset,ToolsetRegistry "$@"
