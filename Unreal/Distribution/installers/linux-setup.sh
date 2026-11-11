#!/bin/sh
set -eu

case ${1:-} in
    '') launch=1 ;;
    --no-launch) launch=0 ;;
    *) printf '%s\n' 'Usage: setup.run [--no-launch]' >&2; exit 2 ;;
esac
if [ "$(uname -m)" != x86_64 ]; then
    printf '%s\n' 'Cet installateur nécessite Linux x86-64. / This installer requires Linux x86-64.' >&2
    exit 1
fi
case ${XDG_DATA_HOME:-} in
    /*) data=$XDG_DATA_HOME ;;
    *) data=$HOME/.local/share ;;
esac
app=$data/le-dernier-train-unreal
applications=$data/applications
entry=$applications/le-dernier-train-unreal.desktop
mkdir -p "$app" "$applications"
exec 9>"$app/.lock"
if ! flock -n 9; then
    printf '%s\n' 'Ferme le jeu et le lanceur. / Close the game and its launcher.' >&2
    exit 1
fi
scratch=$(mktemp -d "$app/.setup.XXXXXXXX")
desktop_dir=$(mktemp -d "$applications/.le-dernier-train.XXXXXXXX")
desktop_tmp=$desktop_dir/le-dernier-train-unreal.desktop
trap 'rm -r -- "$scratch" "$desktop_dir"' EXIT
trap 'exit 1' HUP INT TERM
line=$(awk '/^__TRAIN_PAYLOAD__$/ {print NR+1; exit}' "$0")
tail -n "+$line" "$0" | tar -xf - --no-same-owner -C "$scratch"

# Desktop Entry escaping is applied before Exec quoting, including literal percent signs.
exec_path=$(printf '%s' "$app/LeDernierTrainLauncher" | sed 's/[\\"`$]/\\&/g; s/\\/\\\\/g; s/%/%%/g')
icon_path=$(printf '%s' "$app/icon.png" | sed 's/\\/\\\\/g')
cat > "$desktop_tmp" <<EOF
[Desktop Entry]
Type=Application
Name=Le Dernier Train
Comment=Roguelite d’action en solo ou en coop
Comment[en]=Solo or co-op action roguelite
Exec="$exec_path"
Icon=$icon_path
Terminal=false
Categories=Game;
EOF
if command -v desktop-file-validate >/dev/null 2>&1; then desktop-file-validate "$desktop_tmp"; fi
chmod 755 "$scratch/LeDernierTrainLauncher" "$scratch/uninstall.sh"
chmod 644 "$scratch/icon.png" "$desktop_tmp"
mv -f -- "$scratch/LeDernierTrainLauncher" "$scratch/uninstall.sh" "$scratch/icon.png" "$app/"
mv -f -- "$scratch/bootstrap.zip" "$scratch/bootstrap.json" "$app/"
mv -f -- "$desktop_tmp" "$entry"
if command -v update-desktop-database >/dev/null 2>&1; then update-desktop-database "$applications" || true; fi
printf '%s\n' 'Le Dernier Train apparaît dans Jeux. / Le Dernier Train is available in Games.'
flock -u 9
exec 9>&-
if [ "$launch" -eq 1 ]; then
    nohup "$app/LeDernierTrainLauncher" >/dev/null 2>&1 &
fi
exit 0
__TRAIN_PAYLOAD__
