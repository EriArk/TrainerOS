#!/usr/bin/env python3
"""Keep the pinned native decoder; replace only its GUI error notification."""
import subprocess
import sys
from pathlib import Path

SOURCE = 'fa50bb1976065c4f8b1b47af227d367fe9771555'
CHD = '8bba7745d758627258b315997a860039244cedaf'
root, target = map(Path, sys.argv[1:])
for path, expected in ((root, SOURCE), (root / 'ext/libchdr', CHD)):
    actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != expected:
        raise SystemExit('Unreviewed PPSSPP reader source')
for name in ('Core/FileSystems/BlockDevices.cpp', 'Core/FileSystems/BlockDevices.h',
             'Core/ELF/PBPReader.cpp', 'Core/ELF/PBPReader.h', 'Core/Loaders.h',
             'Common/StringUtils.cpp', 'Common/Data/Text/Parsers.cpp'):
    subprocess.run(['git', '-C', str(root), 'diff', '--exit-code', 'HEAD', '--', name], check=True)
text = (root / 'Core/FileSystems/BlockDevices.cpp').read_text(encoding='utf8')
begin = text.index('void BlockDevice::NotifyReadError() {')
end = text.index('\nFileBlockDevice::FileBlockDevice(', begin)
text = text[:begin] + '''void BlockDevice::NotifyReadError() {
    reportedError_ = true;
    errorString_ = "Disc read failed";
}
''' + text[end:]
target.write_text(text, encoding='utf8')
