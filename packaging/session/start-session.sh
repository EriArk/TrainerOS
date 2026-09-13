#!/bin/bash
# SDDM owns this wrapper; systemd owns the compositor and graphical target.
set -u
systemctl --user daemon-reload
systemctl --user stop plasma-workspace-wayland.target plasma-workspace.target
systemctl --user stop graphical-session.target
systemctl --user unset-environment DISPLAY WAYLAND_DISPLAY GAMESCOPE_WAYLAND_DISPLAY XAUTHORITY
dbus-update-activation-environment --systemd WAYLAND_DISPLAY= DISPLAY=
export XDG_SESSION_TYPE=wayland
export XDG_CURRENT_DESKTOP=TrainerOS
export XDG_SESSION_DESKTOP=traineros
systemctl --user import-environment XDG_SESSION_TYPE XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP
dbus-update-activation-environment --systemd XDG_SESSION_TYPE XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP
trap 'systemctl --user stop traineros-session.service' EXIT
trap 'exit 0' TERM INT HUP
systemctl --user --wait start traineros-session.service &
wait $!
result=$?
logger -t traineros-session "Graphical session ended ($result); returning to Armada desktop"
# Intentional UI transitions already requested their destination. A compositor
# failure or an ordinary client exit gets a functioning, persistent fallback.
sudo -n /var/opt/traineros/session/control.py recover "$$"
