#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Install the optional XML overlay on an already mounted NAND volume."""
from pathlib import Path
import argparse,subprocess,tarfile,tempfile,shlex
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--ssh-target',required=True);p.add_argument('--ssh-port',default='22')
p.add_argument('--ssh-host-alias');p.add_argument('--runtime',type=Path,default=root/'build/runtime')
p.add_argument('--daily-reports',action='store_true');p.add_argument('--dry-run',action='store_true')
a=p.parse_args();runtime=a.runtime.resolve()
for name in ['smart-home-overlay','xml','assets','plugins','lib','system/zoneinfo']:
    if not (runtime/name).exists():p.error('Missing packaged input: '+name)
if a.dry_run:
    print(runtime,'-> /mnt/nand/apps/smart-home-overlay; existing pairing/media/preferences preserved')
    print('zoneinfo -> /mnt/nand/system/zoneinfo; units -> /etc/systemd/system')
    raise SystemExit(0)
ssh=['ssh','-p',a.ssh_port,'-o','BatchMode=yes','-o','ConnectTimeout=5']
if a.ssh_host_alias:ssh+=['-o','HostKeyAlias='+a.ssh_host_alias]
ssh.append(a.ssh_target)
def remote(command,data=None):
    r=subprocess.run(ssh+[command],input=data,capture_output=True,timeout=180)
    if r.returncode:raise RuntimeError(r.stderr.decode(errors='replace'))
    return r.stdout.decode(errors='replace')
remote("grep -qs ' /mnt/nand ' /proc/mounts && test -x /mnt/sd/muse/media/muse-media && test -x /mnt/sd/muse/gateway/muse-gateway")
with tempfile.TemporaryDirectory() as temp:
    archive=Path(temp)/'runtime.tar'
    with tarfile.open(archive,'w') as tar:
        for name in ['smart-home-overlay','xml','assets','plugins','lib','runtime-libraries.json']:
            tar.add(runtime/name,arcname=name)
    remote('cat > /mnt/nand/winuvo-runtime.tar.new',archive.read_bytes())
    remote('systemctl stop smart-home-overlay.service; mkdir -p /mnt/nand/apps/smart-home-overlay; tar -xf /mnt/nand/winuvo-runtime.tar.new -C /mnt/nand/apps/smart-home-overlay; chmod 755 /mnt/nand/apps/smart-home-overlay/smart-home-overlay')
    zones=Path(temp)/'zones.tar'
    with tarfile.open(zones,'w') as tar:tar.add(runtime/'system/zoneinfo',arcname='zoneinfo')
    remote('cat > /mnt/nand/winuvo-zones.tar.new',zones.read_bytes())
    remote('mkdir -p /mnt/nand/system; tar -xf /mnt/nand/winuvo-zones.tar.new -C /mnt/nand/system; mkdir -p /usr/share/zoneinfo')
for name in ['smart-home-overlay.service','winuvo-timezones.service','winuvo-daily-report.service','winuvo-daily-report.timer']:
    remote('cat > '+shlex.quote('/etc/systemd/system/'+name+'.new'),(root/name).read_bytes())
    remote('mv '+shlex.quote('/etc/systemd/system/'+name+'.new')+' '+shlex.quote('/etc/systemd/system/'+name))
remote('systemctl daemon-reload; systemctl enable --now winuvo-timezones.service smart-home-overlay.service')
if a.daily_reports:remote('systemctl enable --now winuvo-daily-report.timer')
print(remote('systemctl is-active smart-home-overlay.service winuvo-timezones.service'))
