"""Copy only reviewed Wolf parts and apply bounded C++/GObject ownership fixes."""
from pathlib import Path
import shutil
import sys

source, target = map(Path, sys.argv[1:])
for directory in ("src/moonlight-server/gst-plugin", "src/moonlight-protocol/crypto",
                  "src/moonlight-protocol/moonlight", "src/moonlight-protocol/nanors"):
    shutil.copytree(source / directory, target / directory, dirs_exist_ok=True)
shutil.copy2(source / "LICENSE", target / "LICENSE")


def replace(file, old, new, directory="src/moonlight-server/gst-plugin"):
    path = target / directory / file
    text = path.read_text()
    if text.count(old) != 1:
        raise RuntimeError(f"Upstream changed: {file}")
    path.write_text(text.replace(old, new))


# An authentication failure must never return unauthenticated plaintext. The
# pinned helper otherwise only prints the failure and continues execution.
replace("utils.cpp", "  ERR_print_errors_fp(stderr);\n  std::cout << msg << std::endl;",
        "  ERR_clear_error();\n  throw std::runtime_error(msg);",
        "src/moonlight-protocol/crypto/src")
replace("utils.hpp", "    gst_buffer_append(buf, buf_idx);",
        "    buf = gst_buffer_append(buf, buf_idx);")
# Moonlight VideoDepacketizer starts at frame 1 and discards frame 0, including
# the encoder's initial SPS/PPS/IDR. Start at the receiver's first frame.
replace("gstrtpmoonlightpay_video.cpp", "  rtpmoonlightpay_video->frame_num = 0;",
        "  rtpmoonlightpay_video->frame_num = 1;")
replace("gstrtpmoonlightpay_audio.hpp", "#include <array>",
        "#include <array>\n#include <string>\n#include <memory>\n#include <new>")
replace("gstrtpmoonlightpay_audio.cpp",
        "  rtpmoonlightpay_audio->cur_seq_number = 0;",
        "  new (&rtpmoonlightpay_audio->aes_key) std::string();\n"
        "  new (&rtpmoonlightpay_audio->aes_iv) std::string();\n"
        "  new (&rtpmoonlightpay_audio->rs) moonlight::fec::rs_ptr();\n"
        "  rtpmoonlightpay_audio->cur_seq_number = 0;")
replace("gstrtpmoonlightpay_audio.cpp",
        "  for (int i = 0; i < AUDIO_TOTAL_SHARDS; i++) {\n"
        "    delete[] rtpmoonlightpay_audio->packets_buffer[i];\n"
        "  }\n  delete[] rtpmoonlightpay_audio->packets_buffer;",
        "  if (rtpmoonlightpay_audio->packets_buffer) {\n"
        "    for (int i = 0; i < AUDIO_TOTAL_SHARDS; i++)\n"
        "      delete[] rtpmoonlightpay_audio->packets_buffer[i];\n"
        "    delete[] rtpmoonlightpay_audio->packets_buffer;\n"
        "    rtpmoonlightpay_audio->packets_buffer = nullptr;\n  }")
replace("gstrtpmoonlightpay_audio.cpp",
        "  G_OBJECT_CLASS(gst_rtp_moonlight_pay_audio_parent_class)->finalize(object);",
        "  std::destroy_at(&rtpmoonlightpay_audio->aes_key);\n"
        "  std::destroy_at(&rtpmoonlightpay_audio->aes_iv);\n"
        "  std::destroy_at(&rtpmoonlightpay_audio->rs);\n"
        "  G_OBJECT_CLASS(gst_rtp_moonlight_pay_audio_parent_class)->finalize(object);")
