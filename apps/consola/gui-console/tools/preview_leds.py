#!/usr/bin/env python3
"""Offscreen LED23 HTML/GIF fixtures, evaluated by the actual firmware C++ renderer.

Run: python3 apps/consola/gui-console/tools/preview_leds.py
C++17 compiler required; Pillow optional for GIF/contact-sheet exports.
"""
from pathlib import Path
from types import SimpleNamespace
import argparse
import json
import os
import subprocess
import sys
import tempfile

GUI = Path(__file__).resolve().parents[1]
ROOT = GUI.parents[2]
sys.path.insert(0, str(GUI))
from pad_leds import (PadState, cc, pad_messages, synth_frame, drum_frame,
                      HIT, DIM, SELECTED, rgb7)
from scale_keyboard import ScaleKeyboard


def fixtures(mode, animations=True):
    keyboard = ScaleKeyboard()
    colors = ('#FF4560', '#FF9A2E', '#F5E04A', '#E8F0FF')
    pads = [dict(tint=colors[i//4], empty=False, mute=i==14, dimmed=False, group=i//4) for i in range(16)]
    groups = [dict(mute=False) for _ in range(4)]
    serials = [0]*16
    last_hit = -1
    for t in range(0, 4455, 34):
        messages = [cc(23, 23), cc(24, mode), cc(25, 28), cc(26, int(animations))] if t==0 else []
        # Last heartbeat at 2040 ms; deliberately absent until recovery at 4250 ms.
        if t<=2380 and t%510==0 or t>=4250:
            messages.append(cc(119, (t//510)%128))
        running = t<2800
        flags = int(running) | (2 if mode==2 and 900<=t<2100 else 0) | (4 if 1800<=t<2700 else 0)
        messages += [cc(27, flags), cc(28, (t//492)%128)]
        if mode==1:
            part = 1 if t<1700 else 3
            held = {(1, keyboard.note(24)):90, (1, keyboard.note(26)):65} if 700<=t<1400 else {(3, keyboard.note(17)):120} if 1800<=t<2400 else {}
            states = synth_frame(keyboard, part, held)
        else:
            step = t//123%16
            steps = [dict(active=True, velocity=(50+i*5 if i%2==0 else 0), playhead=running and i==step) for i in range(16)]
            selected = 0 if t<1700 else 8
            d = SimpleNamespace(pad_entries=lambda:pads, steps=steps, selectedPad=selected, _rack={'groups':groups})
            hits = {}
            hit_index = t//246
            # Explicitly omit probability/mute-rejected hits; choke the open hat after 1500 ms.
            if 600<=t<2800 and hit_index!=last_hit and hit_index%4!=2:
                i = hit_index%12
                serials[i] = (serials[i]+1)%128
                hits[i] = (64 if hit_index%2 else 127, serials[i])
            last_hit = hit_index
            states = drum_frame(d, hits, [int(i==10 and t>=1500) for i in range(16)], serials)
        for i, state in enumerate(states):
            messages += pad_messages(i, state)
        yield t, messages


def evaluate(binary, mode, animations):
    lines = []
    for t, messages in fixtures(mode, animations):
        for message in messages:
            control = message['type']=='control_change'
            lines.append(f"{t} 16 {176 if control else 144} {message['control'] if control else message['note']} {message['value'] if control else message['velocity']}")
        lines.append(f'{t} 0 0 0 0')
    output = subprocess.run([str(binary)], input='\n'.join(lines)+'\n', text=True, capture_output=True, check=True).stdout
    return [list(map(int, line.split())) for line in output.splitlines()]


def html(scenes):
    template = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>GrooveForge · NeoTrellis LED23</title><style>
*{box-sizing:border-box}body{background:#0b0e13;color:#e3e8f0;font:16px system-ui;margin:0;padding:36px;max-width:1160px}small{color:#8d9bb1}h1{font-size:30px;margin:12px 0 8px}p{color:#a8b5ca;line-height:1.5}main{display:grid;grid-template-columns:1fr 275px;gap:28px;margin-top:32px}.grid{display:grid;grid-template-columns:repeat(8,1fr);gap:10px;padding:24px;background:#141b25;border:1px solid #303c50;border-radius:20px}.pad{aspect-ratio:1;border:1px solid #3b4658;border-radius:12px;display:grid;place-items:center;font:12px ui-monospace;color:#a9b4c5;transition:background 34ms linear}aside{background:#141b25;border-radius:16px;padding:20px}button,select{background:#223047;color:#e3e8f0;border:1px solid #415471;border-radius:8px;padding:10px;margin:0 8px 14px 0;font:inherit}input[type=range]{width:100%;margin-top:24px}strong{color:#e3e8f0}#status{color:#e8b84a}label{display:block;margin:12px 0;font-size:14px}.legend{font-size:14px}footer{border-top:1px solid #283248;margin-top:30px;padding-top:16px;color:#8d9bb1;font-size:13px}@media(max-width:800px){main{grid-template-columns:1fr}body{padding:18px}.grid{gap:5px;padding:12px}}
</style><small>GROOVEFORGE / CONSOLE / LED23</small><h1>NeoTrellis · light follows the instrument</h1><p>32 pads. Local Teensy animation, unchanged UART format. This timeline executes the production C++ renderer offscreen.</p>
<select id="scene" aria-label="Scene"></select><button id="play">Pause</button><button id="restart">Restart</button><main><section><div id="grid" class="grid"></div><input id="time" type="range" aria-label="Timeline"><p id="position"></p></section><aside><strong id="name"></strong><p id="status"></p><p class="legend" id="legend"></p><label><input id="boost" type="checkbox" checked> Display gain ×3</label><p class="legend">Display gain makes low brightness visible on a monitor. Device data remains capped at 28/127 (22%). Hover a pad for its actual RGB7.</p></aside></main><footer>Fixture preview, no audio or hardware attached. 34 ms frames; <600 ms mode sweep; 180 ms decays; amber after 1500 ms without host heartbeat. Use the static scene to inspect animations OFF.</footer>
<script>const scenes=__SCENES__;const grid=document.getElementById('grid'),sel=document.getElementById('scene'),slider=document.getElementById('time');let index=0,playing=true,last=0;
for(let i=0;i<32;i++){let p=document.createElement('div');p.className='pad';p.textContent=i+1;grid.append(p)}
scenes.forEach((s,i)=>{let o=document.createElement('option');o.value=i;o.textContent=s.name;sel.append(o)});
function draw(){let s=scenes[+sel.value],f=s.frames[index];slider.max=s.frames.length-1;slider.value=index;document.getElementById('name').textContent=s.name;document.getElementById('position').textContent=(f[0]/1000).toFixed(2)+' s / 4.45 s';document.getElementById('status').textContent=f[0]>=3570&&f[0]<4250?'Host heartbeat lost · local amber':f[0]>=4250?'Host restored · current snapshot':f[0]<480?'Boot / mode sweep':f[0]>=2800?'STOP · host heartbeat silence':f[0]>=1800?'Queued pattern / selected part':f[0]>=900?'Perform / record armed':'Scale / velocity';document.getElementById('legend').textContent=s.description;[...grid.children].forEach((p,i)=>{let rgb=f.slice(1+i*3,4+i*3);let gain=document.getElementById('boost').checked?3:1;let c=rgb.map(v=>Math.min(255,Math.round(v*255/127*gain)));p.style.background='rgb('+c+')';p.style.color=Math.max(...c)>100?'#0b0e13':'#a9b4c5';p.title='Pad '+(i+1)+' · RGB7 '+rgb.join(', ');});}
sel.onchange=()=>{index=0;draw()};slider.oninput=()=>{index=+slider.value;playing=false;document.getElementById('play').textContent='Play';draw()};document.getElementById('play').onclick=()=>{playing=!playing;document.getElementById('play').textContent=playing?'Pause':'Play'};document.getElementById('restart').onclick=()=>{index=0;draw()};document.getElementById('boost').onchange=draw;function tick(t){if(playing&&t-last>=34){last=t;index=(index+1)%scenes[+sel.value].frames.length;draw()}requestAnimationFrame(tick)}draw();requestAnimationFrame(tick);
</script></html>'''
    return template.replace('__SCENES__', json.dumps(scenes, separators=(',', ':')))


def export_images(scenes, output):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return []
    def frame_image(scene, values):
        image = Image.new('RGB', (720, 420), '#0b0e13')
        draw = ImageDraw.Draw(image)
        draw.text((26, 16), scene['name']+' | '+str(values[0])+' ms | display gain x3', fill='#e3e8f0')
        for i in range(32):
            x, y = 28+i%8*84, 52+i//8*84
            color = tuple(min(255, round(v*255/127*3)) for v in values[1+i*3:4+i*3])
            draw.rounded_rectangle((x, y, x+72, y+72), 10, fill=color, outline='#3b4658', width=2)
            draw.text((x+8, y+8), str(i+1), fill='#8d9bb1' if max(color)<100 else '#0b0e13')
        draw.text((26, 400), 'Actual device cap 28/127; preview only, no physical validation', fill='#8d9bb1')
        return image
    artifacts = []
    for scene in scenes:
        name = scene['name'].split()[0].lower()
        images = [frame_image(scene, values) for values in scene['frames'][::2]]
        path = output/(name+'.gif')
        images[0].save(path, save_all=True, append_images=images[1:], duration=68, loop=0)
        artifacts.append(path.name)
    picks = [(scene, min(scene['frames'], key=lambda f:abs(f[0]-target)))
             for scene in scenes[:2] for target in (204, 1054, 2006, 3944)]
    sheet = Image.new('RGB', (1440, 1680), '#0b0e13')
    for i, (scene, values) in enumerate(picks):
        sheet.paste(frame_image(scene, values), ((i//4)*720, (i%4)*420))
    sheet.save(output/'contact-sheet.png')
    return artifacts+['contact-sheet.png']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=GUI/'screenshots/leds')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    surface = ROOT/'apps/consola/surface'
    with tempfile.TemporaryDirectory(prefix='gf-led-preview-') as tmp:
        binary = Path(tmp)/'renderer'
        subprocess.run([os.environ.get('CXX','c++'), '-std=c++17', '-O2', '-I'+str(surface/'include'),
                        str(Path(__file__).with_suffix('.cpp').with_name('led_preview.cpp')),
                        str(surface/'src/PadLedReturn.cpp'), '-o', str(binary)], check=True)
        scenes = [dict(name=name, description=description, frames=evaluate(binary, mode, animation))
                  for name, mode, animation, description in (
                      ('SYNTH held / release',1,True,'Part-colored held notes follow velocity. Root accents, release fades, part change and clock edge pulses.'),
                      ('DRUM hit / sequence',2,True,'Top two rows are pads. Bottom two rows are selected-lane steps. Velocity, playhead, record, queued pulse, mute and choke.'),
                      ('STATIC animations off',2,False,'No sweep, hit flash or pulse. Selected pad, active steps, playhead, record/queued and amber remain legible.'))]
    (args.output/'index.html').write_text(html(scenes))
    (args.output/'frames.json').write_text(json.dumps(scenes, separators=(',', ':'))+'\n')
    artifacts = export_images(scenes, args.output)
    print(json.dumps(dict(output=str(args.output), frames=sum(len(s['frames']) for s in scenes), artifacts=['index.html','frames.json']+artifacts)))


if __name__ == '__main__':
    main()
