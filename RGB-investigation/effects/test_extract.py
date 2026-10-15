import pathlib
import unittest
from unittest.mock import patch
from extract_icue import extract

PROFILE = pathlib.Path(r"C:\Users\valerian\Downloads\LG_Brimstone_lGoYlCr.cueprofile")


class LightingTests(unittest.TestCase):
    def test_real_lighting_values(self):
        result = extract(PROFILE)
        self.assertEqual(result["profile"], "Brimstone")
        self.assertEqual(result["exporter_version"], "5.9.105")
        background = next(x for x in result["layers"] if x["name"] == "BrimstoneBG")
        self.assertEqual(background["transitions"][0]["argb"], "#ff461800")
        rain = next(x for x in result["layers"] if x["name"] == "BrimstoneRain 1")
        self.assertEqual(rain["fields"]["colors_argb"], ["#ffff3800", "#ff0a0300"])
        text = str(result)
        self.assertNotIn("pointerMotionSpeed", text)
        self.assertNotIn("linkedProgramsPaths", text)

    def test_dtd_rejected(self):
        with patch.object(pathlib.Path, "read_bytes", return_value=b'<!DOCTYPE cereal [<!ENTITY x SYSTEM "file:///secret">]><cereal/>'):
            with self.assertRaises(ValueError):
                extract("profile.xml")


if __name__ == "__main__":
    unittest.main()
