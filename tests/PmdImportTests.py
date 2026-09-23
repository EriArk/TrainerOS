"""No network or third-party artwork: exact mapping and attribution fixtures."""
import importlib.util
from pathlib import Path
import io
import unittest
from PIL import Image
spec = importlib.util.spec_from_file_location("pmd", Path(__file__).resolve().parents[1] / "tools/import_pmd_details.py")
pmd = importlib.util.module_from_spec(spec); spec.loader.exec_module(pmd)


class ImportTests(unittest.TestCase):
    def test_mapping_never_guesses_special_forms(self):
        reference = {"entries": [{"number": 37, "id": "vulpix", "forms": [
            {"id": "37", "name": "Standard"}, {"id": "10205", "name": "Alolan Form"},
            {"id": "unknown", "name": "Shiny"}]}]}
        tracker = {"0037": {"canon": True, "subgroups": {"0001": {"name": "Alola", "canon": True}}}}
        mapped, missing = pmd.mappings(reference, tracker)
        self.assertEqual([x[1] for x in mapped], ["0037", "0037/0001"])
        self.assertEqual(missing[0]["target"], "vulpix/unknown")

    def test_credits_are_per_action_and_keep_terms(self):
        names = {"artist": "Artist"}
        text = "date\tartist\tCUR\tPMDCollab_1\tNormal,Happy\n"
        self.assertEqual(pmd.credited(text, "Normal", names)[0], "Artist")
        with self.assertRaises(ValueError): pmd.credited(text, "Pain", names)
        with self.assertRaises(ValueError): pmd.credited("date\tCHUNSOFT\tCUR\tPMDCollab_1\tNormal", "Normal", names)
        with self.assertRaises(ValueError): pmd.credited(text.replace("PMDCollab_1", "Unknown"), "Normal", names)

    def test_idle_cell_is_not_whole_sheet_or_illustration(self):
        image = Image.new("RGBA", (16, 24), "blue")
        image.paste("red", (0, 0, 8, 12)); data = io.BytesIO(); image.save(data, "PNG")
        xml = b"<AnimData><Anims><Anim><Name>Idle</Name><FrameWidth>8</FrameWidth><FrameHeight>12</FrameHeight><Durations><Duration>6</Duration><Duration>12</Duration></Durations></Anim></Anims></AnimData>"
        frame, width, durations = pmd.animation_strip(xml, data.getvalue())
        self.assertEqual(frame.size, (16, 12)); self.assertEqual(width, 8); self.assertEqual(durations, [100, 200]); self.assertEqual(frame.getpixel((7, 11)), (255, 0, 0, 255))

    def test_walk_uses_distinct_right_and_left_source_rows(self):
        image = Image.new("RGBA", (16, 96))
        for row in range(8): image.paste((row * 30, 0, 0, 255), (0, row * 12, 16, (row + 1) * 12))
        data = io.BytesIO(); image.save(data, "PNG")
        xml = b"<AnimData><Anims><Anim><Name>Walk</Name><FrameWidth>8</FrameWidth><FrameHeight>12</FrameHeight><Durations><Duration>6</Duration><Duration>12</Duration></Durations></Anim></Anims></AnimData>"
        for direction in (2, 6):
            frame, _, durations = pmd.animation_strip(xml, data.getvalue(), "Walk", direction)
            self.assertEqual(frame.getpixel((7, 11)), (direction * 30, 0, 0, 255))
            self.assertEqual(durations, [100, 200])
        with self.assertRaises(ValueError): pmd.animation_strip(xml, data.getvalue(), "Walk", 8)


if __name__ == "__main__": unittest.main()
