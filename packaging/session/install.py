#!/usr/bin/python3 -I
"""Install additive session files on the validated immutable ArmadaOS layout.

Run from a reviewed checkout as root. This installs a choice, never selects it.
An existing file's first version is retained in /var/lib/traineros/session-backup.
"""
import argparse
import configparser
import os
from pathlib import Path
import pwd
import re
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--user", required=True)
    args = parser.parse_args()
    if os.geteuid() != 0 or not re.fullmatch(r"[a-z_][a-z0-9_-]*", args.user):
        parser.error("Use root and an existing local login name")
    account = pwd.getpwnam(args.user)
    if account.pw_uid == 0:
        parser.error("The graphical shell must not run as root")
    for binary in ["/usr/bin/gamescope", "/usr/libexec/armada/device-env",
                   "/usr/libexec/armada/session-control", "/usr/sbin/visudo"]:
        if not os.access(binary, os.X_OK):
            parser.error("Missing platform prerequisite: " + binary)
    if not os.access(Path(account.pw_dir) / ".local/bin/traineros", os.X_OK):
        parser.error("Install and validate the normal TrainerOS application first")
    # Preserve the currently configured search directories, not just our own.
    directories = ["/usr/local/share/wayland-sessions", "/usr/share/wayland-sessions"]
    config = configparser.ConfigParser(interpolation=None, strict=False)
    paths = sorted(Path("/etc/sddm.conf.d").glob("*.conf")) + [Path("/etc/sddm.conf")]
    for path in paths:
        if path.is_file():
            config.read(path)
    if config.has_option("Wayland", "SessionDir"):
        directories = config.get("Wayland", "SessionDir").split(",")
    directories = list(dict.fromkeys(d.strip() for d in directories if d.strip()))
    if any(not d.startswith("/") or "\n" in d for d in directories):
        parser.error("Unexpected SDDM session search path")
    own = "/etc/traineros/wayland-sessions"
    if own not in directories:
        directories.append(own)
    source = Path(__file__).resolve().parent
    backup = Path("/var/lib/traineros/session-backup")
    backup.mkdir(parents=True, exist_ok=True, mode=0o700)

    def install(path, data, mode):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True, mode=0o755)
        if path.parent.stat().st_uid != 0 or path.parent.stat().st_mode & 0o022:
            raise RuntimeError("Install directory must be owned and writable only by root")
        prior = backup / str(path).lstrip("/").replace("/", "__")
        if not prior.exists() and not Path(str(prior) + ".absent").exists():
            if path.exists():
                shutil.copy2(path, prior)
            else:
                Path(str(prior) + ".absent").touch(mode=0o600)
        fd, temporary = tempfile.mkstemp(dir=path.parent, prefix=".traineros-")
        try:
            with os.fdopen(fd, "wb") as stream:
                stream.write(data); stream.flush(); os.fchmod(stream.fileno(), mode); os.fsync(stream.fileno())
            os.replace(temporary, path)
            directory = os.open(path.parent, os.O_DIRECTORY)
            try:
                os.fsync(directory)
            finally:
                os.close(directory)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)

    helper = "/var/opt/traineros/session/control.py"
    verbs = ["traineros", "default-traineros", "desktop", "steam", "recover *", "poweroff", "reboot"]
    policy = (args.user + " ALL=(root) NOPASSWD: " + ", ".join(helper + " " + v for v in verbs) + "\n").encode()
    with tempfile.NamedTemporaryFile(prefix="traineros-sudoers-") as trial:
        trial.write(policy); trial.flush()
        subprocess.run(["/usr/sbin/visudo", "-cf", trial.name], check=True, stdout=subprocess.DEVNULL)
    for name in ["client.py", "start-session.sh", "compositor.sh", "control.py"]:
        install("/var/opt/traineros/session/" + name, (source / name).read_bytes().replace(b"\r\n", b"\n"), 0o755)
    install(own + "/traineros.desktop", (source / "traineros.desktop").read_bytes(), 0o644)
    install("/etc/systemd/user/traineros-session.service", (source / "traineros-session.service").read_bytes(), 0o644)
    install("/etc/sudoers.d/traineros-session", policy, 0o440)
    install("/etc/sddm.conf.d/90-traineros.conf",
            ("[Wayland]\nSessionDir=" + ",".join(directories) + "\n").encode(), 0o644)
    # Keep the original default for an explicit rollback, without modifying it.
    original = backup / "original-autologin.conf"
    override = Path("/etc/sddm.conf.d/zz-steamos-autologin.conf")
    if not original.exists() and override.is_file():
        shutil.copy2(override, original)
    print("TrainerOS session choice installed. Current autologin is unchanged.")


if __name__ == "__main__":
    main()
