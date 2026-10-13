#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Owned-game audio for the isolated stream probe: 48 kHz stereo S16LE stdout.

Reuses TrainerOS's exact process/Flatpak-instance selection and Pulse's per-stream
monitor. Never falls back to the desktop mix or microphone. GStreamer mixes the
selected streams; silence preserves the clock while the game has no audio track.
"""
import asyncio
import signal
import select
import subprocess
import sys
import time

sys.path.insert(0, '/var/opt/traineros/integrations')
from game_capture import GameCapture


class AudioCapture:
    def __init__(self, gst):
        self.gst = gst
        self.pipeline = None
        self.recorders = []
        self.inputs = None

    def close(self):
        # Unblock fdsrc before setting the pipeline to NULL.
        for process in self.recorders:
            if process.poll() is None:
                process.terminate()
        for process in self.recorders:
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if self.pipeline is not None:
            self.pipeline.set_state(self.gst.State.NULL)
            self.pipeline = None
        for process in self.recorders:
            process.stdout.close()
        self.recorders.clear()
        self.inputs = None

    def select(self, inputs):
        if inputs == self.inputs and all(p.poll() is None for p in self.recorders):
            return
        self.close()
        if len(inputs) > 8:
            raise RuntimeError('Too many game audio streams')
        try:
            description = (
                'audiomixer name=mix ignore-inactive-pads=true '
                'output-buffer-duration=5000000 ! '
                'audio/x-raw,format=S16LE,rate=48000,channels=2,layout=interleaved '
                '! fdsink fd=1 sync=false '
                'audiotestsrc is-live=true wave=silence samplesperbuffer=240 ! '
                'audio/x-raw,format=S16LE,rate=48000,channels=2 ! mix. ')
            for index, monitor in sorted(inputs.items()):
                process = subprocess.Popen([
                    '/usr/bin/parec', '--record', '--raw', '--format=s16le',
                    '--rate=48000', '--channels=2', '--latency-msec=20',
                    '--client-name=TrainerOS Remote Game Audio',
                    '--device=' + monitor, '--monitor-stream=' + str(index)],
                    stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                    stderr=subprocess.DEVNULL)
                self.recorders.append(process)
                description += (
                    f'fdsrc fd={process.stdout.fileno()} do-timestamp=true ! '
                    'rawaudioparse pcm-format=s16le sample-rate=48000 num-channels=2 ! '
                    'queue max-size-buffers=4 max-size-bytes=0 max-size-time=0 leaky=downstream ! mix. ')
            self.pipeline = self.gst.parse_launch(description)
            if self.pipeline.set_state(self.gst.State.PLAYING) == self.gst.StateChangeReturn.FAILURE:
                raise RuntimeError('Cannot start game audio capture')
            self.inputs = dict(inputs)
            print('owned_audio_streams=' + str(len(inputs)), file=sys.stderr, flush=True)
        except BaseException:
            self.close()
            raise

    def check(self):
        message = self.pipeline.get_bus().pop_filtered(self.gst.MessageType.ERROR)
        if message is not None:
            error, _ = message.parse_error()
            pipe = select.poll()
            pipe.register(1, select.POLLERR | select.POLLHUP)
            if pipe.poll(0):
                raise BrokenPipeError('Host closed game audio')
            raise RuntimeError(str(error))


def main():
    if len(sys.argv) != 2:
        raise SystemExit('Expected current game PID')
    import gi
    gi.require_version('Gst', '1.0')
    from gi.repository import Gst
    Gst.init(None)
    game = GameCapture(int(sys.argv[1]))
    audio = AudioCapture(Gst)
    def stop(*_):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, stop)
    try:
        while game.alive():
            inputs = asyncio.run(game.audio_inputs())
            # Recheck after asynchronous provider queries: never start a source
            # belonging to a game that ended while those queries were running.
            if not game.alive():
                break
            audio.select(inputs)
            audio.check()
            time.sleep(.25)
    except (KeyboardInterrupt, BrokenPipeError):
        pass
    finally:
        audio.close()
        game.close()


if __name__ == '__main__':
    main()
