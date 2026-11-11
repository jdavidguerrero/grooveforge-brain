"""Output behavior, protocol and persistent settings; no physical timing claims."""
from dataclasses import replace
from types import SimpleNamespace
import json
import pytest
from pad_leds import (PadState, pad_messages, synth_frame, drum_frame, cc, rgb7,
                      SELECTED, DIM, HELD, ROOT, PLAYHEAD, HIT)
from scale_keyboard import ScaleKeyboard
from local_settings import LocalSettings


def test_protocol_ranges_and_atomic_apply():
    messages = pad_messages(31, PadState((127, 64, 0), HIT, 80, 127))
    assert [m.get('control') for m in messages[:-1]] == [20, 21, 22, 29, 30, 31]
    assert all(m['channel'] == 15 for m in messages)
    assert messages[-1] == dict(type='note_on', channel=15, note=79, velocity=1)
    for index, state in [(32, PadState()), (-1, PadState()), (0, PadState((128, 0, 0))),
                         (0, PadState(flags=64)), (0, PadState(level=-1))]:
        with pytest.raises(ValueError): pad_messages(index, state)


def test_synth_held_velocity_original_role_and_layout_release():
    keyboard = ScaleKeyboard()
    held = {(1, keyboard.note(24)): 80}
    frame = synth_frame(keyboard, 2, held)
    assert frame[24].flags & HELD and frame[24].flags & ROOT
    assert frame[24].rgb == rgb7('#3D8CFF') and frame[24].level == 80
    assert frame[2].flags & SELECTED
    moved = synth_frame(replace(keyboard, octave=4), 2, held, {('Pad', 24):next(iter(held))})
    assert moved[24].flags & HELD and moved[24].rgb == frame[24].rgb
    assert not synth_frame(keyboard, 2, {})[24].flags & HELD
    assert frame[25].level < frame[24].level


def setup_output(gui, drum=False):
    engine, console, *_ = gui
    console.triggerAction('mode:3' if drum else 'mode:2')
    output = engine.synth_surface.led_output
    clock = [0.]
    output.now = lambda:clock[0]
    sent = []
    port = SimpleNamespace(send=sent.append)
    output.update(port, 2 if drum else 1)
    return engine, output, clock, sent, port


def test_diffs_budget_snapshot_reconnect_and_heartbeat_freshness(gui):
    engine, output, clock, sent, port = setup_output(gui)
    assert len(sent) == 230
    output.update(port, 1)
    assert len(sent) == 230
    clock[0] = .034
    output.observe(dict(event='meters'), 1)
    output.update(port, 1)
    assert any(m.type == 'control_change' and m.control == 119 for m in sent)
    sent.clear()
    clock[0] = .1
    output.update(port, 1)
    assert not sent
    clock[0] = .534
    output.observe(dict(event='meters'), 1)
    output.update(port, 1)
    assert [m.control for m in sent] == [119]
    sent.clear()
    clock[0] = 1.6
    output.update(port, 1)
    assert len([m for m in sent if m.type == 'note_on']) == 32
    assert not any(m.type == 'control_change' and m.control == 119 for m in sent)
    sent.clear()
    clock[0] = 1.7
    output.observe(dict(event='meters'), 1)
    output.update(port, 1)
    assert len([m for m in sent if m.type == 'note_on']) == 32
    sent.clear()
    clock[0] = 1.8
    output.update(SimpleNamespace(send=sent.append), 1)
    assert sent[0].control == 23 and sent[0].value == 23


def test_host_hits_only_no_history_and_epoch_baseline(gui):
    engine, output, clock, sent, port = setup_output(gui, True)
    d = engine.drum_state
    state = dict(audio_epoch='a', running=True, bar=1, step=0, pattern=0,
                 led_hits=['0']*16, led_velocity=[100]*16, led_choked=[0]*16)
    output.observe(dict(event='clock', clock=state), 2)
    assert not output.pending_hits
    d.record_hit(0, 127)
    assert not output.pending_hits  # A UI intention is not an audible hit.
    state = dict(state, led_hits=['1']+['0']*15)
    output.observe(dict(event='clock', clock=state), 2)
    assert output.pending_hits == {0:(100, 1)}
    clock[0] = .034
    output.update(port, 2)
    assert output.frame[0].flags & HIT and output.frame[0].level == 100
    sent.clear()
    clock[0] = .068
    output.update(port, 2)
    assert not output.frame[0].flags & HIT and output.frame[0].serial == 1
    output.observe(dict(event='clock', clock=dict(state, audio_epoch='b', led_hits=['90']*16)), 2)
    assert not output.pending_hits
    output.observe(dict(event='clock', clock=dict(state, audio_epoch='b', led_hits=['91']+['90']*15,led_choked=[1]*16)), 2)
    clock[0] = .1021
    output.update(port, 2)
    assert output.frame[0].flags & DIM


def test_steps_selected_color_velocity_page_and_mutes(gui):
    engine, *_ = gui
    drum = engine.drum_state
    lane = drum._rack['patterns'][0]['lanes'][0]
    lane['length'] = 32
    lane['steps'][0]['velocity'] = 40
    lane['steps'][1]['velocity'] = 100
    frame = drum_frame(drum)
    assert frame[0].flags & SELECTED
    assert frame[16].rgb == frame[0].rgb
    assert (frame[16].level, frame[17].level) == (40, 100)
    drum._clock.update(running=True, pattern_step='1')
    assert drum_frame(drum)[17].flags & PLAYHEAD
    drum._step_page = 1
    assert not any(p.flags & PLAYHEAD for p in drum_frame(drum)[16:])
    drum._rack['groups'][0]['mute'] = True
    assert drum_frame(drum)[0].flags & DIM


def test_settings_cap_persistence_invalid_data_and_hardware_keys(gui):
    engine, console, *_ = gui
    prefs = console.preferences
    assert prefs.ledBrightness == 28 and prefs.ledAnimations
    prefs.setLedBrightness(127)
    prefs.toggleLedAnimations()
    restored = LocalSettings()
    assert restored.ledBrightness == 31 and not restored.ledAnimations
    assert json.loads(prefs.path.read_text())['drum_pad_velocity'] == 100
    prefs.path.write_text('{"led_brightness":true,"led_animations":"false"}')
    restored = LocalSettings()
    assert restored.ledBrightness == 28 and restored.ledAnimations
    console.navigate('Settings')
    console.pressEncoder(4)
    console.pressEncoder(5)
    assert prefs.ledBrightness == 0 and prefs.ledAnimations


def test_note_observer_does_not_change_music_and_failed_notes_do_not_light(gui):
    engine, output, clock, sent, port = setup_output(gui)
    surface = engine.synth_surface
    surface.notes.note(1, 60, 81)
    assert surface.notes.events[-1] == (1, 60, 81)
    assert output.notes.snapshot() == {(1, 60):81}
    surface.notes.note(1, 60, 0)
    assert not output.notes.snapshot()


def test_partial_send_retries_full_snapshot(gui):
    engine, output, clock, sent, port = setup_output(gui)
    def fail(message): raise OSError('USB lost')
    clock[0] = .034
    with pytest.raises(OSError): output.update(SimpleNamespace(send=fail), 1)
    assert output.port is None
    sent.clear()
    output.update(port, 1)
    assert len([m for m in sent if m.type == 'note_on']) == 32
