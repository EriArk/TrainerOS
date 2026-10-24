#!/usr/bin/env python3
"""Fill an entirely unassigned melonDS pad using a verified SDL controller.

Does not replace existing user mappings. Run with melonDS closed and the same
SDL device visibility as its runtime. Hotkeys and keyboard mappings are retained.
"""
import argparse
import ctypes
import ctypes.util
import os
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--device', required=True)
    args = parser.parse_args()
    for proc in Path('/proc').glob('[0-9]*'):
        try:
            if (proc / 'exe').readlink().name.lower() == 'melonds':
                raise SystemExit('Close melonDS first.')
        except OSError:
            pass
    library = ctypes.util.find_library('SDL2') or ctypes.util.find_library('SDL2-2.0')
    sdl = ctypes.CDLL(library or 'libSDL2-2.0.so.0')
    if sdl.SDL_InitSubSystem(0x2000 | 0x200):
        raise SystemExit('SDL input unavailable.')
    sdl.SDL_JoystickNameForIndex.restype = ctypes.c_char_p
    sdl.SDL_GameControllerMappingForDeviceIndex.restype = ctypes.c_void_p
    sdl.SDL_free.argtypes = [ctypes.c_void_p]
    matches = [i for i in range(sdl.SDL_NumJoysticks())
               if sdl.SDL_JoystickNameForIndex(i).decode() == args.device]
    if len(matches) != 1:
        raise SystemExit('Expected one matching verified controller.')
    index = matches[0]
    pointer = sdl.SDL_GameControllerMappingForDeviceIndex(index)
    if not pointer:
        raise SystemExit('No SDL mapping for controller.')
    fields = ctypes.string_at(pointer).decode().split(',')[2:]
    sdl.SDL_free(pointer)
    mapping = dict(item.split(':', 1) for item in fields if ':' in item)
    names = {'A': 'b', 'B': 'a', 'X': 'y', 'Y': 'x', 'L': 'leftshoulder',
             'R': 'rightshoulder', 'Start': 'start', 'Select': 'back',
             'Up': 'dpup', 'Down': 'dpdown', 'Left': 'dpleft', 'Right': 'dpright'}
    values = {}
    for name, semantic in names.items():
        value = mapping.get(semantic, '')
        if re.fullmatch(r'b\d+', value):
            values[name] = int(value[1:])
        elif re.fullmatch(r'h\d+\.[1248]', value):
            hat, direction = map(int, value[1:].split('.'))
            if hat > 15:
                raise SystemExit('Unsupported hat number.')
            values[name] = 0x100 | (hat << 4) | direction
        else:
            raise SystemExit('Unsupported SDL button binding: ' + semantic)
    original = args.config.read_bytes()
    text = original.decode('utf-8')
    section = re.search(r'^\[Instance0.Joystick\]\r?\n(.*?)(?=^\[|\Z)', text, re.M | re.S)
    if not section:
        raise SystemExit('Missing joystick section.')
    body = section[1]
    for name in values:
        if not re.search(r'^' + name + r' = -1\r?$', body, re.M):
            raise SystemExit('Existing button bindings retained; review manually.')
    for name, value in values.items():
        body = re.sub(r'^' + name + r' = -1', name + ' = ' + str(value), body, flags=re.M)
    text = text[:section.start(1)] + body + text[section.end(1):]
    if len(re.findall(r'^JoystickID = \d+', text, re.M)) != 1:
        raise SystemExit('Review multi-instance controller IDs separately.')
    text = re.sub(r'^JoystickID = \d+', 'JoystickID = ' + str(index), text, flags=re.M)
    backup = args.config.with_name(args.config.name + '.before-launch80-input')
    if backup.exists() and backup.read_bytes() != original:
        raise SystemExit('Different backup exists; review first.')
    if not backup.exists():
        backup.write_bytes(original)
    temporary = args.config.with_name(args.config.name + '.launch80-new')
    with temporary.open('x', encoding='utf-8', newline='') as output:
        output.write(text)
    temporary.chmod(args.config.stat().st_mode)
    os.replace(temporary, args.config)
    print('Bound unassigned controller', index, args.device, values)


if __name__ == '__main__':
    main()
