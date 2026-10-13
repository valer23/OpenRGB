"""Regression checks for animated keyboard underlays and assignment ownership."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('combine', Path(__file__).with_name('combine-keyboard-ripple.py'))
combine = importlib.util.module_from_spec(spec)
spec.loader.exec_module(combine)

class CompositionTests(unittest.TestCase):
    def build(self, kind=None, speed=100):
        zone = {'name': 'be quiet! Light Mount', 'zone_idx': 0}
        keyboard = {'EffectClassName': 'KeyRipple', 'ControllerZones': [zone],
                    'CustomSettings': {'highlight_ms': 100}, 'UserColors': [0xffffff, 0xff00a0], 'FPS': 30}
        effects = [] if kind is None else [{'EffectClassName': kind, 'ControllerZones': [zone, {'name': 'GPU'}],
                  'Speed': speed, 'FPS': 30, 'CustomSettings': {'schema': 1, 'layers': []}}]
        def profile(items):
            return {'controllers': [{'name': zone['name'], 'modes': [{'name': 'Direct'}]}],
                    'plugins': {'OpenRGB Effects Plugin': {'Effects': items}}}
        with tempfile.TemporaryDirectory() as directory:
            base, ripple = Path(directory)/'base.json', Path(directory)/'ripple.json'
            base.write_text(json.dumps(profile(effects)))
            ripple.write_text(json.dumps(profile([keyboard])))
            return combine.combine(base, ripple, 'Combined', red=True)['plugins']['OpenRGB Effects Plugin']['Effects']

    def test_static_base_remains_static(self):
        self.assertNotIn('base_effect', self.build()[-1]['CustomSettings'])

    def test_background_retains_speed_layers_and_exclusive_keyboard(self):
        for kind, speed in [('Brimstone', 100), ('IcueLayers', 140)]:
            with self.subTest(kind=kind):
                effects = self.build(kind, speed)
                self.assertEqual(effects[0]['ControllerZones'], [{'name': 'GPU'}])
                ripple = effects[-1]
                self.assertEqual(ripple['UserColors'][1], 0xff)
                background = ripple['CustomSettings']['base_effect']
                self.assertEqual(background['EffectClassName'], kind)
                self.assertEqual(background['Speed'], speed)
                self.assertEqual(background['CustomSettings']['schema'], 1)
                self.assertEqual(ripple['FPS'], 30)

if __name__ == '__main__':
    unittest.main()
