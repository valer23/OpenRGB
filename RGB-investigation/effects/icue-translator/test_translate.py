import copy
import json
import pathlib
import tempfile
import unittest
from unittest.mock import patch
from translate_icue import compile_stack, convert, read_stacks, rgba


class TranslationTests(unittest.TestCase):
    def test_argb(self):
        self.assertEqual(rgba('#80130538'), [19, 5, 56, 128])
        self.assertEqual(rgba('4283914071'), [87, 87, 87, 255])

    def test_entity_rejected_utf16(self):
        for encoding in ['utf-8', 'utf-16', 'utf-32']:
            with patch.object(pathlib.Path, 'read_bytes', return_value='<!DOCTYPE x [<!ENTITY x SYSTEM "file:///test">]><x/>'.encode(encoding)):
                with self.assertRaises(ValueError): read_stacks('unused')

    def test_serialized_references_and_disabled(self):
        xml = b'''<cereal><value0><profile><name>Test</name></profile></value0>
        <definition><polymorphic_id>2147483653</polymorphic_id><polymorphic_name>StaticLighting</polymorphic_name>
        <ptr_wrapper><id>2147483668</id><data><base><duration>1000</duration><transitions><v><time>0</time><color>#ff010203</color></v></transitions></base></data></ptr_wrapper></definition>
        <layers><v><ptr_wrapper><id>2147483670</id><data><base><name>A</name><enabled>true</enabled></base><lighting><polymorphic_id>5</polymorphic_id><ptr_wrapper><id>20</id></ptr_wrapper></lighting></data></ptr_wrapper></v></layers>
        <layers><v><ptr_wrapper><id>22</id></ptr_wrapper></v></layers></cereal>'''
        with patch.object(pathlib.Path, 'read_bytes', return_value=xml):
            result = read_stacks('unused')
        self.assertEqual(result['stacks'][0]['occurrences'], 2)
        self.assertEqual(result['stacks'][0]['layers'][0]['stops'][0]['rgba'], [1, 2, 3, 255])

    def test_compilation_order_mask_offset_and_reactive(self):
        layer = dict(name='BG', kind='StaticLighting', fields={}, stops=[], colors=[], keys=['Led_Cmd_Led300', 'Led_Cmd_Led301'], reactive=False)
        top = dict(layer, name='Top', keys=['Led_Cmd_Led301'])
        reactive = dict(layer, name='Trigger', reactive=True)
        result, warnings = compile_stack([reactive, top, layer])
        self.assertEqual([l['name'] for l in result], ['BG', 'Top'])
        self.assertEqual(result[0]['mask'], [0, 1])
        self.assertEqual(result[1]['mask'], [1])
        self.assertEqual(result[1]['mask_extent'], 2)
        self.assertIn('keypress', warnings[0])

    def test_layout_preserved_and_unknown_devices_omitted(self):
        layer = dict(name='BG', kind='StaticLighting', fields={}, stops=[], colors=[], keys=[], reactive=False)
        extracted = dict(profile='Example', sha256='test', stacks=[dict(layers=[layer], occurrences=1)])
        controller = dict(name='Test', modes=[dict(name='Static'), dict(name='Direct')], active_mode=0, zones=[dict(leds_count=2), dict(leds_count=0)])
        layout = dict(controllers=[controller, dict(controller, name='Unsupported', modes=[])])
        with tempfile.TemporaryDirectory(dir=pathlib.Path(__file__).parent) as directory:
            p = pathlib.Path(directory)/'layout.json';p.write_text(json.dumps(layout));before = p.read_bytes()
            with patch('translate_icue.read_stacks', return_value=extracted):
                profile, report = convert('unused', p)
            self.assertEqual(p.read_bytes(), before)
        self.assertEqual(profile['controllers'][0]['active_mode'], 1)
        effect = profile['plugins']['OpenRGB Effects Plugin']['Effects'][0]
        self.assertEqual(len(effect['ControllerZones']), 1)
        self.assertEqual(effect['ControllerZones'][0]['segment_idx'], -1)
        self.assertTrue(report['warnings'])


if __name__ == '__main__': unittest.main()
