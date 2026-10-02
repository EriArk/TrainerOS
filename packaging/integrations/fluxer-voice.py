#!/usr/bin/env python3
"""Native LiveKit media worker. Grants arrive over stdin, never argv or logs.

Requires livekit==1.1.19 and PulseAudio-compatible parec/pacat (PipeWire on Armada).
The parent owns Fluxer placement/consent; this child owns only its audio streams.
"""
import asyncio
from array import array
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
    audio = grant.pop('audio', {})
    input_name = str(audio.get('input', ''))
    output_name = str(audio.get('output', ''))
    volume = max(0, min(100, int(audio.get('volume', 100))))
    output_revision = 0
    room = rtc.Room()
    source = rtc.AudioSource(48000, 1, queue_size_ms=200)
    track = rtc.LocalAudioTrack.create_audio_track('microphone', source)
    capture = None
    capture_task = None
    capture_device = None
    streams = set()
    outputs = set()
    muted = True
    deaf = False
    closing = asyncio.Event()

    async def stop_process(process):
        if process.returncode is None:
            process.terminate()
        try:
            await asyncio.wait_for(process.wait(), 2)
        except asyncio.TimeoutError:
            process.kill()
            await process.wait()

    async def input_device():
        if input_name:
            return input_name if not input_name.endswith('.monitor') else None
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
        recovering = False
        revision = output_revision
        stream = rtc.AudioStream(remote_track, sample_rate=48000, num_channels=1, capacity=10)
        try:
            async for event in stream:
                if deaf: continue
                try:
                    if process and revision != output_revision:
                        outputs.discard(process)
                        await stop_process(process); process = None
                    if process is None:
                        revision = output_revision
                        process = await asyncio.create_subprocess_exec('/usr/bin/pacat', '--playback', '--raw', '--format=s16le',
                            '--client-name=TrainerOS Voice', '--rate=48000', '--channels=1', '--latency-msec=60', stdin=asyncio.subprocess.PIPE,
                            *(['--device='+output_name] if output_name else []),
                            stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL, preexec_fn=die_with_parent)
                        outputs.add(process)
                    pcm = bytes(event.frame.data)
                    if volume != 100:
                        samples = array('h', pcm)
                        pcm = array('h', (int(value * volume / 100) for value in samples)).tobytes()
                    process.stdin.write(pcm)
                    await process.stdin.drain()
                    if recovering: emit('output-restored'); recovering = False
                except (OSError, ConnectionError):
                    if not recovering: emit('output-error')
                    recovering = True
                    if process:
                        outputs.discard(process)
                        await stop_process(process); process = None
                    # A sound-server restart must not permanently end this
                    # participant's playback or tear down the voice room.
                    await asyncio.sleep(1)
        finally:
            await stream.aclose()
            if process:
                outputs.discard(process)
                await stop_process(process)

    @room.on('track_subscribed')
    def subscribed(remote_track, publication, participant):
        if remote_track.kind == rtc.TrackKind.KIND_AUDIO:
            task = asyncio.create_task(play(remote_track)); streams.add(task)
            task.add_done_callback(streams.discard)

    def participants(*args):
        identities = [room.local_participant.identity] + list(room.remote_participants)
        # Fluxer permits the same account on several devices. Count people, not
        # connections, using the documented user_<id>_<connection> identity.
        people = {identity.split('_')[1] if identity.startswith('user_') and len(identity.split('_')) >= 3 else identity
                  for identity in identities if identity}
        emit('participants', count=max(1, len(people)))

    room.on('participant_connected', participants)
    room.on('participant_disconnected', participants)
    room.on('disconnected', lambda *args: closing.set())
    room.on('reconnecting', lambda *args: emit('reconnecting'))
    room.on('reconnected', lambda *args: emit('reconnected'))

    async def stop_capture():
        nonlocal capture, capture_task, capture_device
        if capture_task: capture_task.cancel(); await asyncio.gather(capture_task, return_exceptions=True); capture_task=None
        if capture:
            await stop_process(capture); capture=None
        capture_device = None
        source.clear_queue()

    async def pump():
        try:
            while True:
                pcm = await capture.stdout.readexactly(960*2)
                await source.capture_frame(rtc.AudioFrame(pcm,48000,1,960))
        except asyncio.IncompleteReadError:
            track.mute()
            source.clear_queue()
            emit('input-error')

    async def set_muted(value):
        nonlocal muted, capture, capture_task, capture_device
        muted = value
        if muted:
            track.mute()
            await stop_capture()
        elif capture is None:
            device = await input_device()
            if not device:
                muted = True
                track.mute()
                emit('input-error')
                return
            capture = await asyncio.create_subprocess_exec('/usr/bin/parec', '--record', '--raw', '--format=s16le',
                '--device='+device, '--client-name=TrainerOS Voice', '--rate=48000', '--channels=1', '--latency-msec=40',
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL, preexec_fn=die_with_parent)
            capture_device = device
            track.unmute()
            capture_task = asyncio.create_task(pump())
        emit('muted', value=muted)

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
                await set_muted(bool(command['value']))
            if command['op'] == 'deaf': deaf=bool(command['value'])
            if command['op'] == 'audio-settings':
                next_input = str(command.get('input', ''))
                next_output = str(command.get('output', ''))
                volume = max(0, min(100, int(command.get('volume', 100))))
                if next_output != output_name:
                    output_name = next_output; output_revision += 1
                if next_input != input_name:
                    input_name = next_input
                    if not muted:
                        track.mute(); await stop_capture(); await set_muted(False)
            if command['op'] == 'ping' and capture and not muted:
                # Follow a plugged-in headset/default microphone without touching
                # the call, game audio, or volume. Never record a speaker monitor.
                if await input_device() != capture_device:
                    track.mute()
                    await stop_capture()
                    await set_muted(False)
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
