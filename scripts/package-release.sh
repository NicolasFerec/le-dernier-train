#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(sed -n 's/^config\/version="\([^"]*\)"/\1/p' project.godot)
if [[ ${1:?Indiquer le tag de la release} != "v$version" ]]; then
    echo "Le tag doit correspondre à la version du projet ($version)." >&2
    exit 1
fi

channel=${3:-latest}
if [[ $channel != latest && $channel != dev ]]; then
    echo "Canal inconnu ($channel)." >&2
    exit 1
fi
suffix=""
if [[ $channel == dev ]]; then
    suffix="-$version"
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
    godot --headless --path . --export-release "$preset" "$directory/Le-Dernier-Train.$extension"
    for library in "${libraries[@]}"; do
        test -s "$directory/$library"
    done
    cp addons/godotsteam/license.md "$directory/GODOTSTEAM-LICENSE.md"
    curl -fsSL --retry 3 https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/LICENSE.txt -o "$directory/GODOT-LICENSE.txt"
    curl -fsSL --retry 3 https://raw.githubusercontent.com/godotengine/godot/4.7.2-stable/COPYRIGHT.txt -o "$directory/GODOT-COPYRIGHT.txt"
    # Published launchers require the original archive filenames.
    cp assets/tiny_dungeon/LICENSE.txt "$directory/KENNEY-LICENSE.txt"
    {
        printf '\n\nThird-party license: netfox and netfox.internals\n\n'
        cat addons/netfox/LICENSE
    } >> "$directory/GODOT-COPYRIGHT.txt"
    (cd "$directory"; zip -q "$output/Le-Dernier-Train-$platform-x86_64$suffix.zip" ./*)
    if [[ $channel == latest ]]; then
        # Existing launchers hardcode both the archive name and the executable inside it.
        mv "$directory/Le-Dernier-Train.$extension" "$directory/Arene-coop.$extension"
        (cd "$directory"; zip -q "$output/Arene-coop-$platform-x86_64.zip" ./*)
    fi
    godot --headless --path "$stage/launcher" --export-release "$preset" "$output/Le-Dernier-Train-Lanceur-$platform-x86_64.$extension"
done

python3 - "$version" "$output" "$channel" <<'PY'
import hashlib
import json
import pathlib
import sys

version, directory, channel = sys.argv[1:]
output = pathlib.Path(directory)
manifest = {"version": version}
suffix = f"-{version}" if channel == "dev" else ""
packages = [("packages_sha256", "Le-Dernier-Train")]
if channel == "latest":
    packages.append(("sha256", "Arene-coop"))
for key, prefix in packages:
    checksums = {}
    for platform in ("windows-x86_64", "linux-x86_64"):
        with (output / f"{prefix}-{platform}{suffix}.zip").open("rb") as archive:
            checksums[platform] = hashlib.file_digest(archive, "sha256").hexdigest()
    manifest[key] = checksums
(output / "update.json").write_text(json.dumps(manifest) + "\n")
PY
