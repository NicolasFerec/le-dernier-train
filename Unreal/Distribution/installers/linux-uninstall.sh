#!/bin/sh
set -eu
app=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
if [ "$(basename -- "$app")" != le-dernier-train-unreal ]; then
    printf '%s\n' 'Lance le désinstallateur installé. / Run the installed uninstaller.' >&2
    exit 1
fi
applications=$(dirname -- "$app")/applications
exec 9>"$app/.lock"
if ! flock -n 9; then
    printf '%s\n' 'Ferme le jeu et le lanceur. / Close the game and its launcher.' >&2
    exit 1
fi
for directory in "$app/versions" "$app/incoming"; do
    if [ -d "$directory" ]; then rm -r -- "$directory"; fi
done
for file in "$app/LeDernierTrainLauncher" "$app/icon.png" "$app/current-stable.json" \
    "$app/current-dev.json" "$app/bootstrap.zip" "$app/bootstrap.json" \
    "$applications/le-dernier-train-unreal.desktop" "$app/uninstall.sh" "$app/.lock"; do
    if [ -e "$file" ]; then rm -- "$file"; fi
done
rmdir -- "$app" 2>/dev/null || true
if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$applications" || true; fi
printf '%s\n' 'Jeu désinstallé. Sauvegardes conservées. / Game uninstalled. Saves kept.'
