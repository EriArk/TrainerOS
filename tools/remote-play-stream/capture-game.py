#!/usr/bin/env python3
"""Lab-only video producer using TrainerOS's existing owned-game selector.

Run in the graphical session: capture-game.py GAME_PID | stream_host CONFIG.
Only framed video is written to stdout. No desktop/root-window fallback.
"""
import subprocess
import sys

sys.path.insert(0, '/var/opt/traineros/integrations')
from game_capture import GameCapture


def main():
    if len(sys.argv) != 2:
        raise SystemExit('Expected current game PID')
    selection = GameCapture(int(sys.argv[1]))
    child = None
    try:
        child = subprocess.Popen(
            ['/var/opt/traineros/integrations/traineros-game-capture', str(selection.appid)],
            stdin=subprocess.DEVNULL)
        while child.poll() is None:
            if not selection.alive():
                child.terminate()
                raise RuntimeError('Owned game ended or capture identity changed')
            try:
                child.wait(timeout=.1)
            except subprocess.TimeoutExpired:
                pass
        return child.returncode
    finally:
        if child is not None and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=2)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
        selection.close()


if __name__ == '__main__':
    raise SystemExit(main())
