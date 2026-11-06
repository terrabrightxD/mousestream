#!/usr/bin/env python3
"""Create an unsigned INF/SYS staging directory. Does not install anything."""
import argparse
from pathlib import Path
import re
import shutil


def hardware_id(value):
    # A concrete HID VID/PID model only. Never accept broad HID_DEVICE_SYSTEM_MOUSE.
    if not re.fullmatch(r'HID\\VID_[0-9A-F]{4}&PID_[0-9A-F]{4}(?:&[A-Z0-9_]+)*', value, re.I):
        raise argparse.ArgumentTypeError('Use an exact HID\\VID_xxxx&PID_xxxx... hardware ID from a Mouse-class device.')
    return value.upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hardware-id', required=True, type=hardware_id)
    parser.add_argument('--sys', required=True, type=Path)
    parser.add_argument('--out', type=Path, default=Path('build/package'))
    args = parser.parse_args()
    if not args.sys.is_file():
        parser.error('Build the driver first: --sys must point to MouseStream.sys')
    args.out.mkdir(parents=True, exist_ok=True)
    template = Path(__file__).resolve().parents[1] / 'driver' / 'MouseStream.inf.in'
    content = template.read_text().replace('@HARDWARE_ID@', args.hardware_id)
    (args.out / 'MouseStream.inf').write_text(content, encoding='ascii')
    shutil.copyfile(args.sys, args.out / 'MouseStream.sys')
    print(f'Staged unsigned package in {args.out.resolve()} for {args.hardware_id}')
    print('Next: InfVerif, Inf2Cat, signing, and isolated Windows testing. Nothing was installed.')


if __name__ == '__main__':
    main()
