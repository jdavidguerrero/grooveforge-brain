#!/usr/bin/env python3
"""Reconcile audio or physical MIDI links on PipeWire registry changes."""
import argparse
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / 'tools/synth-host'))
from link_midi import link_plan as midi_plan


def props(obj):
    return obj.get('info', {}).get('props', {})


def audio_plan(objects, env):
    nodes = {str(o['id']): props(o) for o in objects
             if o['type'] == 'PipeWire:Interface:Node'}
    ports = [o for o in objects if o['type'] == 'PipeWire:Interface:Port']
    existing = {(str(o['info'].get('output-port-id')), str(o['info'].get('input-port-id')))
                for o in objects if o['type'] == 'PipeWire:Interface:Link' and o.get('info')}
    plan, problems = [], []

    def find(node, port, direction):
        return [o['id'] for o in ports
                if nodes.get(str(props(o).get('node.id')), {}).get('node.name') == node
                and props(o).get('port.name') == port
                and props(o).get('port.direction') == direction]

    for bus, key in (('mix', 'GF_AUDIO'), ('cue', 'GF_CUE')):
        node = env.get(key + '_NODE', '')
        if not node:
            problems.append(f'{key}_NODE not configured; {bus} left unlinked')
            continue
        for channel, side in (('L', 'LEFT'), ('R', 'RIGHT')):
            source = find('gf-synth-host', f'{bus}_{channel}', 'out')
            target_name = env.get(key + '_' + side, 'playback_F' + channel)
            target = find(node, target_name, 'in')
            if len(source) != 1 or len(target) != 1:
                problems.append(f'{bus}_{channel} -> {node}:{target_name}: '
                                f'expected unique ports, found {len(source)}/{len(target)}')
                continue
            pair = source[0], target[0]
            if tuple(map(str, pair)) not in existing:
                plan.append(pair)
    return plan, problems


def plan_links(kind, objects, env):
    if kind == 'audio':
        return audio_plan(objects, env)
    try:
        plan = midi_plan(objects, env.get('GF_EXTERNAL_MIDI_PORT', 'auto'))
        return [(s, t) for s, t, _, exists in plan if not exists], []
    except ValueError as exc:
        return [], [str(exc)]


def reconcile(kind, env, run=subprocess.run):
    result = run(['pw-dump'], check=True, capture_output=True, text=True, timeout=10)
    plan, problems = plan_links(kind, json.loads(result.stdout), env)
    for source, target in plan:
        result = run(['pw-link', '-L', str(source), str(target)],
                     check=False, capture_output=True, text=True, timeout=10)
        if result.returncode:
            problems.append(f'pw-link {source} -> {target}: {result.stderr.strip()}')
        else:
            print(f'Linked {source} -> {target}', flush=True)
    return tuple(problems)


def watch(kind):
    # The monitor is only an invalidation signal: a fresh dump avoids merging
    # partial updates/removals or keeping IDs across USB re-enumeration.
    monitor = subprocess.Popen(['pw-dump', '--monitor', '--no-colors'], stdout=subprocess.PIPE)
    try:
        pending, last, retry = True, float('-inf'), 0.0
        previous = None
        while True:
            now = time.monotonic()
            due = max(last + 1, now if pending else retry)
            readable, _, _ = select.select([monitor.stdout], [], [], max(0, due - now))
            if readable:
                if not os.read(monitor.stdout.fileno(), 65536):
                    raise RuntimeError(f'PipeWire monitor closed ({monitor.poll()})')
                pending = True
            now = time.monotonic()
            if now >= last + 1 and (pending or now >= retry):
                problems = reconcile(kind, os.environ)
                if problems != previous:
                    for problem in problems:
                        print(problem, file=sys.stderr, flush=True)
                    if previous and not problems:
                        print('All configured ports resolved', flush=True)
                    previous = problems
                last = time.monotonic()
                retry, pending = last + 5, False
    finally:
        monitor.terminate()
        try:
            monitor.wait(timeout=3)
        except subprocess.TimeoutExpired:
            monitor.kill()
            monitor.wait()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=('audio', 'midi'))
    parser.add_argument('--once', action='store_true', help='Manual reconciliation, without monitor')
    args = parser.parse_args()
    try:
        if args.once:
            errors = reconcile(args.kind, os.environ)
            for error in errors:
                print(error, file=sys.stderr)
            sys.exit(bool(errors))
        watch(args.kind)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
        print(f'Link watcher failed: {exc}', file=sys.stderr, flush=True)
        sys.exit(1)
