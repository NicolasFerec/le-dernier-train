#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
editor="${UNREAL_EDITOR:-$HOME/.local/opt/unreal-engine-5.8.4/Engine/Binaries/Linux/UnrealEditor}"
# QA resolutions and key bindings must never overwrite the player's settings.
settings_args=()
for argument in "$@"; do
    case "$argument" in
        -TrainProfile=*)
            profile="${argument#-TrainProfile=}"
            profile="${profile//[^a-zA-Z0-9_-]/_}"
            settings_dir="$project_dir/Saved/TestSettings/$profile"
            mkdir -p "$settings_dir"
            if [[ ! -f "$settings_dir/GameUserSettings.ini" && -f "$project_dir/Saved/Config/LinuxEditor/GameUserSettings.ini" ]]; then
                cp "$project_dir/Saved/Config/LinuxEditor/GameUserSettings.ini" "$settings_dir/GameUserSettings.ini"
            fi
            settings_args=("-GameUserSettingsINI=$settings_dir/GameUserSettings.ini")
            ;;
    esac
done
# Editor-only MCP/Python toolsets must not run their startup scripts in -game mode.
exec env -u DRI_PRIME "$editor" "$project_dir/LeDernierTrain.uproject" -game -NoSplash -DisablePlugins=ModelContextProtocol,EditorToolset,UMGToolSet,SlateInspectorToolset,AutomationTestToolset,ToolsetRegistry "${settings_args[@]}" "$@"
