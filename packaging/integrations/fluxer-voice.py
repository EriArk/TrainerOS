#!/usr/bin/env python3
"""Native LiveKit media worker. Grants arrive over stdin, never argv or logs.

Requires livekit==1.1.19 and PulseAudio-compatible parec/pacat (PipeWire on Armada).
The parent owns Fluxer placement/consent; this child owns only its audio streams.
"""
import asyncio
import ctypes
import json
import os
import signal
import sys
from livekit import rtc


def die_with_parent():
    ctypes.CDLL(None).prctl(1, signal.SIGKILL)


def emit(event, **fields):
    print(json.dumps(dict(event=event, **fields)), flush=True)


async def main():
    die_with_parent()
    reader = asyncio.StreamReader(limit=65536)
    await asyncio.get_running_loop().connect_read_pipe(lambda: asyncio.StreamReaderProtocol(reader), sys.stdin)
    grant = json.loads(await asyncio.wait_for(reader.readline(), 10))
    room = rtc.Room()
    source = rtc.AudioSource(48000, 1, queue_size_ms=200)
    track = rtc.LocalAudioTrack.create_audio_track('microphone', source)
    capture = None
    capture_task = None
    streams = set()
    outputs = set()
    muted = True
    deaf = False
    closing = asyncio.Event()

    async def input_device():
        query = await asyncio.create_subprocess_exec('/usr/bin/pactl', 'get-default-source',
            stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL, preexec_fn=die_with_parent)
        try:
            output, _ = await asyncio.wait_for(query.communicate(), 3)
        except asyncio.TimeoutError:
            query.kill(); await query.wait(); return None
        name = output.decode().strip()
        # Never substitute the speaker monitor for a missing microphone.
        return name if query.returncode == 0 and name and not name.endswith('.monitor') else None

    async def play(remote_track):
        process = None
        stream = rtc.AudioStream(remote_track, sample_rate=48000, num_channels=1, capacity=10)
        try:
            process = await asyncio.create_subprocess_exec('/usr/bin/pacat', '--playback', '--raw', '--format=s16le',
                '--rate=48000', '--channels=1', '--latency-msec=60', stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL, preexec_fn=die_with_parent)
            outputs.add(process)
            async for event in stream:
                if not deaf:
                    process.stdin.write(bytes(event.frame.data))
                    await process.stdin.drain()
        except (BrokenPipeError, ConnectionError):
            emit('output-error')
        finally:
            await stream.aclose()
            if process:
                outputs.discard(process)
                if process.returncode is None: process.terminate()
                await process.wait()

    @room.on('track_subscribed')
    def subscribed(remote_track, publication, participant):
        if remote_track.kind == rtc.TrackKind.KIND_AUDIO:
            task = asyncio.create_task(play(remote_track)); streams.add(task)
            task.add_done_callback(streams.discard)

    def participants(*args):
        emit('participants', count=len(room.remote_participants)+1)

    room.on('participant_connected', participants)
    room.on('participant_disconnected', participants)
    room.on('disconnected', lambda *args: closing.set())

    async def stop_capture():
        nonlocal capture, capture_task
        if capture_task: capture_task.cancel(); await asyncio.gather(capture_task, return_exceptions=True); capture_task=None
        if capture:
            if capture.returncode is None: capture.terminate()
            await capture.wait(); capture=None
        source.clear_queue()

    async def pump():
        try:
            while True:
                pcm = await capture.stdout.readexactly(960*2)
                await source.capture_frame(rtc.AudioFrame(pcm,48000,1,960))
        except asyncio.IncompleteReadError:
            emit('input-error')

    options = rtc.RoomOptions(auto_subscribe=True, connect_timeout=15)
    if grant.get('e2ee_key'):
        options.encryption = rtc.E2EEOptions(key_provider_options=rtc.KeyProviderOptions(shared_key=grant['e2ee_key'].encode()))
    try:
        await room.connect(grant['endpoint'], grant['token'], options=options)
        grant.clear()
        publication = await room.local_participant.publish_track(track, rtc.TrackPublishOptions(source=rtc.TrackSource.SOURCE_MICROPHONE))
        track.mute()
        emit('connected'); participants()
        while not closing.is_set():
            line = await asyncio.wait_for(reader.readline(), 12)
            if not line: break
            command = json.loads(line)
            if command['op'] == 'leave': break
            if command['op'] == 'mute':
                muted = bool(command['value'])
                if muted: track.mute(); await stop_capture()
                elif capture is None:
                    device = await input_device()
                    if not device:
                        muted = True; emit('input-error'); continue
                    capture = await asyncio.create_subprocess_exec('/usr/bin/parec', '--record', '--raw', '--format=s16le',
                        '--device='+device, '--client-name=TrainerOS Voice', '--rate=48000', '--channels=1', '--latency-msec=40', stdout=asyncio.subprocess.PIPE,
                        stderr=asyncio.subprocess.DEVNULL, preexec_fn=die_with_parent)
                    track.unmute(); capture_task=asyncio.create_task(pump())
                emit('muted', value=muted)
            if command['op'] == 'deaf': deaf=bool(command['value'])
    finally:
        await stop_capture()
        for task in list(streams): task.cancel()
        await asyncio.gather(*streams, return_exceptions=True)
        await room.disconnect(); await source.aclose()
        emit('ended')


if __name__ == '__main__':
    try: asyncio.run(main())
    except (Exception, KeyboardInterrupt) as error:
        # SDK exceptions can contain grant URLs. Never forward raw diagnostics.
        emit('failed', reason=type(error).__name__)
        sys.exit(1)
