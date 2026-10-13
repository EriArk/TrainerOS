"""Fluxer Go Live tracks on the existing LiveKit connection, never a new room."""
import asyncio
import struct
from livekit import rtc
from voice_presence import account

HEADER = struct.Struct('<IIII')
MAGIC = 0x31565254


def frame_header(data):
    magic, width, height, size = HEADER.unpack(data)
    if magic != MAGIC or not (0 < width <= 1280 and 0 < height <= 720) or size != width * height * 4:
        raise ValueError('Invalid game frame')
    return width, height, size


class ScreenStreams:
    def __init__(self, room, emit, stop_process, die_with_parent):
        self.room, self.emit = room, emit
        self.stop_process, self.die_with_parent = stop_process, die_with_parent
        self.publisher = None
        self.viewer = None
        self.watching = ''
        self.socket = ''
        self.blocked = set()

    def publications(self):
        for p in self.room.remote_participants.values():
            if not account(p.identity) or account(p.identity) in self.blocked:
                continue
            for publication in p.track_publications.values():
                yield p, publication

    def catalogue(self, *args):
        rows = []
        for p, pub in self.publications():
            if pub.source == rtc.TrackSource.SOURCE_SCREENSHARE and not pub.muted:
                rows.append({'identity': p.identity, 'account': account(p.identity)})
        self.emit('streams', streams=rows[:32])
        if self.watching and not any(row['identity'] == self.watching for row in rows):
            self.watch('', '')
        self.subscribe()

    def subscribe(self):
        # Microphones stay part of the call. Screen audio/video are subscribed
        # only for the chosen stream; camera and other screen tracks stay off.
        for p in self.room.remote_participants.values():
            for pub in p.track_publications.values():
                allowed = bool(account(p.identity)) and account(p.identity) not in self.blocked
                selected = allowed and (pub.source == rtc.TrackSource.SOURCE_MICROPHONE or
                    (p.identity == self.watching and pub.source in
                     (rtc.TrackSource.SOURCE_SCREENSHARE, rtc.TrackSource.SOURCE_SCREENSHARE_AUDIO)))
                pub.set_subscribed(selected)

    def watch(self, identity, socket):
        if self.viewer:
            self.viewer.cancel()
            self.viewer = None
        self.watching, self.socket = identity, socket
        self.subscribe()
        self.emit('watching', identity=identity)
        for p, pub in self.publications():
            if p.identity == identity and pub.source == rtc.TrackSource.SOURCE_SCREENSHARE and pub.track:
                self.video(pub.track, p)

    def video(self, track, participant):
        if participant.identity != self.watching or not self.socket:
            return
        if self.viewer:
            self.viewer.cancel()
        self.viewer = asyncio.create_task(self.receive_video(track, participant.identity, self.socket))

    async def receive_video(self, track, identity, socket):
        stream = rtc.VideoStream(track, capacity=1, format=rtc.VideoBufferType.BGRA)
        writer = None
        try:
            _, writer = await asyncio.wait_for(asyncio.open_unix_connection(socket), 3)
            async for event in stream:
                if identity != self.watching:
                    break
                f = event.frame
                # Provider limits are not a substitute for our local frame bound.
                if not (0 < f.width <= 1280 and 0 < f.height <= 720):
                    raise ValueError('Stream resolution unavailable')
                writer.write(HEADER.pack(MAGIC, f.width, f.height, f.width * f.height * 4))
                writer.write(f.data)
                await asyncio.wait_for(writer.drain(), 2)
        except asyncio.CancelledError:
            raise
        except (BrokenPipeError, ConnectionResetError):
            # Closing the local viewer is an ordinary stop, not a failed call.
            if identity == self.watching:
                self.watching = ''; self.socket = ''; self.subscribe()
                self.emit('watching', identity='')
        except Exception:
            if identity == self.watching:
                self.watching = ''; self.socket = ''; self.subscribe()
                self.emit('watching', identity='')
                self.emit('stream-error', operation='watch')
        finally:
            await stream.aclose()
            if writer:
                writer.close()
                try: await writer.wait_closed()
                except (OSError, ConnectionError): pass

    async def stop_share(self):
        if self.publisher:
            self.publisher.cancel()
            await asyncio.gather(self.publisher, return_exceptions=True)
            self.publisher = None

    async def share(self, pid):
        await self.stop_share()
        if pid:
            self.publisher = asyncio.create_task(self.publish_game(pid))

    async def publish_game(self, pid):
        from game_capture import GameCapture
        game = None
        process = None
        source = None
        publication = None
        audio = None
        try:
            game = GameCapture(pid)
            process = await asyncio.create_subprocess_exec(
                '/var/opt/traineros/integrations/traineros-game-capture', str(game.appid),
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL,
                preexec_fn=self.die_with_parent)
            # Do not announce publication until the owned capture produces a frame.
            header = await asyncio.wait_for(process.stdout.readexactly(16), 8)
            w, h, size = frame_header(header)
            pixels = await asyncio.wait_for(process.stdout.readexactly(size), 3)
            source = rtc.VideoSource(w, h, is_screencast=True)
            track = rtc.LocalVideoTrack.create_video_track('game', source)
            publication = await self.room.local_participant.publish_track(track, rtc.TrackPublishOptions(
                source=rtc.TrackSource.SOURCE_SCREENSHARE, video_codec=rtc.VideoCodec.VP8,
                video_encoding=rtc.VideoEncoding(max_bitrate=2500000, max_framerate=30), simulcast=False))
            self.emit('sharing', active=True)
            audio = asyncio.create_task(self.publish_audio(game))
            while True:
                if not game.alive():
                    break
                source.capture_frame(rtc.VideoFrame(w, h, rtc.VideoBufferType.BGRA, pixels))
                header = await asyncio.wait_for(process.stdout.readexactly(16), 8)
                w, h, size = frame_header(header)
                pixels = await asyncio.wait_for(process.stdout.readexactly(size), 3)
        except asyncio.CancelledError:
            raise
        except Exception:
            self.emit('stream-error', operation='share')
        finally:
            if audio:
                audio.cancel(); await asyncio.gather(audio, return_exceptions=True)
            if process:
                await self.stop_process(process)
            if publication:
                try: await self.room.local_participant.unpublish_track(publication.sid)
                except Exception: pass
            if source:
                await source.aclose()
            if game:
                game.close()
            self.emit('sharing', active=False)

    async def publish_audio(self, game):
        # Each owned sink input gets its own screen-audio track. The existing
        # provider carries/mixes these; never record the device's complete mix.
        tasks = {}
        attempts = {}
        try:
            while game.alive():
                inputs = await game.audio_inputs()
                for index in list(tasks):
                    if index not in inputs or tasks[index][0] != inputs[index] or tasks[index][1].done():
                        task = tasks.pop(index)[1]; task.cancel(); await asyncio.gather(task, return_exceptions=True)
                for index, monitor in inputs.items():
                    count, retry = attempts.get(index, (0, 0))
                    if index not in tasks and len(tasks) < 8 and count < 3 and asyncio.get_running_loop().time() >= retry:
                        tasks[index] = (monitor, asyncio.create_task(self.audio_track(index, monitor)))
                        attempts[index] = (count + 1, asyncio.get_running_loop().time() + 5)
                await asyncio.sleep(.5)
        except asyncio.CancelledError:
            raise
        except Exception:
            self.emit('stream-error', operation='game-audio')
        finally:
            for _, task in tasks.values(): task.cancel()
            await asyncio.gather(*(task for _, task in tasks.values()), return_exceptions=True)

    async def audio_track(self, index, monitor):
        process = None
        source = rtc.AudioSource(48000, 2, queue_size_ms=100)
        publication = None
        try:
            process = await asyncio.create_subprocess_exec('/usr/bin/parec', '--record', '--raw',
                '--format=s16le', '--rate=48000', '--channels=2', '--latency-msec=40',
                '--client-name=TrainerOS Game Audio', '--device='+monitor, '--monitor-stream='+str(index),
                stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.DEVNULL, preexec_fn=self.die_with_parent)
            track = rtc.LocalAudioTrack.create_audio_track('game-audio-'+str(index), source)
            publication = await self.room.local_participant.publish_track(track,
                rtc.TrackPublishOptions(source=rtc.TrackSource.SOURCE_SCREENSHARE_AUDIO))
            while True:
                pcm = await process.stdout.readexactly(960*4)
                await source.capture_frame(rtc.AudioFrame(pcm, 48000, 2, 960))
        except asyncio.CancelledError:
            raise
        except Exception:
            self.emit('stream-error', operation='game-audio')
        finally:
            if process: await self.stop_process(process)
            if publication:
                try: await self.room.local_participant.unpublish_track(publication.sid)
                except Exception: pass
            await source.aclose()

    async def close(self):
        await self.stop_share()
        if self.viewer:
            self.viewer.cancel(); await asyncio.gather(self.viewer, return_exceptions=True)
        self.watching = ''
