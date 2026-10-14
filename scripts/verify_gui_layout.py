#!/usr/bin/env python3
"""Verify the released V3 atlas GUI, not the retired procedural generator.

The checked-in uidesc files are authoritative for this hardware composition.
Re-running generate_gui.py would remove the atlas resources and change axes.
"""
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
files = [ROOT/'resource'/name for name in ('mixengine.uidesc','mixengine_channel.uidesc')]
subprocess.run([sys.executable, str(ROOT/'scripts/check_gui_layout.py'), *map(str, files)], check=True)
required = {'MixKnobVernierAtlas', 'MixKnobGunmetalSAtlas', 'MixKnobGunmetalMAtlas',
            'MixPush', 'MixVUFace', 'MixVUOverlay'}
for path in files:
    root = ET.parse(path).getroot()
    bitmaps = {b.attrib['name']: b for b in root.findall('./bitmaps/bitmap')}
    missing = required - bitmaps.keys()
    if missing:
        raise SystemExit(f'{path.name}: missing released atlas bindings: {sorted(missing)}')
    for name in required:
        bitmap = bitmaps[name]
        if not bitmap.attrib.get('path', '').endswith('.png'):
            raise SystemExit(f'{path.name}: invalid bitmap path: {name}')
        high = bitmaps.get(name+'#3.0x')
        if high is None or high.attrib.get('scale-factor') != '3':
            raise SystemExit(f'{path.name}: missing 300% bitmap: {name}')
controls = (ROOT/'source/HardwareControls.cpp').read_text(encoding='utf-8')
if 'constexpr double factors[] {0.75,0.90,1.0,1.25,1.5};' not in controls or 'editor_->setZoomFactor(factors[(index+1)%5])' not in controls:
    raise SystemExit('Released 75/90/100/125/150% zoom sequence is missing')
controller = (ROOT/'source/controller.cpp').read_text(encoding='utf-8')
if '"MIX ENGINE V3"' not in controller or '"MIX ENGINE V2"' in controller:
    raise SystemExit('V3 editor identity is incorrect')
print('Released V3 GUI contracts PASS: atlas resources, geometry, identity, five zoom factors; native interaction remains separate')
