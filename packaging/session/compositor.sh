#!/bin/bash
set -eu
export SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS=0
export QT_QPA_PLATFORM=xcb
export ENABLE_GAMESCOPE_WSI=1
unset DISPLAY WAYLAND_DISPLAY SESSION_MANAGER
eval "$(/usr/libexec/armada/device-env)"
args=(--backend drm --expose-wayland --force-windows-fullscreen)
if [ -n "${ARMADA_PRIMARY_CONNECTOR:-}" ]; then
    args+=(--prefer-output "$ARMADA_PRIMARY_CONNECTOR")
fi
if [ -n "${ARMADA_PANEL_ORIENTATION:-}" ]; then
    args+=(--force-orientation "$ARMADA_PANEL_ORIENTATION")
fi
if [ -n "${ARMADA_PANEL_TYPE:-}" ]; then
    args+=(--force-panel-type "$ARMADA_PANEL_TYPE")
fi
gamescope=/usr/bin/gamescope
# A root-installed capture fix is valid only for the reviewed system baseline.
# An OS update or changed artifact returns to the system compositor automatically.
capture_fix=/var/opt/traineros/system/gamescope
if [ -x "$capture_fix/gamescope" ] && [ -r "$capture_fix/SHA256SUMS" ] &&
   sha256sum --status -c "$capture_fix/SHA256SUMS"; then
    gamescope="$capture_fix/gamescope"
fi
exec "$gamescope" "${args[@]}" -- /usr/bin/python3 /var/opt/traineros/session/client.py
