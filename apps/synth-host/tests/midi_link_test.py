"""Offline graph fixtures: USB sources link once; surface and virtual loops stay excluded."""
import importlib.util
import pathlib
import unittest

spec = importlib.util.spec_from_file_location('link_midi', pathlib.Path(__file__).resolve().parents[3] / 'tools/synth-host/link_midi.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)


def obj(id_, kind, **props):
    return dict(id=id_, type='PipeWire:Interface:' + kind, info=dict(props=props))


def port(id_, name, fmt='32 bit raw UMP', direction='out', physical=True):
    return obj(id_, 'Port', **{'node.id': 1, 'port.name': name, 'port.direction': direction,
                             'port.physical': physical, 'format.dsp': fmt})


class MidiLinks(unittest.TestCase):
    def setUp(self):
        self.graph = [obj(1, 'Node', **{'node.name': 'Midi-Bridge'}),
                      obj(2, 'Node', **{'node.name': 'gf-synth-host'}),
                      obj(3, 'Port', **{'node.id': '2', 'port.name': 'midi_in', 'port.direction': 'in'}),
                      port(4, 'Arturia MiniLab mkII'), port(5, 'Other USB', '8 bit raw midi'),
                      port(6, 'GF Consola'), port(7, 'Midi Through'), port(8, 'RtMidi Input'),
                      port(9, 'Virtual sequencer', physical=False),
                      port(10, 'USB Audio', '32 bit float mono audio'),
                      port(11, 'Arturia feedback', direction='in'), port(12, 'consola_s_m4')]

    def test_auto_and_idempotence(self):
        self.graph.append(dict(id=20, type='PipeWire:Interface:Link',
                               info={'output-port-id': 4, 'input-port-id': 3}))
        plan = helper.link_plan(self.graph)
        self.assertEqual([(p[0], p[1], p[3]) for p in plan], [(4, 3, True), (5, 3, False)])

    def test_selector_and_none(self):
        self.assertEqual([p[0] for p in helper.link_plan(self.graph, 'minilab')], [4])
        self.assertEqual(helper.link_plan(self.graph, 'none'), [])
        with self.assertRaises(ValueError):
            helper.link_plan(self.graph, 'GF Consola')

    def test_errors(self):
        with self.assertRaises(ValueError):
            helper.link_plan(self.graph, 'missing keyboard')
        with self.assertRaises(ValueError):
            helper.link_plan(self.graph[:2])
        self.graph.append(self.graph[2])
        with self.assertRaises(ValueError):
            helper.link_plan(self.graph)


if __name__ == '__main__':
    unittest.main()
