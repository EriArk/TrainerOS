#!/usr/bin/env python3
"""Bounded Linux hardware-codec probe; no capture, networking or input injection.

Requires system PyGObject, GStreamer and v4l2h264enc/v4l2h264dec. Measures the
local codec pipeline only, never button-to-display latency. No installation,
root access, audio output or persistent media files are required.
"""

import argparse
import json
import math
import platform
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--frames", type=int, default=180)
    args = parser.parse_args()
    if not 60 <= args.frames <= 600:
        parser.error("--frames must be between 60 and 600")
    try:
        import gi
        gi.require_version("Gst", "1.0")
        from gi.repository import Gst
    except (ImportError, ValueError) as exc:
        print(json.dumps({"passed": False, "error": str(exc)}))
        return 1

    Gst.init(None)
    description = (
        f"videotestsrc is-live=true num-buffers={args.frames} "
        "! video/x-raw,format=NV12,width=1280,height=720,framerate=60/1 "
        "! v4l2h264enc name=encoder "
        'extra-controls="controls,video_bitrate=8000000,h264_i_frame_period=60" '
        "! video/x-h264,profile=constrained-baseline,level=(string)4.2 "
        "! h264parse name=parser "
        "! video/x-h264,stream-format=byte-stream,alignment=au "
        "! v4l2h264dec name=decoder ! fakesink sync=false async=false"
    )
    result = {
        "passed": False, "scope": "local synthetic encode/decode; no network or display",
        "kernel": platform.release(), "gstreamer": Gst.version_string(),
        "requested_frames": args.frames, "pipeline": description,
    }
    try:
        pipeline = Gst.parse_launch(description)
    except Exception as exc:
        result["error"] = str(exc)
        print(json.dumps(result))
        return 1

    stamps = {}
    durations = {"encode_ms": [], "decode_ms": [], "codec_roundtrip_ms": []}
    counts = {"raw": 0, "encoded": 0, "decoded": 0}
    caps = {}
    bad_pts = []
    warnings = []

    def observe(stage):
        def callback(pad, info):
            buffer = info.get_buffer()
            if buffer is None:
                return Gst.PadProbeReturn.OK
            now = time.monotonic_ns()
            counts[stage] += 1
            if stage not in caps:
                current = pad.get_current_caps()
                caps[stage] = current.to_string() if current else None
            pts = buffer.pts
            if pts == Gst.CLOCK_TIME_NONE:
                bad_pts.append(stage)
                return Gst.PadProbeReturn.OK
            if stage == "raw":
                stamps[pts] = {"raw": now, "number": counts[stage]}
            else:
                record = stamps.get(pts)
                if record is None or stage in record:
                    bad_pts.append(stage)
                    return Gst.PadProbeReturn.OK
                record[stage] = now
                # Exclude 30 initial frames from the latency distribution.
                if record["number"] > 30:
                    if stage == "encoded":
                        durations["encode_ms"].append((now - record["raw"]) / 1e6)
                    elif "encoded" in record:
                        durations["decode_ms"].append((now - record["encoded"]) / 1e6)
                        durations["codec_roundtrip_ms"].append((now - record["raw"]) / 1e6)
            return Gst.PadProbeReturn.OK
        return callback

    for element, pad_name, stage in [
        ("encoder", "sink", "raw"), ("parser", "src", "encoded"),
        ("decoder", "src", "decoded"),
    ]:
        pipeline.get_by_name(element).get_static_pad(pad_name).add_probe(
            Gst.PadProbeType.BUFFER, observe(stage))
    result["devices"] = {
        name: pipeline.get_by_name(name).get_property("device")
        for name in ("encoder", "decoder")
    }
    bus = pipeline.get_bus()
    deadline = time.monotonic() + args.frames / 60 + 10
    eos = False
    try:
        if pipeline.set_state(Gst.State.PLAYING) == Gst.StateChangeReturn.FAILURE:
            result["error"] = "Pipeline could not enter PLAYING"
        else:
            while time.monotonic() < deadline:
                message = bus.timed_pop_filtered(
                    Gst.SECOND, Gst.MessageType.ERROR | Gst.MessageType.EOS | Gst.MessageType.WARNING)
                if message is None:
                    continue
                if message.type == Gst.MessageType.EOS:
                    eos = True
                    break
                if message.type == Gst.MessageType.ERROR:
                    error, debug = message.parse_error()
                    result["error"] = str(error)
                    result["debug"] = debug
                    break
                warning, debug = message.parse_warning()
                warnings.append({"message": str(warning), "debug": debug})
            if not eos and "error" not in result:
                result["error"] = "Timed out before EOS"
    finally:
        pipeline.set_state(Gst.State.NULL)

    def stats(samples):
        ordered = sorted(samples)
        if not ordered:
            return {"samples": 0}
        return {
            "samples": len(ordered),
            "p50": round(ordered[math.ceil(len(ordered) * .50) - 1], 3),
            "p95": round(ordered[math.ceil(len(ordered) * .95) - 1], 3),
            "max": round(ordered[-1], 3),
        }

    result.update(counts=counts, caps=caps, warnings=warnings,
                  unmatched_timestamps=len(bad_pts),
                  measurements={name: stats(values) for name, values in durations.items()})
    result["passed"] = (eos and not warnings and not bad_pts
                        and all(n == args.frames for n in counts.values())
                        and all(len(values) == args.frames - 30 for values in durations.values()))
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
