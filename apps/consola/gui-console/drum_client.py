"""Additive DRUM v1 client, sharing the tested JSONL/reconnection/frame seam of SYNTH."""
from synth_client import SynthClient

DRUM_COMMANDS = frozenset(('hello', 'subscribe', 'get_drum_state', 'list_pad_params', 'load_rack',
                          'set_pad_param', 'pad_trigger', 'set_pattern', 'set_steps', 'transport',
                          'record_arm', 'set_group', 'list_racks', 'save_rack', 'save_pattern',
                          'list_drum_samples', 'set_pad_sample', 'import_folder', 'preview',
                          'generate_variations', 'render_rack'))


class DrumClient(SynthClient):
    commands = DRUM_COMMANDS
    event_names = frozenset(('drum', 'clock', 'recorded', 'drum_meters', 'drum_job', 'drum_error'))

    def initial_requests(self):
        return [('hello', dict(client='grooveforge-drum')),
                ('subscribe', dict(topics=['drum', 'clock', 'drum_meters', 'drum_job'])),
                ('get_drum_state', {}), ('list_pad_params', {}), ('list_racks', {}),
                ('list_drum_samples', {})]
