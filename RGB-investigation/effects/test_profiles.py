import json
import pathlib
import unittest
from make_profiles import generate


class ProfilesTests(unittest.TestCase):
    def test_source_unchanged_and_all_populated_zones_assigned(self):
        source = pathlib.Path(r"C:\Users\valerian\Apps\OpenRGB-Custom\Settings\profiles\ValWhite.json")
        before = source.read_bytes()
        original = json.loads(before.decode("utf-8-sig"))
        profiles = generate(source)
        self.assertEqual(source.read_bytes(), before)
        for name, profile in profiles.items():
            self.assertEqual(profile["profile_name"], name)
            self.assertEqual(profile["profile_version"], original["profile_version"])
            self.assertEqual(len(profile["controllers"]), len(original["controllers"]))
            self.assertTrue(all(c["active_mode"] == 0 for c in profile["controllers"]))
            effect = profile["plugins"]["OpenRGB Effects Plugin"]["Effects"][0]
            self.assertEqual(effect["FPS"], 30)
            self.assertTrue(effect["AutoStart"])
            self.assertTrue(all(z["segment_idx"] == -1 for z in effect["ControllerZones"]))
            expected = sum(bool(z["leds_count"]) for c in original["controllers"] for z in c["zones"])
            self.assertEqual(len(effect["ControllerZones"]), expected)
            self.assertEqual(len({(z["location"], z["zone_idx"]) for z in effect["ControllerZones"]}), expected)


if __name__ == "__main__":
    unittest.main()
