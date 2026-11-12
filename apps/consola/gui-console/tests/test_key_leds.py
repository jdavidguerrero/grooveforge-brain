"""Observable LED27 output; no hardware or musical timing assertions."""
import csv
import io
import re
import subprocess
from pathlib import Path
import pytest
from key_leds import (KEY_CHAIN, KEY_REFS, KeyState, key_frame, key_messages,
                      DIM, ON, BLINK, PULSE, TEAL, VIOLET, BLUE, AMBER)
from test_pad_leds import setup_output


def test_chain_matches_cpp_specs_and_all_csv_keys_once(tmp_path):
    root = Path(__file__).resolve().parents[4]
    include = root/'apps/consola/surface/include'
    binary = tmp_path/'key-led-preview'
    subprocess.run(['c++', '-std=c++17', '-I'+str(include),
                    str(root/'apps/consola/gui-console/tools/key_led_preview.cpp'),
                    str(root/'apps/consola/surface/src/KeyLedReturn.cpp'),
                    '-o', str(binary)], check=True)
    output = subprocess.check_output([str(binary), '--chain'], text=True)
    cpp = tuple((ref, float(x), float(y)) for ref, x, y in csv.reader(io.StringIO(output)))
    assert cpp == KEY_CHAIN
    assert len(KEY_REFS) == len(set(KEY_REFS)) == 26
    with (root/'apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv').open() as f:
        keys = [r for r in csv.DictReader(f) if r['kind'] == 'key']
    assert len(keys) == len({r['ref'] for r in keys}) == 26
    assert {ref: (x, y) for ref, x, y in cpp} == {
        r['ref']: (float(r['x_mm']), float(r['y_mm'])) for r in keys}
    spec = (root/'apps/docs/consola/27-leds-teclas.md').read_text()
    entries = re.findall(r'^\|(\d+)\|([^|]+)\|([\d.]+)\|([\d.]+)\|$', spec, re.M)
    assert [int(i) for i, *_ in entries] == list(range(26))
    assert tuple((ref, float(x), float(y)) for _, ref, x, y in entries) == cpp
    pcb = (root/'apps/docs/consola/28-pcb-k.md').read_text()
    entries = [line.split('|') for line in pcb.splitlines() if line.startswith('| 0x2')]
    led_rows = [fields for fields in entries if fields[-2].strip().isdigit()]
    assert len(led_rows) == 26
    for fields in led_rows:
        ref = fields[3].strip().replace('−', '-')
        index = int(fields[-2])
        x, y = map(float, fields[4].split(','))
        assert cpp[index] == (ref, x, y)


def test_protocol_ranges():
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
        assert all(frame[i] == KeyState(ON, color) for i in range(22, 26))
        assert frame[17].state == ON and frame[18].state == DIM and frame[19].state == ON
        assert all(frame[i].palette == VIOLET for i in (0, 1, 2, 3, 4, 11, 12, 13, 14, 15, 16))
    b.handle('Boton', dict(nombre='SHIFT', presionado=True))
    b.section('DROP')
    frame = key_frame(console, {}, False)
    assert frame[11] == KeyState(ON, VIOLET)
    assert frame[9] == KeyState(BLINK) and frame[8].state == frame[10].state == DIM
    assert frame[17].state == frame[18].state == frame[19].state == DIM
    b.handle('Boton', dict(nombre='SHIFT', presionado=False))
    b.section('DROP')
    frame = key_frame(console, {}, False)
    assert frame[11].state == frame[9].state == DIM


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
    assert [(m.control, m.value) for m in sent] == [(103, 11), (104, ON), (105, VIOLET), (106, 1)]
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
    assert all(output.key_frame[i].palette == AMBER for i in range(22, 26))
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
    assert output.key_frame[17].state == output.key_frame[19].state == ON
    clock[0] = 1.1
    output.update(port, 2)
    assert output.key_controls[-1] == 0 and output.key_frame[17].state == DIM
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
