"""Create a separate keyboard ripple profile from the saved device layout."""
import argparse
import copy
import json
import pathlib

def create(source):
    profile = copy.deepcopy(json.loads(pathlib.Path(source).read_text(encoding='utf-8-sig')))
    zones = []
    for controller in profile['controllers']:
        if controller['name'] != 'be quiet! Light Mount':
            continue
        for index, zone in enumerate(controller['zones']):
            matrix = zone.get('matrix_map', {})
            if zone['leds_count'] != 135 or matrix.get('width') != 84 or matrix.get('height') != 7:
                continue
            descriptor = {key:controller.get(key, '') for key in ('name','location','serial','description','version','vendor')}
            descriptor.update(zone_idx=index, reverse=False, self_brightness=100, is_segment=False, segment_idx=-1)
            zones.append(descriptor)
        controller['active_mode'] = next(i for i,mode in enumerate(controller['modes']) if mode['name']=='Direct')
    if not zones:
        raise ValueError('The supported Light Mount keyboard layout is absent')
    profile['profile_name'] = 'Keyboard Ripple'
    profile['plugins'] = {'OpenRGB Effects Plugin':{'version':2,'Effects':[{
        'EffectClassName':'KeyRipple','CustomName':'Keyboard Ripple','FPS':30,'Speed':100,
        'Slider2Val':0,'RandomColors':False,'AllowOnlyFirst':False,'Brightness':100,
        'Temperature':0,'Tint':0,'UserColors':[0xffffff,0xff00a0],
        'AutoStart':True,'ControllerZones':zones,'SelectAll':False,
        'CustomSettings':{'highlight_ms':100,'duration_ms':800,'width':1.8}}]}}
    return profile

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source');parser.add_argument('output')
    args=parser.parse_args()
    output=pathlib.Path(args.output)
    if output.exists():parser.error('Output exists; choose a new path')
    output.write_text(json.dumps(create(args.source),indent=2)+'\n',encoding='utf-8')
