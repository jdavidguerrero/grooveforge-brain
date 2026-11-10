"""Real host + decoded assets + background worker, with inherited Unix sockets in restricted sandboxes."""
from copy import deepcopy
from hashlib import sha256
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time
import wave

REPO = Path(__file__).resolve().parents[3]
# The kit worker needs numpy/scipy/soundfile: use the tools/drum-kits uv venv when present (`uv sync` there).
KIT_PYTHON = str(REPO/'tools/drum-kits/.venv/bin/python') if (REPO/'tools/drum-kits/.venv/bin/python').exists() else sys.executable
sys.path.insert(0, str(REPO/'tools/synth-host'))
sys.path.insert(0, str(REPO/'tools/drum-kits'))
from ctl import Client
def empty_rack(name):
    rack = json.loads((Path(sys.argv[1]).resolve().parent/"drum-metadata.json").read_text())["empty_rack"]
    rack["name"] = name
    return rack

def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value))


def main(binary):
    with tempfile.TemporaryDirectory(prefix='gf-drum-contract-', dir='/tmp') as directory:
        root = Path(directory)
        data = root/'data'
        source = root/'source'
        source.mkdir()
        (data/'samples').mkdir(parents=True)
        rack = empty_rack('Own samples')
        rack['sources'] = [dict(id='own', kind='IMPORTED', name='Own fixtures', licence_note='pending')]
        for k, width in enumerate((2, 3, 4)):
            path = source/f'kick-{width}.wav'
            with wave.open(str(path), 'wb') as f:
                f.setnchannels(1)
                f.setsampwidth(width)
                f.setframerate(24000)
                f.writeframes(b''.join(int(1000 if width == 2 else 100000).to_bytes(width, 'little', signed=True) for _ in range(200)))
            digest = sha256(path.read_bytes()).hexdigest()
            target = data/'samples'/(digest+'.wav')
            target.write_bytes(path.read_bytes())
            rack['pads'][k].update(name=f'Own PCM {width}', source_id='own', layers=[dict(path=f'samples/{digest}.wav', hash=digest, velocity_min=1, velocity_max=127)])
        atomic_json(data/'racks/own.json', rack)
        log = open(root/'host.log', 'w+')
        process = None
        def start():
            parent, child = socket.socketpair()
            args = [binary, '--backend', 'offline', '--presets', str(root/'presets'), '--drum-data', str(data),
                    '--drum-kit-python', KIT_PYTHON, '--drum-kit-script', str(REPO/'tools/drum-kits/drum_kits.py'),
                    '--control-fd', str(child.fileno())]
            p = subprocess.Popen(args, pass_fds=(child.fileno(),), stdout=log, stderr=log)
            child.close()
            return p, Client.from_socket(parent)
        try:
            process, client = start()
            assert client.request('hello', client='drum-test')['drum_version'] == 1
            assert not client.request('get_drum_state')['clock']['running']
            assert client.request('list_racks')['racks'][0]['name'] == 'Own samples'
            assert len(client.request('list_pad_params')['pages']) == 4
            offset=0;catalog=[]
            while True:
                page=client.request('list_drum_samples',offset=offset,limit=1)
                assert len(page['samples'])==1
                catalog.extend(page['samples'])
                assert page['next_offset']==offset+1
                if not page['has_more']:break
                offset=page['next_offset']
            assert len(catalog)==3 and len({s['layer']['hash'] for s in catalog})==3
            client.request('load_rack', path='racks/own.json')
            state = client.request('get_drum_state')
            assert state['ram_bytes'] == 3*400*4
            assert state['rack']['sources'][0]['licence_note'] == 'pending'
            client.request('pad_trigger', pad=0, velocity=127)
            client.request('preview', pad=1)
            client.request('preview', stop=True)
            assert client.request('set_pad_param', pad=0, param='tune', value=100)['value'] == 24
            client.request('set_group', group=0, level=.25, name='LOW')
            steps = deepcopy(rack['patterns'][0]['lanes'][0]['steps'])
            steps[0]['velocity'] = 100
            client.request('set_steps', pattern=0, lane=0, length=7, steps=steps)
            client.request('record_arm', armed=True, quantize=32)
            client.request('transport', action='play', bpm=122)
            assert client.request('get_drum_state')['clock']['running']
            client.request('set_pattern', pattern=2)
            state = client.request('get_drum_state')
            assert state['clock']['queued'] == 2 and state['clock']['pattern'] == 0
            try:
                client.request('transport', action='play', bpm=123)
                raise AssertionError('BPM edit while running was accepted')
            except RuntimeError:
                pass
            client.request('save_rack', name='Saved')
            try:
                client.request('load_rack', path='../source/kick-2.wav')
                raise AssertionError('path escape accepted')
            except RuntimeError:
                pass
            broken = deepcopy(rack)
            broken['pads'][0]['layers'][0]['hash'] = '0'*64
            atomic_json(data/'racks/bad.json', broken)
            try:
                client.request('load_rack', path='racks/bad.json')
                raise AssertionError('hash mismatch accepted')
            except RuntimeError:
                pass
            assert client.request('get_drum_state')['rack']['name'] == 'Saved'
            client.request('subscribe', topics=['drum'])
            # Eight encoder banks can edit in one frame without huge rack snapshots/backpressure.
            ids=[]
            for k in range(32):
                client.sequence+=1;ids.append(client.sequence)
                client.send(dict(id=client.sequence,cmd='set_pad_param',pad=k%16,param='pan',value=.1))
            replies=set();deltas=0
            while len(replies)<32:
                event=client.receive()
                if event.get('id') in ids:
                    assert event['ok'];replies.add(event['id'])
                elif event.get('event')=='drum':
                    assert event.get('kind')=='pad_param' and 'rack' not in event
                    deltas+=1
            assert deltas>=31
            client.request('subscribe', topics=['drum_job'])
            job = client.request('import_folder', path=str(source), licence_note='Juan — pending', trim=True)
            done = None
            deadline = time.monotonic()+60  # first run imports numpy/scipy in the worker (~6 s cold)
            while time.monotonic() < deadline:
                event = client.receive(timeout=60)
                if event.get('event') == 'drum_job' and event.get('job_id') == job['job_id']:
                    assert event.get('status') != 'error', event
                    if event.get('status') == 'done':
                        done = event
                        break
            assert done and done['imported'] == 3
            assert client.request('get_drum_state')['rack']['name'] == 'Saved', 'import changed active rack'
            client.request('subscribe', topics=[])
            client.request('transport', action='stop')
            client.request('load_rack',path=done['racks'][0])
            client.request('pad_trigger',pad=0,velocity=127)
            client.request('preview',pad=0)  # Service-generated float32 WAV is decoded by the actual host.
            client.request('preview',stop=True)
            client.request('load_rack',path='racks/Saved.json')
            client.request('set_pattern',pattern=4)
            client.request('save_rack',name='Saved',overwrite=True)
            process.terminate()
            process.wait(timeout=5)
            assert process.returncode == 0
            client.close()
            process, client = start()
            state = client.request('get_drum_state')
            assert not state['clock']['running'] and not state['clock']['armed'] and state['clock']['queued'] == -1
            assert state['clock']['pattern']==4
            assert state['rack']['pads'][0]['params']['tune'] == 24
            assert state['rack']['patterns'][0]['lanes'][0]['length'] == 7
            assert state['rack']['groups'][0]['level'] == .25
            client.close()
            process.terminate()
            process.wait(timeout=5)
            assert process.returncode == 0
            print('DRUM contract: real PCM16/24/32 decode/resample, RAM, commands/ACK, path/hash rejection, background import/licence, checkpoint/restart stopped: PASS')
        except Exception:
            log.flush()
            log.seek(0)
            print(log.read(), file=sys.stderr)
            raise
        finally:
            if process and process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            log.close()


if __name__ == '__main__':
    main(str(Path(sys.argv[1]).resolve()))
