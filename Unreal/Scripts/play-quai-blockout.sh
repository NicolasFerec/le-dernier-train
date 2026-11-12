#!/usr/bin/env bash
set -euo pipefail
here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec "$here/play.sh" /Game/Maps/Blockout/QuaiBlockout -TrainLocal -nosteam -TrainProfile=quai-blockout "$@"
