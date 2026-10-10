#!/bin/sh
# Create a versioned user-owned helper bundle. Never overwrite an active bundle.
# Record linkedSavePython=OUTPUT/venv/bin/python and linkedSaveHelper=OUTPUT/linked-save.py
# in the host's existing retroarch.json only after verification, then restart shell.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${1:?Usage: install-linked-save.sh NEW_OUTPUT_DIRECTORY}
test ! -e "$out"
umask 077
mkdir -p "$out"
python3 -m venv "$out/venv"
"$out/venv/bin/python" -m pip install -r "$source_dir/linked-save-requirements.txt"
cp "$source_dir/linked-save.py" "$out/linked-save.py"
"$out/venv/bin/python" -m pip freeze > "$out/requirements.lock"
"$out/venv/bin/python" -m pip inspect > "$out/dependencies.json"
"$out/venv/bin/python" -c 'from wormhole import create; from cryptography import x509; import twisted'
sha256sum "$out/linked-save.py" "$out/requirements.lock"
