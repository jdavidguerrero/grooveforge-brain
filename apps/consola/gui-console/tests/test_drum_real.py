"""Qt DRUM client and immediate FSR path against the actual offline C++ audio process."""
from contextlib import contextmanager
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile

import pytest
from PySide6.QtTest import QTest
import shiboken6

from main import ROOT, create_engine
from drum_surface import DrumSocketNotes
from fake_drum_host import metadata
from synth_surface import Fsr
from test_dx7_wiring import real_host
from tools.real_synth_review import InheritedSocket, until


def test_real_host_drums_load_edit_record_cue_and_save(app,real_host,tmp_path):
    binary,_=real_host
    sys.path.insert(0,str(ROOT.parents[2]/'tools/drum-kits'))
    from drum_kits import render_engine
    data=tmp_path/'data'
    rendered=render_engine(data,root='D',mode='dorico',genre='organic')
    parent,child=socket.socketpair()
    music_parent,music_child=socket.socketpair()
    log=open(tmp_path/'drum-host.log','w+')
    process=subprocess.Popen([str(binary),'--backend','offline','--presets',str(tmp_path/'presets'),
                              '--drum-data',str(data),'--no-state','--control-fd',str(child.fileno()),
                              '--control-fd',str(music_child.fileno())],
                             pass_fds=(child.fileno(),music_child.fileno()),stdout=log,stderr=log)
    child.close();music_child.close()
    transport=InheritedSocket(parent.detach())
    engine=None
    try:
        engine,state=create_engine(drum_socket='inherited-drum-host',drum_transport=transport)
        d=engine.drum_state
        state.triggerAction('mode:3')
        until(lambda:d.status=='ready' and bool(d._pages) and not d.client.pending,d.flush_frame)
        d.loadRack(rendered['racks'][0])
        until(lambda:d._rack['root']=='D' and not d.client.pending,d.flush_frame)
        assert d._pages==metadata()['pages'] and d.ramBytes>0
        assert not d.clock['running']
        d.turn(0,3);d.setGroupLevel(2,.6);d.toggleStep(0,4);d.flush_frame()
        until(lambda:d.params['tune']==3 and abs(d._rack['groups'][2]['level']-.6)<1e-5 and
              d._rack['patterns'][0]['lanes'][0]['steps'][4]['velocity']==100 and not d.client.pending,d.flush_frame)
        d.play()
        until(lambda:d.clock['running'] and not d.client.pending,d.flush_frame)
        d.record()
        until(lambda:d.clock['armed'] and not d.client.pending,d.flush_frame)
        notes=DrumSocketNotes('/unused',transport=music_parent)
        engine.drum_surface.notes=notes
        engine.synth_surface.receive(Fsr(0,True,.8))
        assert engine.surface_batcher.pending>0  # C++ note command was already sent.
        until(lambda:any(s['velocity']==102 for s in d._rack['patterns'][0]['lanes'][0]['steps']),d.flush_frame)
        d.selectPattern(3)
        until(lambda:d.clock['queued']==3 and not d.client.pending,d.flush_frame)
        ram=d.ramBytes
        d.request('preview',layer=d._rack['pads'][0]['layers'][0])
        until(lambda:not d.client.pending,d.flush_frame)
        assert d.ramBytes>ram
        d.request('save_rack',name='Roundtrip',overwrite=False)
        until(lambda:not d.client.pending,d.flush_frame)
        saved=json.loads((data/'racks/Roundtrip.json').read_text())
        assert saved['pads'][0]['params']['tune']==3
        assert saved['patterns'][0]['lanes'][0]['steps'][4]['velocity']==100
        assert any(s['velocity']==102 for s in saved['patterns'][0]['lanes'][0]['steps'])
        d.stop()
        until(lambda:not d.clock['running'] and not d.client.pending,d.flush_frame)
        assert d.error=='' and not d.clock['armed'] and d.clock['queued']==-1
    except Exception:
        log.flush();log.seek(0);print(log.read(),file=sys.stderr)
        raise
    finally:
        if engine:
            engine.drum_surface.close();engine.drum_state.client.stop();engine.synth_surface.close()
            shiboken6.delete(engine);app.processEvents()
        else:music_parent.close()
        transport.deleteLater()
        process.terminate();process.wait(timeout=5);log.close()
