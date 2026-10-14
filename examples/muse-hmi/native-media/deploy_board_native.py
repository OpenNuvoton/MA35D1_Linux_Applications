# SPDX-License-Identifier: Apache-2.0
"""Install the SD-backed five-tab runtime; preserve existing board pairing."""
import argparse
import json
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ssh-target', required=True, help='SSH board account@host')
    parser.add_argument('--ssh-port', default='22')
    parser.add_argument('--ssh-host-alias', help='HostKeyAlias for an existing relay')
    parser.add_argument('--backend-dir', type=Path,
                        help='Upload curl, ffmpeg, mutool, ca-certificates.crt and font.ttf')
    parser.add_argument('--pairing-state', type=Path,
                        help='Explicitly migrate identity.json, pairing.json and optional sdk_token')
    parser.add_argument('--enable-swap', action='store_true',
                        help='Use an already initialized /mnt/sd/muse/swapfile; never creates one')
    parser.add_argument('--dry-run', action='store_true', help='List files without connecting')
    args = parser.parse_args()
    sys.path.insert(0, str(ROOT.parent))
    from media_api import MEDIA_SPECS

    specs = dict(MEDIA_SPECS)
    specs['media.switch'] = {
        'description': 'Restore cached text, image, pdf, video or audio tab and view state.',
        'required': {'kind': {'type': 'string', 'description': 'text, image, pdf, video or audio'}},
        'optional': {}, 'timeout_ms': 10000,
    }
    specs['device.health'] = {'description': 'Read native MA35 gateway health.',
                              'required': {}, 'optional': {}}
    files = [(ROOT / 'build/muse-gateway', '/mnt/sd/muse/gateway/muse-gateway', 0o755)]
    files += [(ROOT / ('build/' + name), '/mnt/sd/muse/media/' + name, 0o755)
              for name in ('muse-media', 'quick-present', 'youtube-resolve')]
    files += [(ROOT / 'speak', '/mnt/sd/muse/tts/speak', 0o755)]
    files += [(ROOT / name, '/etc/systemd/system/' + name, 0o644)
              for name in ('muse-media.service', 'muse-gateway.service')]
    if args.enable_swap:
        files.append((ROOT / 'muse-swap.service', '/etc/systemd/system/muse-swap.service', 0o644))
    if args.backend_dir:
        files += [(args.backend_dir / name, '/mnt/sd/muse/media/' + name,
                   0o755 if name in ('curl', 'ffmpeg', 'mutool') else 0o644)
                  for name in ('curl', 'ffmpeg', 'mutool', 'ca-certificates.crt', 'font.ttf')]
    if args.pairing_state:
        files += [(args.pairing_state / name, '/mnt/sd/muse/state/' + name, 0o600)
                  for name in ('identity.json', 'pairing.json')]
        if (args.pairing_state / 'sdk_token').exists():
            files.append((args.pairing_state / 'sdk_token', '/mnt/sd/muse/state/sdk_token', 0o600))
    for local, remote, mode in files:
        if not local.is_file():
            parser.error(f'Missing install input: {local}')
        print(f'{local.relative_to(ROOT) if local.is_relative_to(ROOT) else local.name} -> {remote} ({mode:o})')
    if args.dry_run:
        print('commands.json is generated from the packaged GenUI and media schemas.')
        return
    ssh = ['ssh', '-p', args.ssh_port, '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=5']
    if args.ssh_host_alias:
        ssh += ['-o', 'HostKeyAlias=' + args.ssh_host_alias]
    ssh += [args.ssh_target]

    def run(command, data=None):
        result = subprocess.run(ssh + [command], input=data, capture_output=True, timeout=120)
        if result.returncode:
            raise RuntimeError(result.stderr.decode(errors='replace'))
        return result.stdout.decode(errors='replace')

    run("grep -qs ' /mnt/sd ' /proc/mounts || { echo '/mnt/sd must already be mounted' >&2; exit 1; }")
    run('mkdir -p /mnt/sd/muse/gateway /mnt/sd/muse/media/assets /mnt/sd/muse/media-cache /mnt/sd/muse/tts /mnt/sd/muse/state; chmod 700 /mnt/sd/muse/state')
    if args.enable_swap:
        run('test -f /mnt/sd/muse/swapfile')
    if not args.pairing_state:
        run('test -r /mnt/sd/muse/state/pairing.json && test -r /mnt/sd/muse/state/identity.json')
    if not args.backend_dir:
        run('test -x /mnt/sd/muse/media/curl && test -x /mnt/sd/muse/media/ffmpeg && test -x /mnt/sd/muse/media/mutool && test -r /mnt/sd/muse/media/ca-certificates.crt && test -r /mnt/sd/muse/media/font.ttf')
    # Stage everything before stopping the running services.
    for local, remote, mode in files:
        run('cat > ' + shlex.quote(remote + '.new'), local.read_bytes())
        run(f'chmod {mode:o} ' + shlex.quote(remote + '.new'))
    run('cat > /mnt/sd/muse/state/commands.json.new', json.dumps(specs).encode())
    run('chmod 600 /mnt/sd/muse/state/commands.json.new')
    run('systemctl stop muse-gateway.service muse-media.service')
    for local, remote, mode in files:
        run('mv ' + shlex.quote(remote + '.new') + ' ' + shlex.quote(remote))
    run('mv /mnt/sd/muse/state/commands.json.new /mnt/sd/muse/state/commands.json; systemctl daemon-reload')
    if args.enable_swap:
        run('systemctl enable --now muse-swap.service')
    run('systemctl enable --now muse-media.service muse-gateway.service')
    print(run('systemctl is-active muse-media.service muse-gateway.service'))


if __name__ == '__main__':
    main()
