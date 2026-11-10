"""DRUM behavior and auxiliary touch; no physical latency claims."""
from copy import deepcopy
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time
from types import SimpleNamespace

import pytest
from PySide6.QtCore import QObject, Signal, QTimer, Qt, QPoint, QPointF
from PySide6.QtNetwork import QLocalSocket
from PySide6.QtTest import QTest
import shiboken6
from main import ROOT,create_engine
from drum_client import DrumClient
from drum_state import DRUM_SCREENS
from drum_surface import DrumSocketNotes
from fake_drum_host import FakeDrum
from synth_surface import PreviewNotes
from superficie import Pad,Fsr,Encoder,Fader,Jog,Boton,Mano,ManoPresencia,decodificar


def enter(gui):
    engine,state,window,messages=gui
    state.triggerAction('mode:3')
    engine.surface_batcher.drain_frame()
    return engine,engine.drum_state,engine.drum_surface,engine.surface_batcher


def test_mode_select_fourth_entry_and_immediate_percussion(gui):
    e,d,s,b=enter(gui)
    assert e.console_state.drumMode and e.console_state.screenName=='DrumRack'
    e.synth_surface.receive(Pad(7,True,.5))
    assert s.notes.events==[(10,7,64)] and b.pending==1
    assert e.synth_surface.notes.events==[]
    e.synth_surface.receive(Fsr(0,True,1.))
    assert s.notes.events[-1]==(10,0,127)
    e.synth_surface.receive(Fsr(1,True,.4))
    e.synth_surface.receive(Fsr(2,True,.7))
    e.synth_surface.receive(Fsr(3,True,.8))
    assert [event[1] for event in s.notes.events[-3:]]==[1,10,13]
    b.drain_frame()
    assert d.fsr[0]['velocity']==127
    before=len(s.notes.events)
    e.synth_surface.receive(Pad(7,False,0))
    assert len(s.notes.events)==before
    e.console_state.triggerAction('mode:2')
    e.synth_surface.receive(Pad(0,True,1))
    assert len(s.notes.events)==before
    assert e.synth_surface.notes.events


def test_grid_bottom_steps_page_polymeter_and_encoder_layers(gui):
    e,d,s,b=enter(gui)
    d.selectPad(4)
    p=d._clock['pattern']
    before=d._rack['patterns'][p]['lanes'][4]['steps'][0]['velocity']
    e.synth_surface.receive(Pad(16,True,1))
    assert s.notes.events==[]
    b.drain_frame()
    assert bool(d._rack['patterns'][p]['lanes'][4]['steps'][0]['velocity']) is not bool(before)
    e.console_state.navigate('DrumSteps')
    d.selectStep(4,3)
    d.setLayer('PROB')
    e.synth_surface.receive(Encoder(2,-20))
    e.synth_surface.receive(Encoder(3,20))
    b.drain_frame()
    assert d._rack['patterns'][p]['lanes'][4]['length']==32
    assert d._rack['patterns'][p]['lanes'][4]['steps'][3]['probability']==pytest.approx(.45)
    d.action('step-page')
    e.synth_surface.receive(Pad(31,True,1))
    b.drain_frame()
    assert d._rack['patterns'][p]['lanes'][4]['steps'][31]['velocity']==100


def test_host_parameters_frame_coalescing_groups_and_presence(gui):
    e,d,s,b=enter(gui)
    calls=[]
    request=d.request
    d.request=lambda cmd,**f:(calls.append((cmd,f)),request(cmd,**f))[1]
    e.synth_surface.receive(Encoder(0,2))
    e.synth_surface.receive(Encoder(0,3))
    assert not calls
    b.drain_frame()
    edits=[f for c,f in calls if c=='set_pad_param']
    assert len(edits)==1 and edits[0]['value']==5
    e.synth_surface.receive(Fader(1,.25))
    e.synth_surface.receive(Mano(0,1))
    b.drain_frame()
    assert d._rack['groups'][1]['level']==.5
    assert d.params['cutoff']==20000
    e.synth_surface.receive(ManoPresencia(0,True))
    e.synth_surface.receive(Mano(0,0))
    b.drain_frame()
    assert d.params['cutoff']==20
    d.regroup(0);b.drain_frame()
    assert d.params['group']==1


def test_transport_record_and_queued_pattern_are_host_confirmed(gui):
    e,d,s,b=enter(gui)
    assert not d.clock['running']
    e.synth_surface.receive(Boton('PLAY',True));b.drain_frame()
    assert d.clock['running']
    d.selectPattern(3);b.drain_frame()
    assert d.clock['pattern']==0 and d.clock['queued']==3
    e.synth_surface.receive(Boton('RECORD',True));b.drain_frame()
    assert d.clock['armed']
    e.synth_surface.receive(Boton('STOP',True));b.drain_frame()
    assert not d.clock['running'] and not d.clock['armed'] and d.clock['queued']==-1


def test_browser_cue_only_keep_variation_preserves_steps_and_levels(gui):
    e,d,s,b=enter(gui)
    e.console_state.navigate('DrumBrowser');b.drain_frame()
    calls=[];request=d.request;d.request=lambda cmd,**f:(calls.append((cmd,f)),request(cmd,**f))[1]
    d.auditionSound()
    assert calls[-1][0]=='preview' and 'layer' in calls[-1][1]
    assert not any(c=='pad_trigger' for c,_ in calls)
    d.selectPad(0)
    patterns=deepcopy(d._rack['patterns']);groups=deepcopy(d._rack['groups'])
    d.action('variations');b.drain_frame()
    assert len(d._variants)==8
    d.auditionVariant(2);d.keepVariant();b.drain_frame()
    assert d._rack['patterns']==patterns and d._rack['groups']==groups
    assert d._variants[2]['state']=='kept'
    assert calls[-1][0]=='set_pad_sample'
    d.selectPad(14);b.drain_frame()
    assert not d.enginePad
    assert not d.keys()[5]['enabled'] if d._screen=='DrumRack' else True


def test_decoded_physical_map_and_hold_mute(gui):
    import mido
    e,d,s,b=enter(gui)
    for note in (36,37,38,39,48,63,64,79):
        e.synth_surface.receive(decodificar(mido.Message('note_on',channel=0,note=note,velocity=97)))
    assert [v for _,_,v in s.notes.events]==[97]*6
    b.drain_frame()
    e.console_state.navigate('DrumPerform')
    e.synth_surface.receive(Boton('ENCODER_4',True));b.drain_frame()
    before=len(s.notes.events)
    e.synth_surface.receive(Pad(0,True,1));b.drain_frame()
    assert d._rack['pads'][0]['params']['mute']==1 and len(s.notes.events)==before
    e.synth_surface.receive(Boton('ENCODER_4',False));b.drain_frame()
    assert d.holdMode==''


def test_led_return_deduplicates_and_playhead(gui):
    e,d,s,b=enter(gui)
    sent=[]
    e.synth_surface.led_port=SimpleNamespace(send=sent.append)
    s.update_leds()
    assert len(sent)==128 and all(m.channel==15 for m in sent)
    assert [m.note for m in sent if m.type=='note_on']==list(range(48,80))
    s.update_leds();assert len(sent)==128
    d._clock.update(running=True,step=2,pattern_step='2');d._touch();b.drain_frame()
    s.update_leds();assert len(sent)==132
    e.synth_surface.led_port=None


def test_pad_socket_fallback_sends_drum_command_not_synth_notes():
    parent,child=socket.socketpair();messages=[]
    notes=DrumSocketNotes('/unused',transport=parent)
    try:
        notes.note(10,3,127);notes.note(10,3,0)
        data=child.recv(4096);request=json.loads(data)
        assert request==dict(id=1,cmd='pad_trigger',pad=3,velocity=127)
        child.sendall(b'{"id":1,"ok":true}\n')
    finally:
        notes.close();child.close()


def test_drum_launcher_calibrated_and_normal_scheduler():
    script=(ROOT/'pi/run_drum.sh').read_text()
    assert 'GUI_TOUCH_DEVICE:-invertx' in script and 'GUI_TOUCH_ROTATION:-0' in script
    assert '--rotate 90 --rotation-method qml' in script and 'chrt --other 0' in script
    assert '--drum-mode' in script and '--drum-socket' in script
    subprocess.run(['bash','-n',str(ROOT/'pi/run_drum.sh')],check=True)


class FakeSocket(QObject):
    connected=Signal();readyRead=Signal();disconnected=Signal();errorOccurred=Signal(object)
    def __init__(self):
        super().__init__();self.fake=FakeDrum();self.up=False;self.buffer=bytearray()
    def connectToServer(self,path): self.up=True;QTimer.singleShot(0,self.connected.emit)
    def state(self): return QLocalSocket.ConnectedState if self.up else QLocalSocket.UnconnectedState
    def write(self,payload):
        response,events=self.fake.request(json.loads(payload))
        self.buffer.extend(('\n'.join(json.dumps(m) for m in (response,*events))+'\n').encode())
        QTimer.singleShot(0,self.readyRead.emit)
    def readAll(self): data=bytes(self.buffer);self.buffer.clear();return data
    def abort(self):
        was=self.up;self.up=False
        if was:self.disconnected.emit()


def test_drum_client_handshake_commands_and_reconnect_does_not_replay(app):
    transport=FakeSocket();client=DrumClient('/fake',transport=transport)
    messages=[];client.message.connect(messages.append)
    try:
        client.start()
        for _ in range(10):QTest.qWait(5);client.drain_frame()
        assert any(m.get('_cmd')=='get_drum_state' and m.get('ok') for m in messages)
        client.request('transport',action='play')
        for _ in range(4):QTest.qWait(5);client.drain_frame()
        assert any(m.get('event')=='clock' and m['clock']['running'] for m in messages)
        client.stop();transport.fake=FakeDrum();client.start()
        for _ in range(10):QTest.qWait(5);client.drain_frame()
        assert not transport.fake.clock['running'] and client._state=='ready'
        with pytest.raises(ValueError):client.request('note',part=1,note=48,velocity=127)
    finally:client.stop()


def test_tempo_auxiliary_control_respects_stopped_transport(gui):
    e,d,s,b=enter(gui)
    d.action('tempo');b.drain_frame()
    assert d.tempoVisible
    e.synth_surface.receive(Encoder(0,4));b.drain_frame()
    assert d.clock['bpm']==126
    d.play();b.drain_frame()
    d.setBpm(130);b.drain_frame()
    assert d.clock['bpm']==126 and 'Stop' in d.error
    d.stop();d.setBpm(125);b.drain_frame()
    assert d.clock['bpm']==125
    d.action('tempo-close');b.drain_frame()
    assert not d.tempoVisible


def test_catalog_refresh_ignores_an_old_in_flight_page(gui):
    e,d,s,b=enter(gui)
    before=deepcopy(d._samples)
    d._catalog_generation=2
    d._handle(dict(id=100,ok=True,_cmd='list_drum_samples',_fields=dict(offset=64,generation=1),
                   samples=before,has_more=True,next_offset=128))
    assert d._samples==before
    d._handle(dict(id=101,ok=True,_cmd='list_drum_samples',_fields=dict(offset=0,generation=2),
                   samples=before[:2],has_more=False,next_offset=2))
    assert d._samples==before[:2]


def test_leaving_text_or_tempo_restores_normal_surface_editing(gui):
    e,d,s,b=enter(gui)
    e.console_state.navigate('DrumSave');QTest.qWait(30);b.drain_frame()
    assert d.textTarget=='save'
    d.action('nav:DrumRack');b.drain_frame()
    before=d.params['tune']
    e.synth_surface.receive(Encoder(0,1));b.drain_frame()
    assert d.params['tune']==before+1 and d.textTarget==''
    d.action('tempo');b.drain_frame();assert d.tempoVisible
    e.console_state.navigate('ModeSelect');b.drain_frame()
    assert not d.tempoVisible


def test_perform_modifier_prevents_audio_before_the_next_frame(gui):
    e,d,s,b=enter(gui)
    e.console_state.navigate('DrumPerform');b.drain_frame()
    e.synth_surface.receive(Boton('ENCODER_4',True))
    e.synth_surface.receive(Pad(0,True,1))
    e.synth_surface.receive(Boton('ENCODER_4',False))
    assert s.notes.events==[]
    b.drain_frame()
    assert d.params['mute']==1 and d.holdMode==''
    d.action('mute');b.drain_frame()
    d.action('nav:DrumRack');b.drain_frame()
    assert d.holdMode==''
