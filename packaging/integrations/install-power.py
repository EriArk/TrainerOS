#!/usr/bin/python3 -I
"""Install the bounded power helper; preserve configuration for rollback."""
import argparse
import os
from pathlib import Path
import pwd
import re
import shutil
import subprocess
import tempfile


def install(user):
    if os.geteuid() != 0 or not re.fullmatch(r"[a-z_][a-z0-9_-]*", user) or pwd.getpwnam(user).pw_uid < 1000:
        raise RuntimeError("Root and a regular account are required")
    backup=Path("/var/lib/traineros/power-backup")
    backup.mkdir(parents=True,exist_ok=True,mode=0o700)
    def write(path,content,mode):
        path=Path(path)
        path.parent.mkdir(parents=True,exist_ok=True)
        if path.parent.stat().st_uid!=0 or path.parent.stat().st_mode&0o022:
            raise RuntimeError("Unsafe installation directory")
        old=backup/str(path).lstrip("/").replace("/","__")
        if not old.exists() and not old.with_suffix(old.suffix+".absent").exists():
            if path.exists():shutil.copy2(path,old)
            else:old.with_suffix(old.suffix+".absent").touch()
        fd,tmp=tempfile.mkstemp(dir=path.parent,prefix=".traineros-power-")
        try:
            with os.fdopen(fd,"wb") as f:
                f.write(content);f.flush();os.fchmod(f.fileno(),mode);os.fsync(f.fileno())
            os.replace(tmp,path)
            directory=os.open(path.parent,os.O_DIRECTORY)
            try:os.fsync(directory)
            finally:os.close(directory)
        finally:
            if os.path.exists(tmp):os.unlink(tmp)
    helper="/var/opt/traineros/integrations/power-service.py"
    policy=(user+" ALL=(root) NOPASSWD: "+helper+' ""\n').encode()
    with tempfile.NamedTemporaryFile() as trial:
        trial.write(policy);trial.flush()
        subprocess.run(["/usr/sbin/visudo","-cf",trial.name],check=True)
    write(helper,Path(__file__).with_name("power-service.py").read_bytes().replace(b"\r\n",b"\n"),0o755)
    write("/etc/sudoers.d/traineros-power",policy,0o440)
    # Replace the historical all-sleep-disabled setting, retaining the original.
    write("/etc/systemd/logind.conf.d/zz-traineros-no-sleep.conf",
          b"[Login]\nHandlePowerKey=suspend\nHandlePowerKeyLongPress=ignore\nHandleSuspendKey=suspend\nHandleLidSwitch=suspend\nHandleLidSwitchExternalPower=suspend\nHandleLidSwitchDocked=suspend\nIdleAction=ignore\n",0o644)
    write("/etc/systemd/sleep.conf.d/zz-traineros-no-sleep.conf",
          b"[Sleep]\nAllowSuspend=yes\nAllowHibernation=no\nAllowSuspendThenHibernate=no\nAllowHybridSleep=no\n",0o644)
    for name in ("sleep.target","suspend.target"):
        target=Path("/etc/systemd/system")/name
        record=backup/(name+".state")
        if not record.exists():
            record.write_text(os.readlink(target) if target.is_symlink() else "unmasked")
    subprocess.run(["systemctl","unmask","sleep.target","suspend.target"],check=True)
    subprocess.run(["systemctl","daemon-reload"],check=True)
    # logind supports reload; do not restart it and disrupt the graphical session.
    subprocess.run(["systemctl","kill","--kill-whom=main","--signal=HUP","systemd-logind"],check=True)


if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--user",required=True)
    install(parser.parse_args().user)
