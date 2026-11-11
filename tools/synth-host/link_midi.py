#!/usr/bin/env python3
"""Link physical MIDI sources to gf-synth-host in the current PipeWire session."""
import argparse
import json
import subprocess
import sys

EXCLUDED = ('gf consola', 'gf superficie', 'consola_s_m4', 'midi through',
            'gf-synth-host', 'gf-synth-gui', 'gui-console', 'rtmidi')
MIDI_FORMATS = ('8 bit raw midi', '32 bit raw UMP')


def link_plan(objects, selector='auto'):
    nodes = {str(o['id']): o.get('info', {}).get('props', {})
             for o in objects if o['type'] == 'PipeWire:Interface:Node'}
    ports = [o for o in objects if o['type'] == 'PipeWire:Interface:Port']
    targets = [o for o in ports
               if o.get('info', {}).get('props', {}).get('port.name') == 'midi_in'
               and nodes.get(str(o['info']['props'].get('node.id')), {}).get('node.name') == 'gf-synth-host'
               and o['info']['props'].get('port.direction') == 'in']
    if len(targets) != 1:
        raise ValueError(f'Expected one gf-synth-host:midi_in, found {len(targets)}; start the host first')
    target = targets[0]['id']
    links = {tuple(str(o.get('info', {}).get(k)) for k in ('output-port-id', 'input-port-id'))
             for o in objects if o['type'] == 'PipeWire:Interface:Link'}
    selected = []
    for port in ports:
        props = port.get('info', {}).get('props', {})
        node = nodes.get(str(props.get('node.id')), {})
        name = ':'.join(str(p.get(k, '')) for p, k in
                        ((node, 'node.name'), (props, 'port.name'), (props, 'port.alias')))
        if props.get('port.direction') != 'out' or props.get('format.dsp') not in MIDI_FORMATS:
            continue
        if any(token in name.lower() for token in EXCLUDED):
            continue
        if selector.lower() == 'auto':
            if str(props.get('port.physical', False)).lower() != 'true':
                continue
        elif selector.lower() == 'none' or selector.lower() not in name.lower():
            continue
        selected.append((port['id'], target, name, (str(port['id']), str(target)) in links))
    if selector.lower() not in ('auto', 'none') and not selected:
        raise ValueError(f'No external MIDI source matches {selector!r}')
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--external-midi-port', default='auto',
                        help='auto: physical MIDI outputs; none: disable; otherwise name substring')
    parser.add_argument('--dry-run', action='store_true', help='Print links without changing the graph')
    args = parser.parse_args()
    if args.external_midi_port.lower() == 'none':
        return
    objects = json.loads(subprocess.check_output(['pw-dump'], text=True))
    plan = link_plan(objects, args.external_midi_port)
    if not plan:
        print('No external physical MIDI sources found; connect USB and rerun.', file=sys.stderr)
    for source, target, name, exists in plan:
        print(f'{name} -> gf-synth-host:midi_in ({"already linked" if exists else "new"})')
        if not args.dry_run and not exists:
            subprocess.run(['pw-link', str(source), str(target)], check=True)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        print(f'MIDI link failed: {exc}', file=sys.stderr)
        sys.exit(1)
