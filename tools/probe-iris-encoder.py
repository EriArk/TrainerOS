#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check visible H.264 geometry and an actual requested IDR on Linux V4L2.

Synthetic 720p bands only; software decode isolates the encoder from a hardware
decoder. No audio, capture, network, root access or persistent media. Requires
PyGObject, GStreamer, v4l2h264enc and avdec_h264. Exits nonzero on either failure.
"""
import json
import platform
import re
import time


def main():
    import gi
    gi.require_version("Gst", "1.0")
    gi.require_version("GstVideo", "1.0")
    from gi.repository import Gst, GstVideo
    Gst.init(None)
    pipeline = Gst.parse_launch(
        'appsrc name=input is-live=true format=time block=false '
        'caps="video/x-raw,format=NV12,width=1280,height=720,framerate=60/1" '
        '! v4l2h264enc name=encoder '
        'extra-controls="controls,video_bitrate=6000000,video_gop_size=60" '
        '! video/x-h264,profile=constrained-baseline,level=(string)4.2 '
        '! h264parse name=parser config-interval=-1 '
        '! video/x-h264,stream-format=byte-stream,alignment=au '
        '! avdec_h264 ! videoconvert ! video/x-raw,format=RGB '
        '! appsink name=output emit-signals=true sync=false')
    result = {"kernel": platform.release(), "gstreamer": Gst.version_string(),
              "encoded": 0, "decoded": 0, "idr_frames": [], "sps_frames": [],
              "geometry": None, "request_after_encoded": None,
              "request_event_accepted": False, "errors": []}

    def encoded(pad, info):
        buffer = info.get_buffer()
        data = buffer.extract_dup(0, buffer.get_size())
        result["encoded"] += 1
        types = {data[m.end()] & 31 for m in re.finditer(b"\x00\x00\x01", data)
                 if m.end() < len(data)}
        for kind, field in ((5, "idr_frames"), (7, "sps_frames")):
            if kind in types:
                result[field].append(result["encoded"])
        return Gst.PadProbeReturn.OK

    def decoded(sink):
        sample = sink.emit("pull-sample")
        result["decoded"] += 1
        if result["geometry"] is None:
            video = GstVideo.VideoInfo.new_from_caps(sample.get_caps())
            buffer = sample.get_buffer()
            data = buffer.extract_dup(0, buffer.get_size())
            rows = [tuple(data[y * video.stride[0] + 640 * 3:
                               y * video.stride[0] + 640 * 3 + 3])
                    for y in range(video.height)]
            edges = [y for y in range(1, len(rows))
                     if (sum(rows[y]) > 384) != (sum(rows[y - 1]) > 384)]
            expected = list(range(45, 720, 45))
            neutral = all(max(row) - min(row) <= 12 for row in rows)
            aligned = len(edges) == len(expected) and all(
                abs(a - b) <= 1 for a, b in zip(edges, expected))
            result["geometry"] = {
                "width": video.width, "height": video.height,
                "band_edges": edges, "neutral_rows": neutral,
                "bottom_rgb": rows[-1],
                "passed": video.width == 1280 and video.height == 720
                and aligned and neutral and min(rows[-1]) > 220}
        return Gst.FlowReturn.OK

    pipeline.get_by_name("parser").get_static_pad("src").add_probe(
        Gst.PadProbeType.BUFFER, encoded)
    pipeline.get_by_name("output").connect("new-sample", decoded)
    source = pipeline.get_by_name("input")
    # NV12 storage is negotiated by GStreamer; the visible input is exactly 720.
    raw = b"".join(bytes([16 if (y // 45) % 2 == 0 else 235]) * 1280
                   for y in range(720)) + bytes([128]) * (1280 * 360)
    bus = pipeline.get_bus()
    eos = False
    try:
        if pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
            raise RuntimeError("Cannot start encoder pipeline")
        start = time.monotonic()
        for frame in range(180):
            if result["encoded"] >= 75 and result["request_after_encoded"] is None:
                result["request_after_encoded"] = result["encoded"]
                result["request_event_accepted"] = pipeline.get_by_name("encoder").send_event(
                    GstVideo.video_event_new_upstream_force_key_unit(Gst.CLOCK_TIME_NONE, True, 1))
            buffer = Gst.Buffer.new_allocate(None, len(raw), None)
            buffer.fill(0, raw)
            buffer.pts = frame * Gst.SECOND // 60
            buffer.duration = Gst.SECOND // 60
            if source.emit("push-buffer", buffer) != Gst.FlowReturn.OK:
                raise RuntimeError("Encoder input stopped")
            time.sleep(max(0, start + (frame + 1) / 60 - time.monotonic()))
        source.emit("end-of-stream")
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            msg = bus.timed_pop_filtered(Gst.SECOND, Gst.MessageType.ERROR | Gst.MessageType.EOS)
            if msg is None:
                continue
            if msg.type == Gst.MessageType.ERROR:
                error, _ = msg.parse_error()
                result["errors"].append(str(error))
            else:
                eos = True
            break
    except Exception as exc:
        result["errors"].append(str(exc))
    finally:
        pipeline.set_state(Gst.State.NULL)
    requested = result["request_after_encoded"]
    result["requested_idr"] = requested is not None and any(
        requested < frame <= requested + 15 and frame in result["sps_frames"]
        for frame in result["idr_frames"])
    result["passed"] = bool(eos and not result["errors"] and result["geometry"]
                            and result["geometry"]["passed"] and result["requested_idr"]
                            and result["decoded"] == result["encoded"] == 180)
    print(json.dumps(result))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
