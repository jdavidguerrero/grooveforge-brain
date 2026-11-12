"""Observable control routing; no audio/physical latency claim from these tests."""
import json
import socket
from types import SimpleNamespace
from pathlib import Path
import sys
from threading import Thread, Event

import pytest
from PySide6.QtCore import Qt, QPoint
from PySide6.QtTest import QTest
from scale_keyboard import ScaleKeyboard, ROOTS, SCALES
from synth_surface import (PreviewNotes, SocketNotes, SynthSurface, pot_command,
                           led_messages, MidiNotes, RoomControls)
from superficie import (Pad, Fsr, Encoder, Fader, Mano, ManoPresencia, Expresion,
                        Perilla, Jog, Boton, SelectorPad, decodificar)


def enter(gui):
    engine, state, window, messages = gui
    state.triggerAction('mode:2')
    synth = engine.synth_state
    synth.selectPart(1)
    synth.action('open:1')
    engine.surface_batcher.drain_frame()
    return engine.synth_surface, synth, engine.surface_batcher


@pytest.mark.parametrize('scale', SCALES)
def test_in_key_fourths_octaves_and_high_limit(scale):
    keyboard = ScaleKeyboard(0, scale, 3)
    assert keyboard.note(24) == 48
    assert keyboard.degree(16) == 3
    assert keyboard.note(16) == 48+SCALES[scale][3]
    assert keyboard.note(25) in [48+x for x in SCALES[scale]]
    assert ScaleKeyboard(0, scale, 4).note(24) == keyboard.note(24)+12
    assert keyboard.is_root(24)
    assert not keyboard.is_root(25)
    assert all(keyboard.note(i)%12 in SCALES[scale] for i in range(32))
    assert ScaleKeyboard(11, scale, 7).note(7) is None or ScaleKeyboard(11, scale, 7).note(7) <= 127
    with pytest.raises(ValueError): keyboard.note(32)


def test_immediate_notes_bypass_frame_and_release_original_part_pitch(gui):
    surface, synth, batcher = enter(gui)
    surface.setRoot(0)
    surface.fakePad(24, True)
    assert surface.notes.events == [(1, 48, 100)]
    assert batcher.pending == 1  # Sound was sent before GUI drain.
    synth.selectPart(2)
    surface.transpose(1)
    surface.setScale('Major')
    surface.fakePad(24, False)
    assert surface.notes.events[-1] == (1, 48, 0)
    surface.fakePad(24, True)
    assert surface.notes.events[-1] == (2, 60, 100)
    surface.panic()
    assert surface.notes.events[-1] == (2, 60, 0)


def test_overlapping_sources_velocity_zero_and_exit_panic(gui):
    surface, synth, batcher = enter(gui)
    surface.setRoot(0)
    surface.fakePad(24, True)
    surface.external('keys', SimpleNamespace(type='note_on', channel=3, note=48, velocity=80))
    surface.fakePad(24, False)
    assert len(surface.notes.events) == 1
    surface.external('keys', SimpleNamespace(type='note_on', channel=3, note=48, velocity=0))
    assert surface.notes.events[-1] == (1, 48, 0)
    surface.receive(Fsr(2, True, 1))
    assert surface.notes.events[-1] == (1, 42, 127)
    surface.console.triggerAction('mode:1')
    assert surface.notes.events[-1] == (1, 42, 0)
    assert not surface.held and not surface.refs
    surface.fakePad(24, True)
    assert surface.notes.events[-1] == (1, 42, 0)


def test_encoder_step_clamp_fine_and_frame_coalescing(gui):
    surface, synth, batcher = enter(gui)
    synth.selectPage('FILTER')
    meta = synth.bank[0]
    before = synth.values[meta['id']]
    sent = []
    original = synth.request
    def request(cmd, **fields):
        sent.append((cmd, fields))
        return original(cmd, **fields)
    synth.request = request
    surface.receive(Encoder(0, 1))
    surface.receive(Encoder(0, 2))
    assert not sent
    batcher.drain_frame()
    edits = [f for cmd,f in sent if cmd == 'set_param']
    assert len(edits) == 1
    assert edits[0]['value'] == min(meta['max'], before+3*meta['step'])
    surface.receive(Boton('SHIFT', True))
    surface.receive(Encoder(0, -1))
    batcher.drain_frame()
    assert synth.values[meta['id']] == pytest.approx(edits[0]['value']-.1*meta['step'])
    surface.receive(Encoder(0, 1000000))
    batcher.drain_frame()
    assert synth.values[meta['id']] == meta['max']
    surface.receive(Encoder(0, -1000000))
    batcher.drain_frame()
    assert synth.values[meta['id']] == meta['min']


def test_faders_hands_presence_expression_and_pots(gui):
    surface, synth, batcher = enter(gui)
    for i in range(3): surface.console.preferences.toggleHand(i)
    surface.receive(ManoPresencia(0, True))
    surface.receive(ManoPresencia(1, True))
    calls = []
    original = synth.request
    synth.request = lambda cmd, **f: (calls.append((cmd, f)), original(cmd, **f))[1]
    room_calls = []
    surface.room = SimpleNamespace(submit=lambda n,v:room_calls.append((n,v)), error='')
    for i in range(4): surface.receive(Fader(i, i/3))
    surface.receive(Mano(0, 1))
    surface.receive(ManoPresencia(1, False))
    surface.receive(Mano(1, 1))
    surface.receive(Expresion(1, .5))
    for name in ('MASTER','CUE','IN'): surface.receive(Perilla(name, 1))
    surface.receive(SelectorPad(-10))
    assert not calls
    batcher.drain_frame()
    assert [f for cmd,f in calls if cmd=='set_level'] == [dict(part=i+1,db=-60+24*i) for i in range(4)]
    edits = {f['param']:f['value'] for cmd,f in calls if cmd=='set_param'}
    metas = {p['id']:p for pg in synth.current_pages for bank in pg['banks'] for p in bank}
    assert edits['cutoff'] == metas['cutoff']['max']
    assert edits['move'] == (metas['move']['min']+metas['move']['max'])/2
    assert room_calls == [('MASTER',1),('CUE',1),('IN',1)]
    assert surface.status == 'INPUT PAD -10 dB (physical)'
    surface.room = None


@pytest.mark.parametrize('name,value,command', [('CUE',0,'@GF HP 0\n'),('CUE',1,'@GF HP 127\n'),
                                               ('IN',0,'@GF IN 0\n'),('IN',1,'@GF IN 15\n')])
def test_cdc_pot_contract(name,value,command):
    assert pot_command(name,value) == command
    with pytest.raises(ValueError): pot_command('MASTER',value)


def test_jog_pages_presets_buttons_and_scale(gui):
    surface, synth, batcher = enter(gui)
    before = synth.pageName
    surface.receive(Jog(1))
    batcher.drain_frame()
    assert synth.pageName != before
    surface.receive(Boton('SEARCH', True))
    batcher.drain_frame()
    assert surface.console.screenName == 'SynthPresets'
    surface.receive(Jog(1))
    batcher.drain_frame()
    assert synth.presetIndex == min(1,len(synth._presets)-1)
    surface.receive(Boton('OCT_PLUS', True))
    batcher.drain_frame()
    assert surface.octave == 4
    surface.console.navigate('SynthScale')
    surface.receive(Encoder(0, 1))
    surface.receive(Encoder(1, 1))
    batcher.drain_frame()
    assert surface.root == 10 and surface.scale == 'Major'
    assert surface.console.synthMode


def test_socket_notes_sent_without_qt_event_loop_or_ack(gui):
    surface, synth, batcher = enter(gui)
    sender, receiver = socket.socketpair()
    notes = SocketNotes('', transport=sender)
    surface.notes = notes
    receiver.settimeout(.5)
    try:
        surface.fakePad(24, True)
        raw = receiver.recv(4096)
        command = json.loads(raw)
        assert command['cmd'] == 'note' and command['part'] == 1 and command['velocity'] == 100
        # No processEvents, drain_frame or ACK was needed before note-off either.
        surface.fakePad(24, False)
        assert json.loads(receiver.recv(4096))['velocity'] == 0
        assert batcher.pending == 2
    finally:
        surface.notes = PreviewNotes()
        notes.close()
        receiver.close()


def test_midi_note_channel_velocity_and_external_sustain(gui, monkeypatch):
    import mido
    sent=[]
    monkeypatch.setattr(mido,'get_output_names',lambda:['host MIDI'])
    monkeypatch.setattr(mido,'open_output',lambda name:SimpleNamespace(send=sent.append,close=lambda:None))
    surface,synth,_ = enter(gui)
    surface.notes = MidiNotes('host')
    surface.external('keyboard',mido.Message('note_on',channel=7,note=65,velocity=99))
    surface.external('keyboard',mido.Message('control_change',control=64,value=127))
    synth.selectPart(3)
    surface.external('keyboard',mido.Message('control_change',control=64,value=0))
    surface.external('keyboard',mido.Message('note_off',channel=7,note=65))
    assert [(m.type,m.channel) for m in sent] == [('note_on',0),('control_change',0),('control_change',0),('note_off',0)]
    assert sent[0].velocity == 99 and sent[-1].note == 65


def test_led_output_roots_color_dedup_and_channel_isolation(gui):
    surface, synth, _ = enter(gui)
    sent=[]
    surface.led_port = SimpleNamespace(send=sent.append,close=lambda:None)
    surface.update_leds()
    assert len(sent) == 340
    # Physical non-root colors match the selected BASS card (#3D8CFF).
    assert [m.value for m in sent[13:16]] == [30,70,127]
    assert all(m.channel == 15 for m in sent)
    assert all(decodificar(m) is None for m in sent)
    surface.update_leds()
    assert len(sent) == 340
    surface.setRoot(0)
    # Scale degrees stay at the same physical positions when root changes.
    assert len(sent) == 340
    synth.selectPart(2)
    surface.led_output.last_send -= .034
    surface.update_leds()
    assert len(sent) > 340
    assert led_messages(24,(127,127,127))[-1] == dict(type='note_on',channel=15,note=72,velocity=1)
    surface.led_port = None


def test_port_selection_and_disconnect_releases_notes(gui, monkeypatch):
    import mido
    surface, synth, _ = enter(gui)
    names=['GF Consola input','Keyboard input']
    callbacks={}
    def opened(name, callback):
        callbacks[name]=callback
        return SimpleNamespace(name=name,close=lambda:None)
    monkeypatch.setattr(mido,'get_input_names',lambda:list(names))
    monkeypatch.setattr(mido,'open_input',opened)
    surface.open_ports('GF Consola',['Keyboard'],led=False)
    callbacks['GF Consola input'](mido.Message('note_on',note=72,velocity=127))
    assert surface.refs
    names.clear()
    surface.check_health()
    assert not surface.refs and surface.notes.events[-1][-1] == 0
    assert 'disconnected' in surface.status


def test_fake_keyboard_shortcuts_press_release_no_repeat(app):
    import shiboken6
    from main import create_engine
    engine,state=create_engine(fake_surface=True)
    try:
        window=engine.rootObjects()[0]
        state.triggerAction('mode:2')
        window.requestActivate()
        QTest.qWait(20)
        QTest.keyPress(window,Qt.Key_Z)
        assert len(engine.synth_surface.notes.events) == 1
        QTest.keyRelease(window,Qt.Key_Z)
        assert engine.synth_surface.notes.events[-1][-1] == 0
    finally:
        engine.synth_surface.close()
        shiboken6.delete(engine)
        app.processEvents()


def test_octave_and_part_buttons_affect_music_before_next_frame(gui):
    surface,synth,batcher=enter(gui)
    surface.console.navigate('SynthParts')
    surface.setRoot(0)
    surface.receive(Boton('OCT_PLUS',True))
    surface.receive(Boton('ENCODER_2',True))
    surface.fakePad(24,True)
    assert surface.notes.events[-1] == (2,60,100)
    batcher.drain_frame()
    assert surface.octave==4 and synth.selectedPart==2
    surface.fakePad(24,False)
    assert surface.notes.events[-1] == (2,60,0)


def test_slow_room_worker_uses_existing_cdc_commands_and_room_node(monkeypatch):
    writes=[]
    commands=[]
    class Serial:
        def __init__(self,*a,**k): pass
        def write(self,data): writes.append(data)
        def read_until(self,*a,**k): return b'OK\n'
        def close(self): pass
    monkeypatch.setitem(sys.modules,'serial',SimpleNamespace(Serial=Serial))
    monkeypatch.setattr('synth_surface.subprocess.run',lambda cmd,**kw:commands.append(cmd))
    room=RoomControls('/dev/explicit-gf','audio-plus-room')
    room.submit('MASTER',.5)
    room.submit('CUE',1)
    room.submit('IN',1)
    room.close()
    assert commands == [['wpctl','set-volume','audio-plus-room','0.5000']]
    assert writes == [b'@GF HP 127\n',b'@GF IN 15\n']
    assert not room.error


def test_common_surface_settings_do_not_edit_stale_synth_bank(gui):
    surface,synth,batcher=enter(gui)
    surface.console.navigate('Settings')
    before=dict(synth.values)
    surface.receive(Encoder(0,1))
    batcher.drain_frame()
    assert synth.values==before and not synth._outbound
    assert surface.console.synthMode


@pytest.mark.parametrize('engine',['prophet','dx7','dx7_6op','juno','ob6','model_d','arp2600','hybrid'])
def test_sharp_macros_cover_all_host_engines_including_both_hybrid_filters(gui,engine):
    surface,synth,batcher=enter(gui)
    for i in range(3): surface.console.preferences.toggleHand(i)
    surface.receive(ManoPresencia(0, True))
    surface.receive(ManoPresencia(1, True))
    synth.changeEngine(engine)
    surface.receive(Mano(0,1))
    surface.receive(Mano(1,1))
    batcher.drain_frame()
    metas={p['id']:p for pg in synth.current_pages for bank in pg['banks'] for p in bank}
    for id in ('a.cutoff','b.cutoff','move') if engine=='hybrid' else ('cutoff','move'):
        assert synth.values[id]==metas[id]['max']


def test_note_transport_prefers_midi_and_announces_socket_fallback(monkeypatch):
    import synth_surface as module
    midi, fallback=object(),object()
    monkeypatch.setattr(module,'MidiNotes',lambda name:midi)
    monkeypatch.setattr(module,'SocketNotes',lambda path:fallback)
    assert module.open_note_output('auto','host','socket')==(midi,'MIDI NOTES')
    def absent(name): raise ValueError('native graph port not exposed')
    monkeypatch.setattr(module,'MidiNotes',absent)
    sink,description=module.open_note_output('auto','host','socket')
    assert sink is fallback and 'MIDI unavailable' in description
    with pytest.raises(ValueError): module.open_note_output('midi','host','socket')
    assert module.open_note_output('socket','host','socket')[0] is fallback


def test_auto_external_ports_exclude_surface_through_host_and_gui():
    from synth_surface import external_input_names
    names = ['GF Consola:0', 'Midi Through:0', 'gf-synth-host:midi_in',
             'gf-synth-gui:0', 'RtMidi Input Client:0', 'Arturia MiniLab:0',
             'Other keyboard:0', 'GF Superficie:0']
    assert external_input_names(names, ['auto'], names[0]) == names[5:7]
    assert external_input_names(names, ['MiniLab'], names[0]) == [names[5]]
    assert external_input_names(names, ['none'], names[0]) == []
    assert external_input_names(names, ['auto', 'MiniLab'], names[0]) == names[5:7]
    assert external_input_names(names[:5], ['auto'], names[0]) == []
    assert external_input_names(names + ['Custom host:0'], ['auto'], names[0], ('Custom host:0',)) == names[5:7]
    with pytest.raises(ValueError):
        external_input_names(names, ['keyboard', 'missing'], names[0])


def test_minimal_fsr_and_sharp_wire_messages_reach_synth(gui):
    import mido
    surface, synth, batcher = enter(gui)
    for i in range(3): surface.console.preferences.toggleHand(i)
    surface.receive(ManoPresencia(0, True))
    surface.receive(ManoPresencia(1, True))
    for note, pitch in ((36, 36), (37, 38)):
        surface.receive(decodificar(mido.Message('note_on', channel=0, note=note, velocity=87)))
        assert surface.notes.events[-1] == (1, pitch, 87)
        surface.receive(decodificar(mido.Message('note_off', channel=0, note=note, velocity=0)))
        assert surface.notes.events[-1] == (1, pitch, 0)
    synth.selectPage('FILTER')
    surface.receive(decodificar(mido.Message('note_off', note=108)))
    batcher.drain_frame()
    before = synth.values['cutoff']
    surface.receive(decodificar(mido.Message('control_change', control=33, value=127)))
    batcher.drain_frame()
    assert synth.values['cutoff'] == before
    surface.receive(decodificar(mido.Message('note_on', note=108, velocity=127)))
    surface.receive(decodificar(mido.Message('control_change', control=33, value=127)))
    batcher.drain_frame()
    meta = next(p for page in synth.current_pages for bank in page['banks'] for p in bank if p['id'] == 'cutoff')
    assert synth.values['cutoff'] == meta['max']


def test_run_synth_external_environment_default_and_explicit_override(tmp_path):
    import os
    import shutil
    import subprocess
    script = Path(__file__).resolve().parents[1] / 'pi' / 'run_synth.sh'
    shutil.copy(script, tmp_path / script.name)
    (tmp_path / 'run_gui.sh').write_text('printf "%s\\n" "$@"\n')
    # Exercise the normal-priority wrapper on macOS as well as Linux.
    bin_dir = tmp_path / 'bin'
    bin_dir.mkdir()
    chrt = bin_dir / 'chrt'
    chrt.write_text('#!/bin/sh\n[ "$1" = --other ] && [ "$2" = 0 ] || exit 99\nshift 2\nexec "$@"\n')
    chrt.chmod(0o755)
    env = dict(os.environ, XDG_RUNTIME_DIR=str(tmp_path), PATH=f'{bin_dir}:/usr/bin:/bin')
    env.pop('GF_EXTERNAL_MIDI_PORT', None)
    def run(*args):
        return subprocess.check_output(['bash', str(tmp_path / script.name), *args], env=env, text=True).splitlines()
    args = run()
    assert args[args.index('--external-midi-port')+1] == 'auto'
    env['GF_EXTERNAL_MIDI_PORT'] = 'Arturia MiniLab'
    args = run()
    assert args[args.index('--external-midi-port')+1] == 'Arturia MiniLab'
    args = run('--external-midi-port', 'Other keyboard')
    assert args.count('--external-midi-port') == 1
    assert args[args.index('--external-midi-port')+1] == 'Other keyboard'
