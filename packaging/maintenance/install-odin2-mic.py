#!/usr/bin/env python3
"""Install a reviewed Odin 2 audio candidate, preserving Armada's rollback image.

Run as root with BASELINE_DTB CANDIDATE_DTB UCM_FRAGMENT. Reboot separately.
This is a bounded maintenance tool, not automatic support for other handhelds.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zlib


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main(baseline, candidate, fragment):
    if os.geteuid() or Path('/sys/firmware/devicetree/base/model').read_bytes().rstrip(b'\0') != b'AYN Odin 2':
        raise SystemExit('Requires root on the original AYN Odin 2')
    # Caller must leave game/call lifecycle intact; refuse active owners.
    processes=subprocess.check_output(['ps','-eo','comm='],text=True).splitlines()
    if any(p.strip().lower() in {'retroarch','ppsspp','dolphin-emu','cemu','xemu','rpcs3','eden','duckstation','armsx2'} for p in processes):
        raise SystemExit('Close the active emulator first')
    if subprocess.run(['pgrep','-f','[f]luxer-voice.py'],stdout=subprocess.DEVNULL).returncode==0:
        raise SystemExit('End the call first')
    boot=Path('/boot/efi/KERNEL');spare=Path('/boot/efi/KERNEL.BAK')
    stamp=Path('/boot/efi/.armada-bootimg.id')
    if not os.path.ismount('/boot/efi') or not spare.is_file():
        raise SystemExit('Mounted ESP and existing recovery image required')
    if shutil.disk_usage('/boot/efi').free < boot.stat().st_size+4*1024**2:
        raise SystemExit('Insufficient space for atomic boot-image replacement')
    entry=max(Path('/boot/loader/entries').glob('*.conf'),key=lambda p:int(next(l.split()[1] for l in p.read_text().splitlines() if l.startswith('version '))))
    fields=dict(line.split(' ',1) for line in entry.read_text().splitlines() if ' ' in line)
    live=next(a for a in Path('/proc/cmdline').read_text().split() if a.startswith('ostree='))
    if live not in fields['options'].split():
        raise SystemExit('Next boot differs from running deployment; do not mix updates')
    dtb=Path('/boot'+fields['fdtdir'])/'qcom/qcs8550-ayn-odin2.dtb'
    if sha(dtb)!=sha(baseline):
        raise SystemExit('The reviewed DTB baseline changed')
    original=Path('/usr/share/alsa/ucm2/AYN/Odin2/HiFi.conf')
    if '\nSectionDevice."Mic"' in original.read_text():
        raise SystemExit('System already supplies a microphone; re-evaluate the override')
    overlay=Path('/var/opt/traineros/audio/ucm2')
    dropin=Path('/etc/systemd/user/wireplumber.service.d/90-traineros-odin2-mic.conf')
    if overlay.exists() or dropin.exists():
        raise SystemExit('An audio override already exists; inspect before replacing')
    backup=Path('/var/lib/traineros/audio-backups')/time.strftime('%Y%m%d-%H%M%S')
    backup.mkdir(parents=True,mode=0o700)
    for src,name in [(boot,'KERNEL'),(dtb,'odin2.dtb'),(stamp,'bootimg.id'),(original,'HiFi.conf')]:
        shutil.copy2(src,backup/name)
    spare_sha=sha(spare)
    manifest={'baseline_dtb':sha(dtb),'candidate_dtb':sha(candidate),'baseline_boot':sha(boot),'spare':spare_sha,'dtb_path':str(dtb),'overlay':str(overlay),'dropin':str(dropin)}
    (backup/'manifest.json').write_text(json.dumps(manifest,indent=2))
    boot_readonly='ro' in subprocess.check_output(['findmnt','-rn','-o','OPTIONS','/boot'],text=True).strip().split(',')
    try:
        if boot_readonly:subprocess.run(['mount','-o','remount,rw','/boot'],check=True)
        # Inherit the system UCM tree via links; override only this board's HiFi.
        def overlay_tree(source,target,parts):
            target.mkdir(parents=True,exist_ok=True)
            for child in source.iterdir():
                dest=target/child.name
                if parts and child.name==parts[0]:
                    if len(parts)==1:
                        dest.write_text(child.read_text()+'\n'+fragment.read_text())
                    else:overlay_tree(child,dest,parts[1:])
                else:dest.symlink_to(child)
        overlay_tree(Path('/usr/share/alsa/ucm2'),overlay,['AYN','Odin2','HiFi.conf'])
        dropin.parent.mkdir(parents=True,exist_ok=True)
        dropin.write_text('[Service]\nEnvironment=ALSA_CONFIG_UCM2='+str(overlay)+'\n')
        tmp=dtb.with_suffix('.traineros-next');shutil.copy2(candidate,tmp);os.replace(tmp,dtb)
        subprocess.run(['/usr/libexec/armada/armada-bootimg-update'],check=True,timeout=90)
        # Verify that the kernel, ramdisk and every other appended DTB stayed intact.
        headers=[]
        for source,name in [(backup/'KERNEL','old'),(boot,'new')]:
            folder=backup/name
            header=subprocess.check_output(['unpack_bootimg','--boot_img',str(source),'--out',str(folder),'--format','mkbootimg','-0'])
            headers.append(header.replace(str(folder).encode(),b'COMPONENTS'))
        if headers[0]!=headers[1]:
            raise ValueError('Boot command line or header geometry changed')
        old=backup/'old';new=backup/'new'
        if sha(old/'ramdisk')!=sha(new/'ramdisk'):
            raise ValueError('Ramdisk changed unexpectedly')
        def split(path):
            dec=zlib.decompressobj(31);kernel=dec.decompress(path.read_bytes())+dec.flush()
            return kernel,dec.unused_data
        ok,od=split(old/'kernel');nk,nd=split(new/'kernel')
        before=baseline.read_bytes();after=candidate.read_bytes()
        if ok!=nk or od.count(before)!=1 or nd!=od.replace(before,after,1) or sha(spare)!=spare_sha:
            raise ValueError('Unexpected boot components or recovery image changed')
        manifest['installed_boot']=sha(boot)
        (backup/'manifest.json').write_text(json.dumps(manifest,indent=2))
        os.sync()
        print(json.dumps({'backup':str(backup),'installed_boot':sha(boot),'recovery_unchanged':True,'reboot_required':True}))
    except Exception:
        for src,dest in [(backup/'odin2.dtb',dtb),(backup/'KERNEL',boot),(backup/'bootimg.id',stamp)]:
            if sha(src)!=sha(dest):
                tmp=dest.with_name(dest.name+'.restore');shutil.copy2(src,tmp);os.replace(tmp,dest)
        if dropin.exists():dropin.unlink()
        if overlay.exists():shutil.rmtree(overlay)
        os.sync()
        raise
    finally:
        if boot_readonly:subprocess.run(['mount','-o','remount,ro','/boot'],check=True)


if __name__=='__main__':
    if len(sys.argv)!=4:raise SystemExit('Usage: install-odin2-mic.py BASELINE.dtb CANDIDATE.dtb MIC.conf')
    main(*map(Path,sys.argv[1:]))
