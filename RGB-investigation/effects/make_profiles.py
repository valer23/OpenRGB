"""Create effect profiles from saved OpenRGB controller layouts, without modifying the source profile."""
import argparse
import copy
import json
import pathlib


def generate(source):
    template = json.loads(pathlib.Path(source).read_text(encoding="utf-8-sig"))
    zones = []
    for controller in template["controllers"]:
        for index, zone in enumerate(controller["zones"]):
            if not zone["leds_count"]:
                continue
            descriptor = {key: controller.get(key, "") for key in
                          ("name", "location", "serial", "description", "version", "vendor")}
            descriptor.update(zone_idx=index, reverse=False, self_brightness=100,
                              is_segment=False, segment_idx=0)
            zones.append(descriptor)
    profiles = {}
    for name, effect, speed, frequency in [("Rainbow Wave", "RainbowWave", 4, 10),
                                            ("Brimstone", "Brimstone", 100, 0)]:
        profile = copy.deepcopy(template)
        profile["profile_name"] = name
        for controller in profile["controllers"]:
            controller["active_mode"] = 0
        profile["plugins"] = {"OpenRGB Effects Plugin": {"version": 2, "Effects": [{
            "EffectClassName": effect, "CustomName": name, "FPS": 10,
            "Speed": speed, "Slider2Val": frequency, "RandomColors": False,
            "AllowOnlyFirst": False, "Brightness": 100, "Temperature": 0, "Tint": 0,
            "UserColors": [], "AutoStart": True, "ControllerZones": zones, "SelectAll": False,
        }]}}
        profiles[name] = profile
    return profiles


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source")
    parser.add_argument("output")
    args = parser.parse_args()
    output = pathlib.Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    for name, profile in generate(args.source).items():
        (output / (name + ".json")).write_text(json.dumps(profile, indent=2) + "\n", encoding="utf-8")
