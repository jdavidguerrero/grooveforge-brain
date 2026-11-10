"""DRUM surface bridge: graph MIDI pad hits precede frame-batched UI work."""
from threading import RLock
import json
from PySide6.QtCore import QObject, Property, Signal, Slot, QTimer
from synth_surface import (MidiNotes, SocketNotes, PreviewNotes, led_messages,
                           Pad, Fsr, Boton)


class DrumSocketNotes(SocketNotes):
    def note(self,part,note,velocity):
        if not velocity: return
        with self.lock:
            if self.error: raise OSError(self.error)
            self.serial+=1
            self.sock.sendall((json.dumps(dict(id=self.serial,cmd='pad_trigger',pad=note,velocity=velocity))+'\n').encode())


def open_drum_notes(transport,midi_port,path,fake_surface=False):
    if transport=='socket' or fake_surface:
        return DrumSocketNotes(path),'SOCKET PADS'
    try:
        return MidiNotes(midi_port),'MIDI PADS · CH10'
    except (OSError,ValueError,ImportError,RuntimeError) as exc:
        if transport=='midi': raise
        return DrumSocketNotes(path),f'SOCKET PADS · MIDI unavailable: {exc}'


class DrumSurface(QObject):
    changed=Signal()
    failed=Signal(str)

    def __init__(self,console,drum,batcher,touch,shared,notes=None,parent=None):
        super().__init__(parent)
        self.console,self.drum,self.batcher,self.touch,self.shared=console,drum,batcher,touch,shared
        self.notes=notes or PreviewNotes()
        self.lock=RLock()
        self.active=False
        self._selected=0
        self._fsr=(0,1,10,4)
        self._hold=''
        self._screen=''
        self._route='PREVIEW · NO AUDIO'
        self._error=''
        self._shift=False
        self._presence=[False,False]
        self._leds={}
        self._pad_velocity=console.preferences.padVelocity
        console.preferences.changed.connect(self.preferences_changed)
        self.failed.connect(self.failure)
        self.console.changed.connect(self.publish)
        self.drum.changed.connect(self.publish)
        shared.drum_surface=self
        batcher.drum_surface=self
        self.led_timer=QTimer(self);self.led_timer.setInterval(34)
        self.led_timer.timeout.connect(self.update_leds);self.led_timer.start()
        self.publish()

    status=Property(str,lambda s:s._error or s._route,notify=changed)

    @Slot(str)
    def failure(self,error):
        self._error='SURFACE ERROR · '+error;self.changed.emit()

    @Slot()
    def publish(self):
        with self.lock:
            self.active=self.console.drumMode and self.console.screenName.startswith('Drum')
            self._selected=self.drum.selectedPad
            self._fsr=tuple(f['pad'] for f in self.drum.fsr)
            self._hold=self.drum.holdMode
            self._screen=self.console.screenName

    @Slot()
    def preferences_changed(self):
        with self.lock:
            self._pad_velocity=self.console.preferences.padVelocity
            self._presence=[present and self.console.preferences.hand_allowed(i)
                            for i,present in enumerate(self._presence)]

    def receive(self,event):
        try:
            with self.lock:
                if not self.active: return False
                if isinstance(event,Boton) and self._screen=='DrumPerform' and event.nombre in ('ENCODER_4','ENCODER_5'):
                    self._hold=('mute' if event.nombre=='ENCODER_4' else 'solo') if event.presionado else ''
                if isinstance(event,(Pad,Fsr)) and event.presionado:
                    pad=event.indice if isinstance(event,Pad) else self._fsr[event.indice]
                    if pad<16 and not self._hold:
                        self.notes.note(10,pad,self._pad_velocity if isinstance(event,Pad) else max(1,min(127,round(event.velocidad*127))))
                    if isinstance(event,Pad) and pad<16: self._selected=pad
            self.batcher.enqueue(event)
            return True
        except (OSError,ValueError,RuntimeError) as exc:
            self.failed.emit(str(exc));return True

    def apply_frame(self,kind,fields):
        if not self.console.drumMode or not self.console.screenName.startswith('Drum'): return False
        d=self.drum;i=fields.get('indice',0)
        if kind in ('Mano','ManoPresencia') and not self.console.preferences.hand_allowed(i): return True
        if kind in ('Pad','Fsr'):
            if fields['presionado']:
                if kind=='Pad' and i>=16:
                    d.toggleStep(d.selectedPad,(i-16)+d.stepPage*16)
                else:
                    pad=i if kind=='Pad' else d.fsr[i]['pad']
                    if d.holdMode: d.togglePad(pad,d.holdMode)
                    else: d.record_hit(pad,round(fields['velocidad']*127),i if kind=='Fsr' else None)
        elif kind=='Encoder': d.turn(i,fields['delta'])
        elif kind=='Fader': d.setGroupLevel(i,fields['valor']*2)
        elif kind=='Jog':
            if self._shift: self.touch.move(fields['delta'])
            else: d.jog(fields['delta'])
        elif kind=='Boton':
            name,down=fields['nombre'],fields['presionado']
            if name=='SHIFT': self._shift=down;d._fine=down;d._touch()
            elif name.startswith('ENCODER_'):
                encoder=int(name.split('_')[1])-1
                if d._screen=='DrumVariations' and self._shift:
                    if down: d.auditionVariant(encoder)
                elif down: self.console.pressEncoder(encoder)
                elif d._screen=='DrumPerform' and encoder in (3,4):
                    d._hold='';d._touch()
            elif down:
                if name=='PLAY': d.play()
                elif name=='STOP': d.stop()
                elif name in ('REC','RECORD'): d.record()
                elif name=='JOG':
                    if self._shift: self.touch.activate()
                    elif d._screen in ('DrumBrowser','DrumBrowserEmpty'): d.loadSound()
                    else: d.openPad(d.selectedPad)
                elif name in ('LEFT','RIGHT'): self.touch.move(-1 if name=='LEFT' else 1)
                elif name in ('PAGE_LEFT','PAGE_RIGHT'): d.action('step-page' if d._screen in ('DrumSteps','DrumProbability') else 'bank')
                elif name=='SEARCH': self.console.navigate('DrumBrowser')
                elif name=='BACK': self.console.navigate('DrumRack' if d._screen!='DrumRack' else 'ModeSelect')
                elif name=='MENU': self.console.navigate('Settings')
        elif kind=='ManoPresencia': self._presence[i]=fields['presente']
        elif kind in ('Mano','Expresion'):
            if kind=='Expresion' or self._presence[i]:
                param='cutoff' if i==0 else 'send';value=20*1000**fields['valor'] if i==0 else fields['valor']
                d.set_param(d.selectedPad,param,value)
        elif kind=='Perilla':
            if self.shared.room: self.shared.room.submit(fields['nombre'],fields['valor'])
        return True

    def update_leds(self):
        if not self.active: return
        port=self.shared.led_port
        if not port: return
        try:
            import mido
            pads=self.drum.pad_entries();steps=self.drum.steps
            for index in range(32):
                if index<16:
                    p=pads[index];rgb=tuple(round(int(p['tint'][i:i+2],16)/255*(45 if index==self._selected else 16)) for i in (1,3,5)) if not p['empty'] else (0,0,0)
                    if p['mute']: rgb=tuple(v//4 for v in rgb)
                else:
                    s=steps[index-16];rgb=(15,80,55) if s['playhead'] else (30,30,30) if s['velocity'] and s['active'] else (0,0,0)
                if self._leds.get(index)!=rgb:
                    for msg in led_messages(index,rgb): port.send(mido.Message(**msg))
                    self._leds[index]=rgb
            error=getattr(self.notes,'error','')
            if error: self.failure(error)
        except (OSError,ValueError,RuntimeError) as exc: self.failure(str(exc))

    @Slot()
    def close(self):
        self.led_timer.stop();self.notes.close()
