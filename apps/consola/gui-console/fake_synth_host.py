#!/usr/bin/env python3
"""Hardware-free JSON-lines development host. No audio; presets live in memory."""
import argparse
from copy import deepcopy
import json
import math
import os
from pathlib import Path
import signal
import socketserver
import threading
import time
import sys

from synth_catalog import ENGINES, METADATA, all_params, list_pages


def socket_path():
    return str(Path(os.environ.get('XDG_RUNTIME_DIR', '/tmp'))/'gf-synth.sock')


class FakeSynth:
    def __init__(self):
        self.parts = [dict(part=i, engine=eng, preset=name, role=role, level_db=level, mute=False, modified=False)
                      for i, eng, name, role, level in [
                          (1,'model_d','Moog Round','BASS',-2.6), (2,'dx7','E-Piano Dusk','CHORDS',-4.2),
                          (3,'juno','Kalimba Glass','LEAD',-1.2), (4,'hybrid','Woodglass','PAD',-6.)]]
        self.values = {p['part']: {id:m['default'] for id,m in all_params(p['engine']).items()} for p in self.parts}
        self.presets = []
        self.saved = {}
        for p in self.parts:
            self.presets.append(dict(name=p['preset'], engine=p['engine'], tags=[p['role'],'SUS'], favorite=True))
        for eng in ENGINES:
            for role, suffix, tag in [('BASS','Sub Dub','SUB'), ('CHORDS','Warm Keys','PLK'),
                                      ('LEAD','Glass Bells','AIR'), ('PAD','Slow Air','PAD')]:
                self.presets.append(dict(name=f'{suffix} · {eng}', engine=eng, tags=[role,tag], favorite=False))
        self.lock = threading.RLock()

    def request(self, req):
        """Return response and publishable events using exactly contract-14 fields."""
        with self.lock:
            try:
                result, events = self._request(req)
                return dict(id=req.get('id'), ok=True, **deepcopy(result)), deepcopy(events)
            except (ValueError, KeyError, TypeError, IndexError) as exc:
                return dict(id=req.get('id'), ok=False, error=str(exc)), []

    def _request(self, r):
        if not isinstance(r.get('id'), int):
            raise ValueError('integer id required')
        cmd = r['cmd']
        if cmd == 'hello':
            if not isinstance(r['client'], str):
                raise ValueError('client must be text')
            return dict(version='fake-1',sample_rate=48000,block=128,engines=list(ENGINES)), []
        if cmd == 'get_state':
            return dict(parts=self.parts), []
        if cmd == 'list_params':
            return dict(pages=list_pages(r['engine'])), []
        if cmd == 'list_presets':
            return dict(presets=[p for p in self.presets if all(
                not r.get(key) or (r[key] == p['engine'] if key == 'engine' else r[key] in p['tags'])
                for key in ('engine','role','tag'))]), []
        if cmd == 'subscribe':
            if not isinstance(r['topics'],list) or set(r['topics']) - {'meters','state','cpu'}:
                raise ValueError('unsupported topic')
            return {}, []
        number = r['part']
        if type(number) is not int or not 1 <= number <= 4:
            raise ValueError('part must be 1–4')
        part = self.parts[number-1]
        if cmd == 'get_params':
            allowed = None
            if 'page' in r:
                pg = next((p for p in list_pages(part['engine']) if p['name'] == r['page']), None)
                if pg is None:
                    raise ValueError('unknown page')
                allowed = {p['id'] for bank in pg['banks'] for p in bank}
            return dict(values={k:v for k,v in self.values[number].items() if allowed is None or k in allowed}), []
        if cmd == 'set_param':
            meta = all_params(part['engine'])[r['param']]
            value = float(r['value'])
            if not math.isfinite(value):
                raise ValueError('value must be finite')
            value = max(meta['min'],min(meta['max'],value))
            if meta['kind'] in ('enum','bool'):
                value = meta['min'] + math.floor((value-meta['min'])/meta['step']+.5)*meta['step']
            if r['param'] in METADATA['integer_params'][part['engine']]:
                value = math.floor(value+.5) if value >= 0 else math.ceil(value-.5)
            self.values[number][r['param']] = value
            part['modified'] = True
            return dict(part=number,param=r['param'],value=value), [
                dict(event='param',part=number,param=r['param'],value=value), dict(event='state',**part)]
        if cmd == 'set_engine':
            values = {k:p['default'] for k,p in all_params(r['engine']).items()}
            part.update(engine=r['engine'],preset='Init',modified=False)
            self.values[number] = values
        elif cmd == 'load_preset':
            preset = next((p for p in self.presets if p['name'] == r['name']),None)
            if preset is None:
                raise ValueError('preset not found')
            part.update(engine=preset['engine'],preset=preset['name'],modified=False)
            self.values[number] = deepcopy(self.saved.get(preset['name'],
                {k:p['default'] for k,p in all_params(preset['engine']).items()}))
        elif cmd == 'save_preset':
            name = r['name']
            if not isinstance(name,str) or not name.strip():
                raise ValueError('preset name required')
            if any(p['name'] == name for p in self.presets):
                raise ValueError('preset already exists')
            self.presets.append(dict(name=name,engine=part['engine'],tags=[part['role']],favorite=False))
            self.saved[name] = deepcopy(self.values[number])
            part.update(preset=name,modified=False)
        elif cmd == 'set_level':
            db = float(r['db'])
            if not math.isfinite(db):
                raise ValueError('db must be finite')
            part['level_db'] = max(-60,min(12,db))
        elif cmd == 'set_mute':
            if type(r['mute']) is not bool:
                raise ValueError('mute must be boolean')
            part['mute'] = r['mute']
        elif cmd == 'note':
            if not (type(r['note']) is int and type(r['velocity']) is int and
                    0 <= r['note'] <= 127 and 0 <= r['velocity'] <= 127):
                raise ValueError('note and velocity must be MIDI 0–127')
            return {}, []
        else:
            raise ValueError('unknown command')
        return dict(part=part), [dict(event='state', **part)]

    def meters(self, tick=0):
        parts = [dict(peak_db=-60 if p['mute'] else p['level_db']-3-abs(math.sin(tick+i))*6,
                      rms_db=-60 if p['mute'] else p['level_db']-12) for i,p in enumerate(self.parts)]
        return dict(event='meters',parts=parts,mix=dict(peak_db=-3.2,rms_db=-12.4))


class _Connection(socketserver.StreamRequestHandler):
    def setup(self):
        super().setup()
        self.topics = set()
        self.send_lock = threading.Lock()
        with self.server.clients_lock:
            self.server.clients.add(self)

    def send(self, message):
        try:
            with self.send_lock:
                self.wfile.write((json.dumps(message,ensure_ascii=False,allow_nan=False)+'\n').encode())
                self.wfile.flush()
        except (OSError,ValueError):
            pass

    def handle(self):
        while not self.server.stopping.is_set():
            line = self.rfile.readline(1024*1024)
            if not line:
                break
            try:
                req = json.loads(line)
                if not isinstance(req,dict):
                    raise ValueError('object required')
            except (ValueError,UnicodeError):
                self.send(dict(id=None,ok=False,error='invalid JSON object'))
                continue
            if self.server.delay:
                self.server.stopping.wait(self.server.delay)
            if req.get('cmd') == 'load_preset':
                self.server.stopping.wait(self.server.load_delay)
            response, events = self.server.synth.request(req)
            if req.get('cmd') == 'subscribe' and response['ok']:
                self.topics = set(req['topics'])
            self.send(response)
            for event in events:
                self.server.publish(event)

    def finish(self):
        with self.server.clients_lock:
            self.server.clients.discard(self)
        super().finish()


class FakeSynthServer(socketserver.ThreadingUnixStreamServer):
    daemon_threads = True

    def __init__(self, path, delay=0, load_delay=.12):
        self.synth = FakeSynth()
        self.clients = set()
        self.clients_lock = threading.Lock()
        self.stopping = threading.Event()
        self.delay = delay
        self.load_delay = load_delay
        self._owns_socket = False
        # Never unlink an existing socket: it may be a running real host.
        super().__init__(str(path),_Connection)

    def server_bind(self):
        super().server_bind()
        self._owns_socket = True

    def publish(self, event):
        topic = 'state' if event['event'] == 'param' else event['event']
        with self.clients_lock:
            clients = tuple(self.clients)
        for client in clients:
            if topic in client.topics:
                client.send(event)

    def event_loop(self):
        tick = 0
        while not self.stopping.wait(1/30):
            self.publish(self.synth.meters(tick/30))
            if tick % 30 == 0:
                self.publish(dict(event='cpu',mean_pct=21.0,max_pct=24.0,xruns=0))
            tick += 1

    def server_close(self):
        self.stopping.set()
        with self.clients_lock:
            clients = tuple(self.clients)
        for client in clients:
            try:
                client.request.shutdown(2)
            except OSError:
                pass
        super().server_close()
        if self._owns_socket:
            Path(self.server_address).unlink(missing_ok=True)
            self._owns_socket = False



def serve_stdio(delay=0, load_delay=.12):
    """Pipe-only protocol harness for sandboxes that prohibit AF_UNIX binding."""
    synth = FakeSynth()
    topics = set()
    lock = threading.Lock()
    stopping = threading.Event()
    def send(message):
        with lock:
            print(json.dumps(message, ensure_ascii=False, allow_nan=False), flush=True)
    def events():
        tick = 0
        while not stopping.wait(1/30):
            if 'meters' in topics:
                send(synth.meters(tick/30))
            if tick % 30 == 0 and 'cpu' in topics:
                send(dict(event='cpu',mean_pct=21.,max_pct=24.,xruns=0))
            tick += 1
    threading.Thread(target=events,daemon=True).start()
    print('FAKE synth host (pipe protocol harness, no audio)',file=sys.stderr,flush=True)
    try:
        for line in sys.stdin.buffer:
            try:
                req = json.loads(line)
                if not isinstance(req,dict):
                    raise ValueError('object required')
            except (ValueError,UnicodeError):
                send(dict(id=None,ok=False,error='invalid JSON object'))
                continue
            if delay:
                stopping.wait(delay)
            if req.get('cmd') == 'load_preset':
                stopping.wait(load_delay)
            response, updates = synth.request(req)
            if req.get('cmd') == 'subscribe' and response['ok']:
                topics.clear()
                topics.update(req['topics'])
            send(response)
            if 'state' in topics:
                for update in updates:
                    send(update)
    finally:
        stopping.set()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--socket',default=socket_path())
    parser.add_argument('--stdio',action='store_true',help='Pipe protocol test harness; production uses the Unix socket')
    parser.add_argument('--load-delay-ms',type=float,default=120,help='Simulated preset load latency')
    parser.add_argument('--delay-ms',type=float,default=0,help='Fault injection: delay every reply')
    args = parser.parse_args()
    if args.stdio:
        serve_stdio(args.delay_ms/1000,args.load_delay_ms/1000)
        return
    with FakeSynthServer(args.socket,args.delay_ms/1000,args.load_delay_ms/1000) as server:
        worker = threading.Thread(target=server.serve_forever,daemon=True)
        worker.start()
        threading.Thread(target=server.event_loop,daemon=True).start()
        for sig in (signal.SIGTERM,signal.SIGINT):
            signal.signal(sig,lambda *_: server.stopping.set())
        print(f'FAKE synth host (no audio): {args.socket}',flush=True)
        server.stopping.wait()
        server.shutdown()
        worker.join(timeout=2)


if __name__ == '__main__':
    main()
