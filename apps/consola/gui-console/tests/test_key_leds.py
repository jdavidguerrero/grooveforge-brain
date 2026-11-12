"""Observable LED27 output; no hardware or musical timing assertions."""
import csv
from pathlib import Path
import pytest
from key_leds import (KEY_REFS, KeyState, key_frame, key_messages,
                      DIM, ON, BLINK, PULSE, TEAL, VIOLET, BLUE, AMBER)
from test_pad_leds import setup_output


def test_chain_matches_all_csv_keys_once_and_protocol_ranges():
    root = Path(__file__).resolve().parents[4]
    with (root/'apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv').open() as f:
        refs = {r['ref'] for r in csv.DictReader(f) if r['kind'] == 'key'}
    assert len(KEY_REFS) == len(set(KEY_REFS)) == 26
    assert set(KEY_REFS) == refs
    messages = key_messages(25, KeyState(PULSE, VIOLET))
    assert [(m['control'], m['value']) for m in messages] == [(103, 25), (104, 4), (105, 1), (106, 1)]
    assert all(m['channel'] == 15 and m['type'] == 'control_change' for m in messages)
    for index, state in [(-1, KeyState()), (26, KeyState()), (True, KeyState()),
                         (0, KeyState(5)), (0, KeyState(-1)), (0, KeyState(ON, 5))]:
        with pytest.raises(ValueError): key_messages(index, state)


def test_modes_shift_sections_and_host_freshness(gui):
    engine, console, *_ = gui
    b = console.buttons
    for mode, color in [(0, TEAL), (1, TEAL), (2, BLUE), (3, AMBER)]:
        console.triggerAction('mode:'+str(mode))
        frame = key_frame(console, dict(running=True, armed=True), True)
        assert all(frame[i] == KeyState(ON, color) for i in range(5, 9))
        assert frame[0].state == ON and frame[1].state == DIM and frame[2].state == ON
        assert all(frame[i].palette == VIOLET for i in (9, 10, 11, 12, 13, 14, 21, 22, 23, 24, 25))
    b.handle('Boton', dict(nombre='SHIFT', presionado=True))
    b.section('DROP')
    frame = key_frame(console, {}, False)
    assert frame[14] == KeyState(ON, VIOLET)
    assert frame[16] == KeyState(BLINK) and frame[15].state == frame[17].state == DIM
    assert frame[0].state == frame[1].state == frame[2].state == DIM
    b.handle('Boton', dict(nombre='SHIFT', presionado=False))
    b.section('DROP')
    frame = key_frame(console, {}, False)
    assert frame[14].state == frame[16].state == DIM


def test_output_shared_brightness_heartbeat_diff_and_mode_reset(gui):
    engine, output, clock, sent, port = setup_output(gui)
    console = output.surface.console
    assert [(m.control, m.value) for m in sent if m.type == 'control_change' and m.control == 102] == [(102, 27)]
    assert len([m for m in sent if m.type == 'control_change' and m.control == 106]) == 26
    assert any(m.type == 'control_change' and m.control == 25 and m.value == 28 for m in sent)
    sent.clear()
    console.buttons.handle('Boton', dict(nombre='SHIFT', presionado=True))
    output.update(port, 1)
    assert not sent
    clock[0] = .034
    output.update(port, 1)
    assert [(m.control, m.value) for m in sent] == [(103, 14), (104, ON), (105, VIOLET), (106, 1)]
    sent.clear()
    console.preferences.setLedBrightness(0)
    clock[0] = .068
    output.update(port, 1)
    assert any(m.control == 25 and m.value == 0 for m in sent)
    sent.clear()
    clock[0] = .501
    output.update(port, 1)
    assert [(m.control, m.value) for m in sent] == [(110, 0)]
    sent.clear()
    console.triggerAction('mode:3')
    clock[0] = .54
    output.update(port, 2)
    assert any(m.type == 'control_change' and m.control == 102 for m in sent)
    assert all(output.key_frame[i].palette == AMBER for i in range(5, 9))
    assert any(m.type == 'control_change' and m.control == 25 and m.value == 0 for m in sent)


def test_tap_serial_tempo_and_confirmed_running_beats(gui):
    engine, output, clock, sent, port = setup_output(gui, True)
    b = output.surface.console.buttons
    b.tap(10); b.tap(10.5); b.tap(10.6)
    assert b.tap_serial == 2
    clock[0] = .034
    output.update(port, 2)
    assert output.key_controls == (120, 0, 2, 0)
    output.observe(dict(event='clock', clock=dict(running=True, armed=True, bar=1, step=4)), 2)
    clock[0] = .068
    sent.clear(); output.update(port, 2)
    assert output.key_controls[-1] == 1
    assert any(m.type == 'control_change' and m.control == 28 and m.value == 5 for m in sent)
    assert output.key_frame[0].state == output.key_frame[2].state == ON
    clock[0] = 1.1
    output.update(port, 2)
    assert output.key_controls[-1] == 0 and output.key_frame[0].state == DIM
    output.observe(dict(event='clock', clock=dict(running=True, bar=9, step=0)), 1)
    clock[0] = 1.14
    output.update(port, 2)
    assert output.key_controls[-1] == 0


def test_reconnect_snapshot_and_failure_after_partial_key_apply(gui):
    engine, output, clock, sent, port = setup_output(gui)
    sent.clear()
    class Broken:
        def send(self, message):
            sent.append(message)
            if message.type == 'control_change' and message.control == 105:
                raise OSError('key transaction interrupted')
    clock[0] = .034
    with pytest.raises(OSError): output.update(Broken(), 1)
    assert output.port is None
    sent.clear(); output.update(port, 1)
    assert any(m.type == 'control_change' and m.control == 102 for m in sent)
    assert len([m for m in sent if m.type == 'control_change' and m.control == 106]) == 26
    sent.clear(); clock[0] = 1.04
    output.update(port, 1)
    assert not any(m.type == 'control_change' and m.control == 102 for m in sent)
    assert len([m for m in sent if m.type == 'control_change' and m.control == 106]) == 26
