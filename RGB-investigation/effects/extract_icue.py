"""Read lighting data only; never import iCUE actions, DPI, cooling or executable settings."""
import argparse
import collections
import hashlib
import json
import pathlib
import xml.etree.ElementTree as ET


def extract(path):
    content = pathlib.Path(path).read_bytes()
    if b"<!DOCTYPE" in content.upper() or b"<!ENTITY" in content.upper():
        raise ValueError("DTD/entity declarations are not lighting profile data")
    root = ET.fromstring(content)
    types = {}
    for element in root.iter():
        name = element.findtext("polymorphic_name")
        if name:
            types[int(element.findtext("polymorphic_id")) & 0x7fffffff] = name
    variants = collections.Counter()
    for element in root.iter():
        layer = element.find("ptr_wrapper/data")
        if layer is None:
            continue
        name = layer.findtext("base/name", "")
        if not name.startswith("Brimstone") or layer.findtext("base/enabled") != "true":
            continue
        lighting = layer.find("lighting")
        if lighting is None:
            continue
        data = lighting.find("ptr_wrapper/data")
        if data is None:
            continue  # A serialized reference; its definition is recorded separately.
        fields = {}
        for field in data.iter():
            if len(field) == 0 and field.tag in {
                "duration", "brightness", "opacity", "speed", "direction",
                "velocity", "tailLength", "angle", "isDoublesided", "random",
            }:
                fields[field.tag] = field.text
        colors = data.find("colors")
        if colors is not None:
            fields["colors_argb"] = [f"#{int(c.text):08x}" for c in colors]
        transitions = [{"time": float(v.findtext("time")), "argb": v.findtext("color")}
                       for container in data.iter("transitions") for v in container]
        spec = {"name": name, "type": types.get(int(lighting.findtext("polymorphic_id", "0")) & 0x7fffffff),
                "fields": fields, "transitions": transitions}
        variants[json.dumps(spec, sort_keys=True)] += 1
    layers = [dict(json.loads(spec), serialized_definitions=count)
              for spec, count in variants.most_common()]
    return {"profile": root.findtext("value0/profile/name"),
            "exporter_version": root.findtext("value0/profile/exporterVersion"),
            "sha256": hashlib.sha256(content).hexdigest(), "layers": layers,
            "notes": ["ARGB includes opacity; it is not OpenRGB's packed colour order.",
                      "Definition counts describe exported device variants, not active simultaneous layers.",
                      "iCUE rain randomness, enum timing and spatial coordinates need rendering adaptation."]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    parser.add_argument("output")
    args = parser.parse_args()
    pathlib.Path(args.output).write_text(json.dumps(extract(args.profile), indent=2) + "\n", encoding="utf-8")
