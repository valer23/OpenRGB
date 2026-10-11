"""Regression checks for failed restoration; no hardware access."""
import unittest
from unittest.mock import Mock, patch
from kfa2_nvapi import GPU, NVAPIError


class RestorationTests(unittest.TestCase):
    def setUp(self):
        self.delay=patch('kfa2_nvapi.time.sleep').start()
        self.addCleanup(patch.stopall)
        self.gpu=GPU.__new__(GPU)
        self.gpu.write=Mock()
        self.gpu.read=Mock(return_value=b'\x01')

    def test_generic_error_then_success_requires_readback(self):
        self.gpu.write.side_effect=[NVAPIError(-1,'sync'),None]
        self.gpu.set_sync(b'\x01')
        self.assertEqual(self.gpu.write.call_count,2)
        self.gpu.read.assert_called_once_with(7)

    def test_retries_are_bounded(self):
        self.gpu.write.side_effect=NVAPIError(-1,'sync')
        with self.assertRaises(NVAPIError):self.gpu.set_sync(b'\x01')
        self.assertEqual(self.gpu.write.call_count,3)

    def test_success_status_with_wrong_readback_is_not_success(self):
        self.gpu.read.return_value=b'\x00'
        with self.assertRaisesRegex(RuntimeError,'readback'):self.gpu.set_sync(b'\x01')

    def test_sync_restore_still_attempted_after_colour_failure(self):
        self.gpu.write.side_effect=[NVAPIError(-1,'colour'),None,None,None]
        self.gpu.set_sync=Mock()
        snapshot={'rgb':b'\xff\x00\x00','mode':b'\x00','mode2':b'\x20','sync':b'\x01'}
        with self.assertRaisesRegex(RuntimeError,'restoration incomplete'):self.gpu.restore(snapshot)
        self.gpu.set_sync.assert_called_once_with(b'\x01')
        self.assertEqual(self.gpu.write.call_count,4)


if __name__=='__main__':unittest.main()
