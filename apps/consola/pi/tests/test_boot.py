"""Host-independent routing, installation and unit contracts (no Pi claimed)."""
import configparser
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import Mock, patch

PI = Path(__file__).resolve().parents[1]


def module(name):
    spec = importlib.util.spec_from_file_location(name, PI / 'systemd' / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


links = module('links')
wait_host = module('wait_host')


def node(ident, name):
    return dict(id=ident, type='PipeWire:Interface:Node', info=dict(props={'node.name': name}))


def port(ident, parent, name, direction, **props):
    return dict(id=ident, type='PipeWire:Interface:Port', info=dict(props={
        'node.id': parent, 'port.name': name, 'port.direction': direction, **props}))


def graph():
    return [node(1, 'gf-synth-host'), node(2, 'audio-plus'), node(3, 'Teensy'),
            port(10, 1, 'mix_L', 'out'), port(11, 1, 'mix_R', 'out'),
            port(12, 1, 'cue_L', 'out'), port(13, 1, 'cue_R', 'out'),
            port(14, 1, 'midi_in', 'in'),
            port(20, 2, 'playback_FL', 'in'), port(21, 2, 'playback_FR', 'in'),
            port(30, 3, 'playback_FL', 'in'), port(31, 3, 'playback_FR', 'in')]


ENV = dict(GF_AUDIO_NODE='audio-plus', GF_CUE_NODE='Teensy')


class RoutingTests(unittest.TestCase):
    def test_ports_late_then_present_and_idempotent(self):
        self.assertEqual(links.audio_plan([], ENV)[0], [])
        objects = graph()
        plan, errors = links.audio_plan(objects, ENV)
        self.assertFalse(errors)
        self.assertEqual(plan, [(10, 20), (11, 21), (12, 30), (13, 31)])
        for i, (source, target) in enumerate(plan):
            objects.append(dict(id=100+i, type='PipeWire:Interface:Link',
                                info={'output-port-id': source, 'input-port-id': target}))
        self.assertEqual(links.audio_plan(objects, ENV), ([], []))
        # Re-enumeration replaces node and port IDs, including surviving links to mix.
        objects = [o for o in objects if o['id'] not in (3, 30, 31, 102, 103)]
        objects += [node(9, 'Teensy'), port(90, 9, 'playback_FL', 'in'),
                    port(91, 9, 'playback_FR', 'in')]
        self.assertEqual(links.audio_plan(objects, ENV), ([(12, 90), (13, 91)], []))

    def test_missing_teensy_does_not_block_mix(self):
        objects = [o for o in graph() if o['id'] not in (3, 30, 31)]
        plan, errors = links.audio_plan(objects, ENV)
        self.assertEqual(plan, [(10, 20), (11, 21)])
        self.assertEqual(len(errors), 2)

    def test_unconfigured_or_ambiguous_never_guesses(self):
        self.assertEqual(links.audio_plan(graph(), {})[0], [])
        objects = graph() + [port(99, 2, 'playback_FL', 'in')]
        plan, errors = links.audio_plan(objects, ENV)
        self.assertNotIn((10, 20), plan)
        self.assertTrue(errors)

    def test_midi_ump_hotplug_and_exclusions(self):
        objects = graph()
        self.assertEqual(links.plan_links('midi', objects, {}), ([], []))
        objects += [node(4, 'MiniLab'), port(40, 4, 'capture', 'out', **{
            'format.dsp': '32 bit raw UMP', 'port.physical': True}),
                    node(5, 'GF Consola'), port(50, 5, 'capture', 'out', **{
            'format.dsp': '8 bit raw midi', 'port.physical': True})]
        self.assertEqual(links.plan_links('midi', objects, {}), ([(40, 14)], []))
        self.assertEqual(links.plan_links('midi', objects, {'GF_EXTERNAL_MIDI_PORT': 'none'}), ([], []))
        objects += [dict(id=60, type='PipeWire:Interface:Link',
                         info={'output-port-id': 40, 'input-port-id': 14})]
        self.assertEqual(links.plan_links('midi', objects, {}), ([], []))

    def test_failed_link_retried_from_next_snapshot(self):
        runner = Mock()
        snapshot = subprocess.CompletedProcess([], 0, json.dumps(graph()), '')
        failed = subprocess.CompletedProcess([], 1, '', 'port vanished')
        success = subprocess.CompletedProcess([], 0, '', '')
        runner.side_effect = [snapshot, failed, success, success, success,
                              snapshot, success, success, success, success]
        self.assertEqual(len(links.reconcile('audio', ENV, runner)), 1)
        self.assertFalse(links.reconcile('audio', ENV, runner))
        self.assertEqual(runner.call_args_list[1].args[0], ['pw-link', '-L', '10', '20'])

    def test_gui_start_stops_live_transport_and_requires_ack(self):
        client = Mock()
        client.request.side_effect = [{}, {}, {}, {'clock': dict(running=False, armed=False, queued=-1)}]
        wait_host.stop_for_gui(client)
        self.assertEqual([call.args[0] for call in client.request.call_args_list],
                         ['hello', 'transport', 'preview', 'get_drum_state'])
        self.assertEqual(client.request.call_args_list[1].kwargs, {'action': 'stop'})
        client.request.side_effect = [{}, {}, {}, {'clock': dict(running=True, armed=True, queued=2)}]
        with self.assertRaisesRegex(RuntimeError, 'D4'):
            wait_host.stop_for_gui(client)


    def test_wait_retries_stale_socket_then_closes_live_client(self):
        client = Mock()
        client.request.side_effect = [{}, {}, {}, {'clock': dict(running=False, armed=False, queued=-1)}]
        with patch.object(wait_host, 'Client', side_effect=[ConnectionRefusedError('stale'), client]), \
                patch.object(wait_host.time, 'sleep') as sleep, \
                patch.dict(os.environ, {'XDG_RUNTIME_DIR': '/run/user/1000'}):
            wait_host.main()
        sleep.assert_called_once_with(1)
        client.close.assert_called_once()

    def test_monitor_failure_exits_for_systemd_restart(self):
        monitor = Mock()
        with patch.object(links.subprocess, 'Popen', return_value=monitor), \
                patch.object(links.select, 'select', return_value=([monitor.stdout], [], [])), \
                patch.object(links.os, 'read', return_value=b''):
            with self.assertRaisesRegex(RuntimeError, 'monitor closed'):
                links.watch('midi')
        monitor.terminate.assert_called_once()
        monitor.wait.assert_called_once_with(timeout=3)

    def test_registry_bursts_rate_limit_snapshots_and_retry_after_quiet(self):
        monitor = Mock()
        now = [10.0]
        snapshots = []
        events = iter([(10.0, True), (10.1, True), (11.0, True), (16.0, False)])

        def select_event(*args):
            when, event = next(events)
            now[0] = when
            return ([monitor.stdout] if event else [], [], [])

        def reconcile(*args):
            snapshots.append(now[0])
            if len(snapshots) == 3:
                raise RuntimeError('end test')
            return ()

        with patch.object(links.subprocess, 'Popen', return_value=monitor), \
                patch.object(links.select, 'select', side_effect=select_event), \
                patch.object(links.os, 'read', return_value=b'[]'), \
                patch.object(links.time, 'monotonic', side_effect=lambda: now[0]), \
                patch.object(links, 'reconcile', side_effect=reconcile):
            with self.assertRaisesRegex(RuntimeError, 'end test'):
                links.watch('audio')
        self.assertEqual(snapshots, [10.0, 11.0, 16.0])


class UnitTests(unittest.TestCase):
    def test_supervision_and_graph(self):
        names = ('gf-synth-host', 'gf-console-gui', 'gf-audio-links', 'gf-midi-link')
        for name in names:
            unit = configparser.ConfigParser(interpolation=None, strict=True)
            unit.read(PI / 'systemd' / (name + '.service'))
            self.assertEqual(unit['Unit']['PartOf'], 'gf-console.target')
            self.assertEqual(unit['Unit']['StartLimitIntervalSec'], '0')
            self.assertIn('pipewire.service', unit['Unit']['After'])
            self.assertIn('wireplumber.service', unit['Unit']['After'])
            if name != 'gf-synth-host':
                self.assertIn('gf-synth-host.service', unit['Unit']['After'])
            service = unit['Service']
            self.assertEqual(service['Restart'], 'on-failure')
            self.assertEqual(service['RestartSec'], '2s')
            self.assertEqual(service['RestartMaxDelaySec'], '30s')
            self.assertEqual(service['RestartSteps'], '5')
            self.assertEqual(service['StandardError'], 'journal')
            if name == 'gf-synth-host':
                self.assertEqual(service['LimitRTPRIO'], '70')
                self.assertNotIn('LimitMEMLOCK', service)
            for forbidden in ('User', 'CPUSchedulingPolicy', 'WatchdogSec'):
                self.assertNotIn(forbidden, service)
            self.assertIn('%h/.local/lib/gf-consola/run.sh', service['ExecStart'])
        target = configparser.ConfigParser(interpolation=None)
        target.read(PI / 'systemd/gf-console.target')
        self.assertEqual(set(target['Unit']['Wants'].split()), {n+'.service' for n in names})
        self.assertEqual(target['Install']['WantedBy'], 'default.target')

    def test_host_launcher_arguments_are_safe_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executable = root/'fake host'
            executable.write_text('#!/usr/bin/env python3\nimport json,sys\nprint(json.dumps(sys.argv[1:]))\n')
            executable.chmod(0o755)
            env = dict(os.environ, HOME=str(root), XDG_RUNTIME_DIR=str(root), GF_HOST_BINARY=str(executable))
            result = subprocess.run(['bash', str(PI/'systemd/run.sh'), 'host'],
                                    env=env, check=True, capture_output=True, text=True)
            args = json.loads(result.stdout)
            self.assertEqual(args[:4], ['--backend', 'pipewire', '--block', '256'])
            self.assertNotIn('--benchmark', args)
            self.assertNotIn('--no-state', args)
            self.assertIn(str(root/'.local/share/gf-drum'), args)
            self.assertIn('--drum-kit-python', args)


class InstallerTests(unittest.TestCase):
    def test_dry_run_does_not_write_or_invoke_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for arguments in (['--dry-run'], ['--dry-run', '--uninstall']):
                subprocess.run(['bash', str(PI/'install.sh'), *arguments],
                               env=dict(os.environ, HOME=str(root)), check=True, capture_output=True)
                self.assertEqual(list(root.iterdir()), [])

    def test_install_repeat_preserves_config_and_uninstall_preserves_data(self):
        if os.geteuid() == 0:
            self.skipTest('Installer intentionally refuses root; run integration test as a user')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fakebin = root/'bin'
            fakebin.mkdir()
            log = root/'commands'
            for command in ('systemctl', 'sudo', 'loginctl', 'pw-dump', 'pw-link'):
                script = fakebin/command
                script.write_text('#!/bin/sh\nprintf "%s\\n" "$0 $*" >> "$GF_TEST_LOG"\n')
                script.chmod(0o755)
            (fakebin/'uname').write_text('#!/bin/sh\necho Linux\n')
            (fakebin/'uname').chmod(0o755)
            env = dict(os.environ, HOME=str(root), PATH=str(fakebin)+':'+os.environ['PATH'], GF_TEST_LOG=str(log))
            args = ['bash', str(PI/'install.sh'), '--host-binary', '/usr/bin/true', '--audio-node', 'Audio+ name']
            subprocess.run(args, env=env, check=True, capture_output=True)
            config = root/'.config/gf-consola/config'
            original = config.read_text() + '\nGF_CONSOLE_MODE=SYNTH\n'
            config.write_text(original)
            subprocess.run(args, env=env, check=True, capture_output=True)
            self.assertEqual(config.read_text(), original)
            self.assertEqual(config.stat().st_mode & 0o777, 0o600)
            for unit in (PI/'systemd').glob('*.service'):
                self.assertEqual((root/'.config/systemd/user'/unit.name).read_bytes(), unit.read_bytes())
            subprocess.run(['bash', str(PI/'install.sh'), '--uninstall'], env=env, check=True, capture_output=True)
            self.assertEqual(config.read_text(), original)
            self.assertFalse((root/'.config/systemd/user/gf-console.target').exists())
            self.assertFalse((root/'.local/lib/gf-consola/run.sh').exists())
            self.assertIn('disable --now gf-console.target', log.read_text())
            self.assertNotIn('disable-linger', log.read_text())


if __name__ == '__main__':
    unittest.main()
