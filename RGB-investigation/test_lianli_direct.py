"""Verify sync-off ordering and lighting-only writes without USB access."""
import ctypes
import unittest
from contextlib import ExitStack
from types import SimpleNamespace
from unittest.mock import patch
import lianli_direct as target

class LightingTests(unittest.TestCase):
    def run_test(self, args):
        sent = []
        def capture(feature):
            def call(handle, data, length):
                sent.append((feature, ctypes.string_at(data, length)))
                return True
            return call
        commits = {i: bytes([0xe0, 0x10 + i, 1, 0, 0, 0, 0]) for i in range(8)}
        with ExitStack() as stack:
            stack.enter_context(patch('sys.argv', ['test', *args]))
            stack.enter_context(patch.object(target, 'validate_capture', return_value=commits))
            stack.enter_context(patch.object(target.hid, 'device_paths', return_value=['fake']))
            stack.enter_context(patch.object(target.hid, 'open_device', return_value=123))
            stack.enter_context(patch.object(target.hid, 'capabilities', return_value=SimpleNamespace(page=0xff72,usage=0xa1,output=353,feature=7)))
            stack.enter_context(patch.object(target.hid.kernel, 'CloseHandle'))
            stack.enter_context(patch.object(target.hid.hid, 'HidD_SetFeature', side_effect=capture(True)))
            stack.enter_context(patch.object(target.hid.hid, 'HidD_SetOutputReport', side_effect=capture(False)))
            stack.enter_context(patch.object(target.time, 'sleep'))
            target.main()
        return sent

    def test_readonly_never_switches_sync(self):
        self.assertEqual(self.run_test(['--disable-sync']), [])

    def test_sync_off_precedes_only_lighting_commands(self):
        sent = self.run_test(['--replay', '--disable-sync', '--fans', '3,2,1,0'])
        self.assertEqual(sent[0], (True, bytes.fromhex('e0106100000000')))
        self.assertEqual(len(sent), 61)
        for feature, data in sent:
            self.assertEqual(len(data), 7 if feature else 353)
            self.assertNotIn(data[1], range(0x20,0x24))
            self.assertNotEqual(data[:3], bytes.fromhex('e01062'))
        self.assertEqual(sent[1][1], bytes.fromhex('e0106001030000'))
        self.assertEqual(sent[2][1][:5], bytes.fromhex('e030ff0000'))

if __name__ == '__main__': unittest.main()