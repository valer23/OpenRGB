"""Translate an iCUE lighting stack into the local OpenRGB IcueLayers effect.

Lighting only. A conversion report makes layout and animation approximations explicit.
"""
import argparse
import collections
import copy
import hashlib
import json
import math
import pathlib
import re
import xml.etree.ElementTree as ET

SUPPORTED = {'StaticLighting', 'GradientLighting', 'ColorWaveLighting',
             'ColorPulseLighting', 'ColorShiftLighting', 'RainLighting', 'WaveLighting'}


def number(text, default=0):
    value = float(text) if text is not None else default
    if not math.isfinite(value):
        raise ValueError('Non-finite lighting value')
    return value


def rgba(value):
    packed = int(value[1:], 16) if value.startswith('#') else int(value)
    if not 0 <= packed <= 0xffffffff:
        raise ValueError('Colour outside ARGB range')
    return [(packed >> shift) & 255 for shift in (16, 8, 0, 24)]


def read_stacks(path):
    content = pathlib.Path(path).read_bytes()
    if len(content) > 32 * 1024 * 1024:
        raise ValueError('Profile exceeds 32 MiB')
    # Removing NUL also catches UTF-16/32 declarations before XML parsing.
    safe = content.replace(b'\0', b'').upper()
    if b'<!DOCTYPE' in safe or b'<!ENTITY' in safe:
        raise ValueError('DTD/entity declarations are unsupported')
    root = ET.fromstring(content)
    types = {}
    refs = {}
    for e in root.iter():
        if e.findtext('polymorphic_name'):
            types[int(e.findtext('polymorphic_id')) & 0x7fffffff] = e.findtext('polymorphic_name')
        if e.tag == 'ptr_wrapper' and e.find('data') is not None and e.findtext('id'):
            refs[int(e.findtext('id')) & 0x7fffffff] = e.find('data')

    def resolve(e):
        if e is None:
            return None
        data = e.find('ptr_wrapper/data')
        return data if data is not None else refs.get(int(e.findtext('ptr_wrapper/id', '0')) & 0x7fffffff)

    stacks = collections.Counter()
    for container in root.iter('layers'):
        stack = []
        for item in container:
            layer = resolve(item)
            if layer is None or layer.findtext('base/enabled') != 'true':
                continue
            lighting = layer.find('lighting')
            data = resolve(lighting)
            if data is None:
                continue
            kind = types.get(int(lighting.findtext('polymorphic_id', '0')) & 0x7fffffff, 'Unknown')
            fields = {e.tag: e.text for e in data.iter() if len(e) == 0 and e.tag in
                      {'duration', 'brightness', 'opacity', 'speed', 'direction', 'angle',
                       'velocity', 'tailLength', 'isDoublesided', 'random'}}
            stops = [{'time': number(v.findtext('time')), 'rgba': rgba(v.findtext('color'))}
                     for ts in data.iter('transitions') for v in ts]
            if any(not 0 <= s['time'] <= 1 for s in stops):
                raise ValueError('Transition time outside normalized range')
            colors = data.find('colors')
            keys = layer.find('base/keys')
            reactive = layer.findtext('base/executionHints/startOnKeyPress') == 'true'
            stack.append({'name': layer.findtext('base/name', ''), 'kind': kind,
                          'fields': fields, 'stops': sorted(stops, key=lambda s: s['time']),
                          'colors': [rgba(c.text) for c in colors] if colors is not None else [],
                          'keys': [k.text for k in keys] if keys is not None else [],
                          'reactive': reactive})
        if stack:
            stacks[json.dumps(stack, sort_keys=True)] += 1
    variants = [dict(layers=json.loads(s), occurrences=n) for s, n in stacks.most_common()]
    return {'profile': root.findtext('value0/profile/name', pathlib.Path(path).stem),
            'sha256': hashlib.sha256(content).hexdigest(), 'stacks': variants}


def compile_stack(stack):
    output, warnings = [], []
    # iCUE serializes its top layer first. Composite from the bottom up.
    for source in reversed(stack):
        kind, fields = source['kind'], source['fields']
        if source['reactive'] or kind not in SUPPORTED:
            warnings.append(f"Skipped {source['name']}: {'keypress-triggered' if source['reactive'] else kind}")
            continue
        if fields.get('random') == 'true':
            warnings.append(f"Skipped {source['name']}: random palette is not defined in XML")
            continue
        duration = number(fields.get('duration'), 1000) / 1000
        if not 0.01 <= duration <= 3600:
            raise ValueError('Lighting duration outside 0.01–3600 seconds')
        speed = number(fields.get('speed'), 2)
        # iCUE's speed enum isn't a documented duration. This is an explicit adaptation.
        if kind in {'ColorWaveLighting', 'ColorPulseLighting', 'ColorShiftLighting', 'RainLighting'}:
            duration = {1: 4.0, 2: 2.5, 3: 1.5}.get(speed, 2.5)
        direction = int(number(fields.get('direction'), 2))
        keys = source['keys']
        indices = [int(m.group(1)) - 1 for key in keys if (m := re.search(r'Led(\d+)$', key))]
        # Numeric LED masks are proportionally remapped, never interpreted as target hardware IDs.
        mask = sorted(set(indices)) if len(indices) == len(keys) and all(i >= 0 for i in indices) else []
        output.append({'kind': kind, 'name': source['name'], 'period': duration,
                       'gain': max(0, min(1, number(fields.get('brightness'), 10) / 10)),
                       'opacity': max(0, min(1, number(fields.get('opacity'), 255) / 255)),
                       'reverse': direction in (1, 3), 'vertical': direction in (3, 4),
                       'angle': number(fields.get('angle'), 0),
                       'stops': source['stops'], 'colors': source['colors'], 'mask': mask})
    if not output:
        raise ValueError('Selected stack has no supported lighting layers')
    # The common mask extent includes LEDs assigned to the background layer.
    offset = min((i for layer in output for i in layer['mask']), default=0)
    extent = max((i for layer in output for i in layer['mask']), default=offset - 1) - offset + 1
    for layer in output:
        layer['mask'] = [i - offset for i in layer['mask']]
        layer['mask_extent'] = extent
    return output, warnings


def convert(source, layout, stack_index=0, name=None):
    extracted = read_stacks(source)
    if not 0 <= stack_index < len(extracted['stacks']):
        raise ValueError('No such lighting stack; use --inspect to list stacks')
    selected = extracted['stacks'][stack_index]
    layers, warnings = compile_stack(selected['layers'])
    template = json.loads(pathlib.Path(layout).read_text(encoding='utf-8-sig'))
    profile = copy.deepcopy(template)
    name = name or extracted['profile']
    profile['profile_name'] = name
    zones = []
    for controller in profile['controllers']:
        modes = controller.get('modes', [])
        direct = next((i for i, mode in enumerate(modes) if mode.get('name') == 'Direct'), None)
        # These local drivers intentionally provide streaming through their first mode.
        local = controller['name'].startswith(('Lian Li SL Infinity', 'KFA2 GeForce RTX 3080 Ti', 'ASUS ROG STRIX B850-F'))
        if direct is None and not local:
            warnings.append(f"Device omitted: {controller['name']} has no Direct mode")
            continue
        controller['active_mode'] = direct if direct is not None else 0
        for index, zone in enumerate(controller['zones']):
            if not zone['leds_count']:
                continue
            descriptor = {key: controller.get(key, '') for key in
                          ('name', 'location', 'serial', 'description', 'version', 'vendor')}
            descriptor.update(zone_idx=index, reverse=False, self_brightness=100,
                              is_segment=False, segment_idx=-1)
            zones.append(descriptor)
    if not zones:
        raise ValueError('Layout contains no supported output zones')
    effect = {'EffectClassName': 'IcueLayers', 'CustomName': name, 'FPS': 30, 'Speed': 100,
              'Slider2Val': 0, 'RandomColors': False, 'AllowOnlyFirst': False,
              'Brightness': 100, 'Temperature': 0, 'Tint': 0, 'UserColors': [],
              'AutoStart': True, 'ControllerZones': zones, 'SelectAll': False,
              'CustomSettings': {'schema': 1, 'layers': layers}}
    profile['plugins'] = {'OpenRGB Effects Plugin': {'version': 2, 'Effects': [effect]}}
    report = {'profile': name, 'source_sha256': extracted['sha256'], 'selected_stack': stack_index,
              'source_stack_occurrences': selected['occurrences'], 'target_zones': len(zones),
              'layers': [l['name'] for l in layers], 'warnings': warnings,
              'approximations': ['Uses one selected source device stack across the target zones.',
                  'Numeric source LED masks scale to each target zone; other masks use the whole zone.',
                  'Static/gradient ARGB and normalized transition times are retained.',
                  'Wave geometry, speed enums and rain randomness are adapted, not exact iCUE playback.',
                  'Keyboard-specific artwork and keypress effects are not imported.'],
              'available_stacks': [{'index': i, 'occurrences': s['occurrences'],
                                    'layers': [l['name'] for l in s['layers']]}
                                   for i, s in enumerate(extracted['stacks'])]}
    return profile, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=pathlib.Path)
    parser.add_argument('--layout', type=pathlib.Path)
    parser.add_argument('--output', type=pathlib.Path)
    parser.add_argument('--name')
    parser.add_argument('--stack', type=int, default=0)
    parser.add_argument('--inspect', action='store_true')
    args = parser.parse_args()
    if args.inspect:
        print(json.dumps(read_stacks(args.input), indent=2))
        return
    if not args.layout or not args.output:
        parser.error('--layout and --output are required for conversion')
    profile, report = convert(args.input, args.layout, args.stack, args.name)
    if args.output.exists():
        parser.error('Output already exists; choose a new path')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(profile, indent=2) + '\n', encoding='utf-8')
    # Avoid ".json" in report filenames: OpenRGB treats these as profiles too.
    args.output.with_suffix('.conversion-report.txt').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f"Created {args.output}: {len(report['layers'])} layers, {report['target_zones']} zones")


if __name__ == '__main__':
    main()
