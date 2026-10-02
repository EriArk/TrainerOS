#!/bin/sh
# Run as the handheld user. Install only TrainerOS-owned files.
set -eu
test "$(id -u)" -ne 0 || { echo 'Run this installer as the handheld user.' >&2; exit 1; }
command -v python3 >/dev/null
command -v pactl >/dev/null
command -v parec >/dev/null
command -v pacat >/dev/null
command -v ffmpeg >/dev/null
voice_dir="${XDG_DATA_HOME:-$HOME/.local/share}/traineros-voice"
test ! -L "$voice_dir" || { echo 'Refusing a symlinked voice environment.' >&2; exit 1; }
python3 -m venv "$voice_dir"
"$voice_dir/bin/python" -m pip install --disable-pip-version-check 'livekit==1.1.19'
"$voice_dir/bin/python" -c 'from livekit import rtc; assert rtc.__version__ == "1.1.19"'
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sudo install -d -m 0755 /var/opt/traineros/integrations
sudo install -m 0755 "$script_dir/fluxer-voice.py" /var/opt/traineros/integrations/fluxer-voice.py
