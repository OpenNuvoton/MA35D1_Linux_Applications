#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Generate a light Daybreak palette in the same shared Qt/LVGL XML tree.

Coordinates, bindings, actions and screen content are preserved. This produces
static XML for review and deployment, not arbitrary executable Muse content.
"""
from pathlib import Path
import argparse,shutil
from xml.etree import ElementTree as E

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--source-dir',type=Path,default=root/'xml')
p.add_argument('--output-dir',type=Path,default=root/'themes/daybreak')
a=p.parse_args()
if a.source_dir.resolve()==a.output_dir.resolve():p.error('Keep the source templates; choose a separate output directory')
palette={
    '0x0c102b':'0xedf6fa','0x202c52':'0xffffff',
    '0x192443':'0x29475e','0x635bff':'0x075fdb',
    '0x263858':'0x23576d','0x373b77':'0x1269b3',
    '0xf3f6ff':'0x173a50','0xf4f6ff':'0x173a50',
    '0xa7b8dd':'0x546d82','0xa3b4dc':'0x506b7d',
    '0x899dca':'0x52768b','0x839ac7':'0x4f6d82',
    '0x59e3d1':'0x08777e','0x5ae4ca':'0x08777e',
    '0xffc362':'0xa35f07','0xcc93ff':'0x753eb7',
    '0x66e3cf':'0x007f89','0x263553':'0xc5dbe5',
    '0x178f87':'0x00766f','0xa354a0':'0x87378e',
}
files=sorted(a.source_dir.glob('*.xml'))
if len(files)!=11:p.error('Expected ten screens and components.xml')
a.output_dir.mkdir(parents=True,exist_ok=True)
changes=0
for source in files:
    tree=E.parse(source)
    for node in tree.iter():
        for key,value in list(node.attrib.items()):
            if value.lower() in palette:
                node.set(key,palette[value.lower()]);changes+=1
    E.indent(tree);tree.write(a.output_dir/source.name,encoding='unicode',xml_declaration=False)
shutil.copy2(a.source_dir/'screen.xsd',a.output_dir/'screen.xsd')
print(f'Daybreak: {len(files)-1} shared screens, {changes} color bindings; {a.output_dir}')
