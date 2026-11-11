#!/usr/bin/env bash
set -euo pipefail
# Local development only: Epic's IP net driver, one listen server and 1–3 guests.
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

players="${1:-2}"
case "$players" in 2|3|4) ;; *) echo 'Usage: local-coop.sh [2|3|4]' >&2; exit 2;; esac
version_line="$(rg --no-filename '^ProjectVersion=' "$project_dir/Config/DefaultGame.ini")"
game_version="${version_line#ProjectVersion=}"
pids=()
smoke=()
if [[ ${TRAIN_COOP_SMOKE:-0} == 1 && $players == 2 ]]; then
    smoke=(-ExecCmds=Train.LocalCoopSmoke -TrainCoopSmokeExit)
fi
cleanup() { for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done; }
trap cleanup EXIT INT TERM
mkdir -p "$project_dir/Saved/LocalCoop"
"$project_dir/Scripts/play.sh" '/Game/Maps/Wagon?listen' -nosteam -TrainLocal -TrainProfile=local-host -MULTIHOME=127.0.0.1 -windowed -ResX=960 -ResY=640 -WinX=0 -WinY=0 -NoSplash -ModelContextProtocolPort=8011 "${smoke[@]}" -AbsLog="$project_dir/Saved/LocalCoop/host.log" >"$project_dir/Saved/LocalCoop/host.stdout" 2>&1 &
pids+=("$!")
# Wait for the native listen socket, rather than racing startup on slower machines.
ready=false
for ((attempt=0;attempt<120;attempt++)); do
    if rg -q 'GameNetDriver.*listening|listening on port' "$project_dir/Saved/LocalCoop/host.log" 2>/dev/null; then ready=true; break; fi
    kill -0 "${pids[0]}" 2>/dev/null || { echo 'Host failed; see Saved/LocalCoop/host.log' >&2; exit 1; }
    sleep .5
done
$ready || { echo 'Host startup timed out' >&2; exit 1; }
for ((guest=1;guest<players;guest++)); do
    "$project_dir/Scripts/play.sh" "127.0.0.1:7777?TrainVersion=$game_version" -nosteam -TrainLocal -TrainProfile="local-guest-$guest" -windowed -ResX=960 -ResY=640 -WinX=960 -WinY=0 -NoSplash -ModelContextProtocolPort="$((8011+guest))" "${smoke[@]}" -AbsLog="$project_dir/Saved/LocalCoop/guest-$guest.log" >"$project_dir/Saved/LocalCoop/guest-$guest.stdout" 2>&1 &
    pids+=("$!")
done
wait
