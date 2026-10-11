#!/bin/sh
# Install an isolated versioned helper; never alter the GB/GBA preparation bundle.
set -eu
recipe=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:?Usage: install-transport.sh NEW_OUTPUT_DIRECTORY}
test ! -e "$out"
umask 077
mkdir -p "$out"
python3 -m venv "$out/venv"
"$out/venv/bin/python" -m pip install -r "$recipe/../handheld/linked-save-requirements.txt" noiseprotocol==0.3.1
cp "$recipe/secure-channel.py" "$out/secure-channel.py"
"$out/venv/bin/python" -m pip freeze > "$out/requirements.lock"
"$out/venv/bin/python" -m pip inspect > "$out/dependencies.json"
"$out/venv/bin/python" -c 'import noise.connection; from wormhole import create; import twisted'
sha256sum "$out/secure-channel.py" "$out/requirements.lock"
