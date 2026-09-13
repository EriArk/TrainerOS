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
exec /usr/bin/gamescope "${args[@]}" -- /usr/bin/python3 /var/opt/traineros/session/client.py
