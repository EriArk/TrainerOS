#!/bin/sh
# Read-only report for the first ArmadaOS visit. Run from Desktop / Maintenance Mode.
# No sudo, installs, configuration edits, session changes or game/save file scans.
set -u
if [ "$(uname -s)" != "Linux" ]; then
    printf '%s\n' 'This report is for the Linux handheld. Run it on ArmadaOS.' >&2
    exit 2
fi
section() { printf '\n%s\n' "$1"; }
section 'TrainerOS device baseline (read-only)'
date -u '+UTC: %Y-%m-%dT%H:%M:%SZ'
uname -srm
section 'System base'
if [ -r /etc/os-release ]; then cat /etc/os-release; else printf 'os-release unavailable\n'; fi
section 'Current graphical session (reported environment; verify on device)'
printf 'Type: %s\nDesktop: %s\n' "${XDG_SESSION_TYPE:-unknown}" "${XDG_CURRENT_DESKTOP:-unknown}"
if command -v loginctl >/dev/null 2>&1 && [ -n "${XDG_SESSION_ID:-}" ]; then
    loginctl show-session "$XDG_SESSION_ID" -p Type -p Desktop -p Service -p State 2>&1
fi
section 'Installed session entries (names only; none are selected or changed)'
for entry in /usr/share/wayland-sessions/*.desktop /usr/share/xsessions/*.desktop; do
    [ -f "$entry" ] && basename "$entry"
done
section 'Known compositor / display-manager processes'
ps -eo comm= | sort -u | awk '/^(kwin_wayland|kwin_x11|gamescope|gamescope-wl|gamescope-sessi|sddm|gdm|lightdm|greetd|weston|Xorg|Xwayland)$/ { print }'
section 'Display connectors and advertised modes (not measured active geometry)'
for status_file in /sys/class/drm/*/status; do
    [ -r "$status_file" ] || continue
    connector=${status_file%/status}
    printf '%s: ' "${connector##*/}"
    cat "$status_file"
    if [ "$(cat "$status_file")" = 'connected' ] && [ -r "$connector/modes" ]; then cat "$connector/modes"; fi
done
if command -v kscreen-doctor >/dev/null 2>&1 && { [ -n "${WAYLAND_DISPLAY:-}" ] || [ -n "${DISPLAY:-}" ]; }; then
    section 'KScreen output report (may be unavailable outside Plasma)'
    kscreen-doctor -o 2>&1
else
    section 'KScreen output skipped: no graphical connection in this shell or tool unavailable'
fi
section 'Input device names (no unique device identifiers)'
for input_name in /sys/class/input/event*/device/name; do
    [ -r "$input_name" ] && cat "$input_name"
done
section 'Available build / runtime commands'
for program in cmake ninja c++ pkg-config qtpaths6 qtpaths qmake6 qmake; do
    if command -v "$program" >/dev/null 2>&1; then printf '%s: available\n' "$program"; else printf '%s: unavailable\n' "$program"; fi
done
if command -v pkg-config >/dev/null 2>&1; then
    for package in Qt6Core Qt6Gui Qt6Quick Qt6Sql sdl2; do
        printf '%s: ' "$package"
        pkg-config --modversion "$package" 2>/dev/null || printf 'development metadata unavailable\n'
    done
fi
section 'Known emulator commands (availability only; nothing is launched)'
for program in retroarch melonDS azahar dolphin-emu; do
    if command -v "$program" >/dev/null 2>&1; then printf '%s: available\n' "$program"; else printf '%s: not in PATH\n' "$program"; fi
done
printf '\n%s\n' 'End of report. This does not validate controller mapping, active screen size, suspend/wake or launch/return behavior.'
