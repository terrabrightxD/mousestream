import importlib.util
from pathlib import Path
import unittest
import argparse

spec = importlib.util.spec_from_file_location('package', Path(__file__).parents[1] / 'scripts/package.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)

class HardwareIdTests(unittest.TestCase):
    def test_exact_hid_models(self):
        for value in [r'HID\VID_046D&PID_C077', r'HID\VID_1234&PID_ABCD&MI_00&COL01']:
            self.assertEqual(package.hardware_id(value.lower()), value)

    def test_broad_and_injected_ids_rejected(self):
        for value in ['*', r'HID_DEVICE_SYSTEM_MOUSE', r'USB\VID_046D&PID_C077',
                      'HID\\VID_1234&PID_5678\n[Injected]', r'HID\VID_1234&PID_5678,anything',
                      r'HID\VID_1234&PID_5678%bad%', '']:
            with self.assertRaises(argparse.ArgumentTypeError):
                package.hardware_id(value)

if __name__ == '__main__':
    unittest.main()
