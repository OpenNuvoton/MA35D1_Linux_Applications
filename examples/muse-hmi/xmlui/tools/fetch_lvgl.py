#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Fetch the exact upstream LVGL 9.4 source used by the shared XML renderer."""
from pathlib import Path
import subprocess
PIN='5a59ad4cf5de8717e2c841e5c0e402803e42ab8b'
DEST=Path(__file__).resolve().parents[1]/'vendor/lvgl'
if DEST.exists():
    actual=subprocess.check_output(['git','-C',str(DEST),'rev-parse','HEAD'],text=True).strip()
    if actual!=PIN:raise SystemExit('Existing LVGL checkout differs from the pinned revision; use a separate checkout.')
    subprocess.run(['git','-C',str(DEST),'diff','--exit-code'],check=True)
else:
    DEST.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(['git','clone','--filter=blob:none','--no-checkout','https://github.com/lvgl/lvgl.git',str(DEST)],check=True)
    subprocess.run(['git','-C',str(DEST),'fetch','--depth=1','origin',PIN],check=True)
    subprocess.run(['git','-C',str(DEST),'checkout','--detach',PIN],check=True)
print('LVGL 9.4, MIT license:',PIN)
