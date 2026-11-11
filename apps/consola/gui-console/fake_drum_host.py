"""Deterministic DRUM development host. No audio, samples, worker or disk mutations."""
from copy import deepcopy
import argparse
import json
import math
from pathlib import Path
import signal
import sys
import threading
from types import SimpleNamespace

from fake_synth_host import FakeSynthServer, socket_path

CAT = ('kick','snare','clap','wood','wood','skin','skin','skin','skin','skin','hat','hat','hat','perc','perc','perc')
NAMES = ('KICK','SNARE','CLAP','RIM','CLAVE','CONGA LOW','CONGA MID','CONGA HIGH','BONGO LOW','BONGO HIGH','HAT CLOSED','HAT OPEN','MARACA','WOOD TOM','SHAKER','EMPTY')
COLORS = ('#FF4560','#FF9A2E','#F5E04A','#E8F0FF')


def metadata():
    return json.loads(Path(__file__).with_name('drum-metadata.json').read_text())


def demo_rack():
    rack = metadata()['empty_rack']
    rack.update(id='preview-organic', name='Organic · A minor', root='A')
    rack['sources'] = [dict(id='engine', name='Organic engine', kind='ENGINE', licence_note='Generated locally'),
                       dict(id='import', name='Imported · preview', kind='IMPORTED', licence_note='pending')]
    for i,pad in enumerate(rack['pads']):
        group = 0 if CAT[i]=='kick' else 2 if CAT[i]=='wood' else 3 if CAT[i]=='hat' else 1
        pad.update(name=NAMES[i], category=CAT[i], source_id='engine' if i<13 else 'import')
        pad['params'].update(group=group, choke=1 if i in (10,11) else 0,
                             level=.5, decay=420 if i==0 else 340)
        if i<13:
            voices=('kick','snare','clap','rim','clave','conga_baja','conga_media','conga_alta','bongo_bajo','bongo_alto','hat_cerrado','hat_abierto','maraca')
            pad['recipe']=dict(voice=voices[i],root='A',mode='menor',genre='organic',seed=1)
        if i<15:
            pad['layers']=[dict(path=f'samples/preview-{i}.wav',hash=f'{i:064x}',velocity_min=1,velocity_max=127,
                                frames=round(48000*(.42+i*.014)),channels=1,sample_rate=48000,
                                waveform=[round(100*math.exp(-j/22)*abs(math.sin(j*(.9+i*.023)))+2,2) for j in range(64)])]
    for p in range(8):
        for lane in range(13):
            row=rack['patterns'][p]['lanes'][lane]
            if lane==4: row['length']=12
            if lane==8: row['length']=7
            for step in range(row['length']):
                on=(step%4==0) if lane==0 else step in (4,12) if lane in (1,2) else step%2==0 if lane==10 else (step+lane+p)%7==0
                row['steps'][step].update(velocity=127 if on and step%4==0 else 86 if on else 0,
                                         probability=.65 if lane==4 else 1.,micro=.08 if lane==10 else 0.)
    return rack


class FakeDrum:
    def __init__(self, demo=True):
        self.rack=demo_rack() if demo else metadata()['empty_rack']
        self.clock=dict(sample_counter='0',origin_sample='0',rev='0',running=False,armed=False,
                        pattern=0,queued=-1,bpm=122.,swing=0.,bar=1,step=0,voice_steals=0,clipped=0,dropped=0)
        self.events=[]
        self.saved={}
        self.hits=[]

    def request(self,j):
        cmd=j['cmd'];out=dict(id=j.get('id'),ok=True);events=[]
        try:
            if cmd=='hello': out.update(version='0.1.0',drum_version=1,capabilities=['drum'],sample_rate=48000,block=256)
            elif cmd=='subscribe': pass
            elif cmd=='get_drum_state': out.update(rack=deepcopy(self.rack),clock=deepcopy(self.clock),ram_bytes=18*1024**2,ram_limit=256*1024**2,data_dir='/data/drum')
            elif cmd=='list_pad_params': out['pages']=metadata()['pages']
            elif cmd=='list_racks': out['racks']=[dict(name='Organic · A minor',path='racks/preview.json',id='preview')]+[dict(name=n,path='racks/'+n+'.json',id=n) for n in self.saved]
            elif cmd=='list_drum_samples':
                out['samples']=[dict(name=p['name'],category=p['category'],source=next(s for s in self.rack['sources'] if s['id']==p['source_id']),
                                     recipe=p.get('recipe'),layer=p['layers'][0]) for p in self.rack['pads'] if p['layers']]
                offset=int(j.get('offset',0));limit=int(j.get('limit',64))
                if offset<0 or not 1<=limit<=128: raise ValueError('invalid catalog page')
                total=len(out['samples']);out['samples']=out['samples'][offset:offset+limit]
                out.update(has_more=offset+limit<total,next_offset=min(total,offset+limit))
            elif cmd=='load_rack': self.rack=deepcopy(self.saved.get(j['path'].split('/')[-1][:-5],demo_rack()))
            elif cmd=='set_pad_param':
                d=next(p for pg in metadata()['pages'] for b in pg['banks'] for p in b if p['id']==j['param'])
                value=max(d['min'],min(d['max'],j['value']))
                if d['kind']=='enum': value=round(value)
                self.rack['pads'][j['pad']]['params'][j['param']]=value;out['value']=value
            elif cmd=='set_group': self.rack['groups'][j['group']].update({k:j[k] for k in ('level','mute','name') if k in j})
            elif cmd=='set_steps': self.rack['patterns'][j['pattern']]['lanes'][j['lane']]=deepcopy(dict(length=j['length'],steps=j['steps']))
            elif cmd=='set_pattern':
                if self.clock['running']: self.clock['queued']=j['pattern']
                else: self.clock['pattern']=j['pattern']
            elif cmd=='transport':
                if self.clock['running'] and 'bpm' in j and j['bpm']!=self.clock['bpm']: raise ValueError('stop before changing BPM')
                self.clock.update({k:j[k] for k in ('bpm','swing') if k in j})
                self.rack.update({k:j[k] for k in ('bpm','swing') if k in j})
                self.clock['running']=j['action']=='play'
                if not self.clock['running']: self.clock.update(armed=False,queued=-1,bar=1,step=0)
            elif cmd=='record_arm': self.clock['armed']=j['armed']
            elif cmd=='pad_trigger': self.hits.append((j['pad'],j['velocity'],j.get('cue',False)))
            elif cmd=='preview': pass
            elif cmd=='set_pad_sample':
                pad=self.rack['pads'][j['pad']];pad['layers']=[deepcopy(j['layer'])]
                pad.update({k:j[k] for k in ('name','recipe') if k in j})
                if j.get('source'): pad['source_id']=j['source']['id']
            elif cmd in ('save_rack','save_pattern'):
                if j['name'] in self.saved and not j.get('overwrite'): raise ValueError('rack exists')
                self.saved[j['name']]=deepcopy(self.rack);out['path']='racks/'+j['name']+'.json'
            elif cmd=='generate_variations':
                pad=self.rack['pads'][j['pad']]
                if not pad.get('recipe'): raise ValueError('Engine pads only')
                out['job_id']='preview'
                variants=[dict(name=pad['name']+' '+chr(65+i),layer=deepcopy(pad['layers'][0]),recipe=deepcopy(pad['recipe']),state='ready',render=dict(ajuste=dict(decay=.8+.08*i,ataque=1.2-.08*i,brillo=.8+.06*i))) for i in range(j['count'])]
                events.append(dict(event='drum_job',job_id='preview',status='done',variants=variants,done=len(variants),total=len(variants)))
            elif cmd in ('import_folder','render_rack'): raise ValueError('PREVIEW · run the real kit worker to create assets')
            else: raise ValueError('unknown drum command')
            if cmd.startswith('set_') or cmd in ('load_rack','transport','record_arm'):
                self.clock['rev']=str(int(self.clock['rev'])+1)
                events += [dict(event='drum',rack=deepcopy(self.rack)),dict(event='clock',clock=deepcopy(self.clock))]
        except (ValueError,KeyError,IndexError,StopIteration) as exc:
            out=dict(id=j.get('id'),ok=False,error=str(exc))
        return out,events


class FakeDrumServer(FakeSynthServer):
    def __init__(self,path):
        super().__init__(path,load_delay=0)
        self.synth=FakeDrum()

    def publish(self,event):
        topic='drum' if event['event'] in ('recorded','drum_error') else event['event']
        with self.clients_lock: clients=tuple(self.clients)
        for client in clients:
            if topic in client.topics: client.send(event)

    def event_loop(self):
        import time
        last=time.monotonic()
        while not self.stopping.wait(1/30):
            now=time.monotonic();delta=now-last;last=now
            c=self.synth.clock
            c['sample_counter']=str(int(c['sample_counter'])+round(delta*48000))
            if c['running']:
                steps=(int(c['sample_counter'])-int(c['origin_sample']))*c['bpm']/(48000*15)
                c['bar'],c['step']=int(steps)//16+1,int(steps)%16
                c['pattern_step']=str(int(steps))
                if c['step']==0 and c['queued']>=0: c['pattern'],c['queued']=c['queued'],-1
            self.publish(dict(event='clock',clock=deepcopy(c)))
            # Match gf-synth-host: one clock and one meter event per 1600 samples.
            phase=int(c['sample_counter'])/48000
            pads=[max(0.,math.sin(phase*8+i))*.8 if c['running'] else 0. for i in range(16)]
            self.publish(dict(event='drum_meters',pads=pads,
                              groups=[max(pads[i::4]) for i in range(4)],room=max(pads),cue=0.))


def serve_stdio(load_delay=.4):
    """Same JSONL contract over pipes where the sandbox prohibits socket bind."""
    topics=set();lock=threading.Lock();stopping=threading.Event()
    def send(message):
        with lock: print(json.dumps(message,ensure_ascii=False),flush=True)
    def publish(message):
        if message['event'] in topics: send(message)
    server=SimpleNamespace(synth=FakeDrum(),stopping=stopping,publish=publish)
    threading.Thread(target=FakeDrumServer.event_loop,args=(server,),daemon=True).start()
    print('FAKE drum host (pipe protocol harness, no audio)',file=sys.stderr,flush=True)
    try:
        for line in sys.stdin:
            req=json.loads(line)
            if req.get('cmd')=='load_rack': stopping.wait(load_delay)
            response,events=server.synth.request(req)
            if req.get('cmd')=='subscribe' and response['ok']:
                topics.clear();topics.update(req['topics'])
            send(response)
            for event in events: publish(event)
    finally: stopping.set()


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--socket',default=socket_path())
    p.add_argument('--stdio',action='store_true')
    p.add_argument('--load-delay-ms',type=float,default=400)
    args=p.parse_args()
    if args.stdio:
        serve_stdio(args.load_delay_ms/1000);return
    with FakeDrumServer(args.socket) as server:
        worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
        threading.Thread(target=server.event_loop,daemon=True).start()
        for sig in (signal.SIGTERM,signal.SIGINT): signal.signal(sig,lambda *_:server.stopping.set())
        print(f'FAKE drum host (no audio): {args.socket}',flush=True)
        server.stopping.wait();server.shutdown();worker.join(timeout=2)


if __name__=='__main__': main()
