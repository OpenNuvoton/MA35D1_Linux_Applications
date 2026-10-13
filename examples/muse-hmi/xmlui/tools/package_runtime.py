#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Collect the target Qt runtime, XML, brands, font and time-zone database."""
from pathlib import Path
import argparse,subprocess,re,shutil,json
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sysroot',type=Path,required=True,help='Sysroot from the same BSP SDK used to build the program')
p.add_argument('--readelf',default='aarch64-linux-gnu-readelf')
p.add_argument('--font',type=Path,default=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'))
p.add_argument('--zoneinfo',type=Path,default=Path('/usr/share/zoneinfo'))
p.add_argument('--output',type=Path,default=root/'build/runtime')
p.add_argument('--xml-dir',type=Path,default=root/'xml',help='Canonical XML or a generated theme directory')
a=p.parse_args();output=a.output
program=root/'build/qt-board/smart-home-overlay'
plugin=a.sysroot/'usr/lib/qt/plugins/platforms/libqoffscreen.so'
for path in [program,plugin,a.font,a.zoneinfo]:
    if not path.exists():p.error('Missing runtime input: '+str(path))
(output/'lib').mkdir(parents=True,exist_ok=True)
(output/'plugins/platforms').mkdir(parents=True,exist_ok=True)
shutil.copy2(program,output/program.name);shutil.copy2(plugin,output/'plugins/platforms'/plugin.name)
shutil.copytree(a.xml_dir,output/'xml',dirs_exist_ok=True)
shutil.copytree(root/'assets',output/'assets',dirs_exist_ok=True)
shutil.copy2(a.font,output/'assets/font.ttf')
copyright=Path('/usr/share/doc/fonts-dejavu-core/copyright')
if copyright.exists():shutil.copy2(copyright,output/'assets/FONT-LICENSE.txt')
shutil.copytree(a.zoneinfo,output/'system/zoneinfo',symlinks=True,dirs_exist_ok=True,
                ignore=shutil.ignore_patterns('localtime','posixrules'))
base={'libc.so.6','libm.so.6','libpthread.so.0','libdl.so.2','librt.so.1','ld-linux-aarch64.so.1'}
queue=[program,plugin];visited=set();manifest=[]
while queue:
    path=queue.pop();info=subprocess.check_output([a.readelf,'-d',str(path)],text=True)
    for name in re.findall(r'NEEDED.*\[(.*?)\]',info):
        if name in base or name in visited:continue
        visited.add(name)
        candidates=[a.sysroot/'usr/lib'/name,a.sysroot/'lib'/name]
        src=next((path for path in candidates if path.exists()),None)
        if src is None:raise RuntimeError('Missing target dependency '+name)
        shutil.copy2(src,output/'lib'/name);queue.append(src)
        manifest.append({'name':name,'bytes':src.stat().st_size})
(output/'runtime-libraries.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Packaged',program.name,'and',len(manifest),'target libraries at',output)
print('Include Qt, time-zone and BSP dependency licenses from your SDK with deployment.')
