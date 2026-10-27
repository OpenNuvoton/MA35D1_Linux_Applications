#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build LVGL into isolated objects, never replacing another demo's LVGL build."""
from pathlib import Path
import argparse, concurrent.futures, subprocess, os
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--arch',choices=['host','board'],default='host');p.add_argument('--cc');p.add_argument('--jobs',type=int,default=4);p.add_argument('--lvgl-dir',type=Path,default=root/'vendor/lvgl');a=p.parse_args()
cc=a.cc or ('gcc' if a.arch=='host' else os.environ.get('CROSS_COMPILE','aarch64-linux-gnu-')+'gcc')
folder=root/'build'/a.arch;folder.mkdir(parents=True,exist_ok=True)
source_root=a.lvgl_dir.resolve()
if not (source_root/'src').is_dir():p.error('LVGL source missing; run tools/fetch_lvgl.py or pass --lvgl-dir')
flags=['-O2','-fPIC','-I'+str(source_root),'-I'+str(root),'-DLV_CONF_PATH="'+str(root/'lv_conf.h')+'"']
def compile(src):
 target=folder/(str(src.relative_to(source_root)).replace('/','_')+'.o')
 if not target.exists() or target.stat().st_mtime<max(src.stat().st_mtime,(root/'lv_conf.h').stat().st_mtime):
  subprocess.run([cc,*flags,'-c',str(src),'-o',str(target)],check=True,stdout=subprocess.DEVNULL)
 return target
sources=sorted((source_root/'src').rglob('*.c'))
# Only the three actual Expat translation units should be compiled.
sources=[s for s in sources if s.name not in ['xmltok_impl.c','xmltok_ns.c']]
with concurrent.futures.ThreadPoolExecutor(a.jobs) as pool: objects=list(pool.map(compile,sources))
subprocess.run(['ar','rcs',str(folder/'liblvgl.a'),*[str(o) for o in objects]],check=True)
print(f'{a.arch}: built {len(objects)} LVGL source objects')
