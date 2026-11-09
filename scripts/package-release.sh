#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(sed -n 's/^config\/version="\([^"]*\)"/\1/p' project.godot)
if [[ ${1:?Indiquer le tag de la release} != "v$version" ]]; then
    echo "Le tag doit correspondre à la version du projet ($version)." >&2
    exit 1
fi

output=$(realpath -m "${2:?Indiquer le dossier de sortie}")
mkdir -p "$output"
stage=$(mktemp -d)
trap 'find "$stage" -depth -delete' EXIT
mkdir -p "$stage/launcher/locale"
cp launcher/project.godot launcher/splash.tscn launcher/splash.gd "$stage/launcher/"
cp export_presets.cfg "$stage/launcher/"
cp locale/game.csv "$stage/launcher/locale/"

# Load GodotSteam at editor startup instead of hot-loading it on the first scan.
mkdir -p .godot
printf "res://addons/godotsteam/godotsteam.gdextension\n" > .godot/extension_list.cfg
godot --headless --editor --path . --import
cp locale/game.fr.translation locale/game.en.translation "$stage/launcher/locale/"
godot --headless --path "$stage/launcher" --script res://splash.gd --check-only
godot --headless --editor --path "$stage/launcher" --import
for platform in windows linux; do
    directory="$stage/$platform"
    mkdir "$directory"
    if [[ $platform == windows ]]; then
        preset="Windows x86_64"
        extension=exe
        libraries=(steam_api64.dll libgodotsteam.windows.template_release.x86_64.dll)
    else
        preset="Linux x86_64"
        extension=x86_64
        libraries=(libsteam_api.so libgodotsteam.linux.template_release.x86_64.so)
    fi
    godot --headless --path . --export-release "$preset" "$directory/Arene-coop.$extension"
    for library in "${libraries[@]}"; do
        test -s "$directory/$library"
    done
    cp assets/tiny_dungeon/LICENSE.txt "$directory/KENNEY-LICENSE.txt"
    cp addons/godotsteam/license.md "$directory/GODOTSTEAM-LICENSE.md"
    curl -fsSL --retry 3 https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/LICENSE.txt -o "$directory/GODOT-LICENSE.txt"
    curl -fsSL --retry 3 https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/COPYRIGHT.txt -o "$directory/GODOT-COPYRIGHT.txt"
    (cd "$directory"; zip -q "$output/Arene-coop-$platform-x86_64.zip" ./*)
    godot --headless --path "$stage/launcher" --export-release "$preset" "$output/Arene-coop-Lanceur-$platform-x86_64.$extension"
done

python3 - "$version" "$output" <<'PY'
import hashlib
import json
import pathlib
import sys

version, directory = sys.argv[1:]
output = pathlib.Path(directory)
checksums = {}
for platform in ("windows-x86_64", "linux-x86_64"):
    with (output / f"Arene-coop-{platform}.zip").open("rb") as archive:
        checksums[platform] = hashlib.file_digest(archive, "sha256").hexdigest()
(output / "update.json").write_text(json.dumps({"version": version, "sha256": checksums}) + "\n")
PY
