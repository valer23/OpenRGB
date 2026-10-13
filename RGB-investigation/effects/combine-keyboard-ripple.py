"""Combine a saved base profile with a keyboard-only ripple effect."""
import argparse
import copy
import json
import pathlib
import re


def filename(name):
    # Match OpenRGB StringUtils::make_filename, including stripping '+'.
    name = re.sub(r"[#%&{}\\<>*?/!`';@+|=]", '', name.replace(':', '-'))
    return re.sub(r'[\x00-\x1f\x7f]', '', name.lstrip('.')) + '.json'


def combine(base, ripple, name, red=False):
    profile = copy.deepcopy(json.loads(pathlib.Path(base).read_text(encoding='utf-8-sig')))
    source = json.loads(pathlib.Path(ripple).read_text(encoding='utf-8-sig'))
    keyboard = copy.deepcopy(next(e for e in source['plugins']['OpenRGB Effects Plugin']['Effects']
                                  if e['EffectClassName'] == 'KeyRipple'))
    if not keyboard['ControllerZones'] or any(z['name'] != 'be quiet! Light Mount' for z in keyboard['ControllerZones']):
        raise ValueError('Ripple contains unexpected device assignments')
    effects = profile.get('plugins', {}).get('OpenRGB Effects Plugin', {}).get('Effects', [])
    effects = [e for e in effects if e['EffectClassName'] != 'KeyRipple']
    keyboard['CustomSettings'].pop('base_effect', None)
    backgrounds = [e for e in effects if e['EffectClassName'] in ('Brimstone', 'IcueLayers')
                   and any(z['name'] == 'be quiet! Light Mount' for z in e['ControllerZones'])]
    if len(backgrounds) > 1:
        raise ValueError('Multiple keyboard background effects cannot be composed')
    if backgrounds:
        base_effect = backgrounds[0]
        keyboard['CustomSettings']['base_effect'] = {
            'EffectClassName': base_effect['EffectClassName'],
            'Speed': base_effect['Speed'],
            'CustomSettings': copy.deepcopy(base_effect.get('CustomSettings', {})),
        }
        keyboard['FPS'] = base_effect['FPS']
    for effect in effects:
        effect['ControllerZones'] = [z for z in effect['ControllerZones'] if z['name'] != 'be quiet! Light Mount']
    if red:
        keyboard['CustomName'] = 'Red Keyboard Ripple'
        keyboard['UserColors'] = [0xffffff, 0x0000ff]  # OpenRGB packs R in the low byte.
    effects.append(keyboard)
    for controller in profile['controllers']:
        if controller['name'] == 'be quiet! Light Mount':
            controller['active_mode'] = next(i for i, mode in enumerate(controller['modes']) if mode['name'] == 'Direct')
    profile['profile_name'] = name
    profile['plugins'] = {'OpenRGB Effects Plugin': {'version': 2, 'Effects': effects}}
    return profile


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('base', type=pathlib.Path)
    parser.add_argument('ripple', type=pathlib.Path)
    parser.add_argument('output_directory', type=pathlib.Path)
    parser.add_argument('--name', required=True)
    parser.add_argument('--red', action='store_true')
    args = parser.parse_args()
    profile = combine(args.base, args.ripple, args.name, args.red)
    output = args.output_directory / filename(args.name)
    if output.exists():
        parser.error('Output already exists; choose a new name or directory')
    args.output_directory.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(profile, indent=2) + '\n', encoding='utf-8')
    print(output)
