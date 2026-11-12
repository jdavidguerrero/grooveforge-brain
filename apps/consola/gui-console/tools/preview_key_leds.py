#!/usr/bin/env python3
"""Render CSV-positioned key previews with Pi fixtures and the production C++ renderer.

Run from any directory: python3 apps/consola/gui-console/tools/preview_key_leds.py
Requires C++17, PySide6 and Pillow. Builds only under the checkout's ignored .pio/.
"""
import csv
import html
import json
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
from PIL import Image, ImageDraw, ImageFont

GUI = Path(__file__).resolve().parents[1]
ROOT = GUI.parents[2]
sys.path.insert(0, str(GUI))
from console_buttons import ConsoleButtons
from key_leds import KEY_REFS, key_frame, key_messages

OUT = GUI/'screenshots/key-leds'
LABELS = ('PLAY', 'STOP', 'REC', 'LOOP', 'METRO', 'PG>', 'PG<', 'OCT-', 'OCT+',
          'DEL', 'DUP', 'QNT', 'NEW', 'UNDO', 'SHIFT', 'BUILD', 'DROP', 'BREAK',
          'TAP', 'FILL', 'CUE', 'BRWS', '>', '<', 'MENU', 'BACK')


def evaluate(binary, mode):
    console = SimpleNamespace(drumMode=mode=='DRUM', synthMode=mode=='SYNTH')
    console.buttons = ConsoleButtons(console)
    console.buttons.shift = True
    console.buttons._armed = 'DROP'
    states = key_frame(console, dict(running=True, armed=True), mode!='LIVE')
    controls = [(102, 27), (25, 28), (26, 1), (107, 120), (108, 0), (109, 0), (111, int(mode!='LIVE'))]
    for i, state in enumerate(states):
        controls += [(m['control'], m['value']) for m in key_messages(i, state)]
    lines = [f'0 16 176 {cc} {value}' for cc, value in controls]
    for t in range(0, 1021, 34):
        if t%510==0:
            lines += [f'{t} 16 176 110 {t//510}', f'{t} 16 176 28 {t//510}']
        lines.append(f'{t} 0 0 0 0')
    output = subprocess.run([str(binary)], input='\n'.join(lines)+'\n', text=True, capture_output=True, check=True).stdout
    return [list(map(int, line.split())) for line in output.splitlines()]


def xy(row):
    return (60+float(row['x_mm'])*2.3, 155+(340-float(row['y_mm']))*2.3)


def display_rgb(frame, index):
    return tuple(min(255, v*3) for v in frame[2+index*3:5+index*3])


def svg(mode, rows, frame):
    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="910" height="1020" viewBox="0 0 910 1020">',
             '<rect width="910" height="1020" fill="#0c0f12"/>',
             f'<text x="60" y="48" fill="#eee" font-size="28" font-family="sans-serif">GrooveForge · {mode} · 26 key LEDs</text>',
             f'<text x="60" y="80" fill="#aeb7bd" font-size="15" font-family="sans-serif">SHIFT held · DROP armed · t={frame[0]} ms · brightness 28/127 · LED model {frame[1]/1000:.1f} mA</text>',
             '<text x="60" y="108" fill="#aeb7bd" font-size="15" font-family="sans-serif">CSV v1 positions · front at bottom · display gain ×3 · provisional chain</text>',
             '<rect x="60" y="155" width="782" height="782" rx="14" fill="#15191e" stroke="#46505c"/>',
             '<rect x="245" y="183" width="410" height="235" rx="6" fill="#20262c"/>',
             f'<text x="450" y="302" fill="#586672" font-size="26" text-anchor="middle" font-family="sans-serif">{mode}</text>']
    coords = [xy(rows[ref]) for ref in KEY_REFS]
    route = coords[:5]+[xy({'x_mm':248,'y_mm':39.5}),xy({'x_mm':248,'y_mm':115.5})]+coords[5:9]+[xy({'x_mm':242,'y_mm':180}),xy({'x_mm':98,'y_mm':180})]+coords[9:]
    parts.append('<polyline points="'+' '.join(f'{x},{y}' for x,y in route)+'" fill="none" stroke="#39444e" stroke-dasharray="3 5"/>')
    for i, ref in enumerate(KEY_REFS):
        x,y = coords[i]
        color = '#'+''.join(f'{c:02x}' for c in display_rgb(frame,i))
        label = 'S'+ref[5] if ref.startswith('SCENE') and mode=='LIVE' else LABELS[i]
        parts += [f'<g class="key"><title>{i} · {html.escape(ref)}</title>',
                  f'<rect x="{x-15.5}" y="{y-15.5}" class="border" width="31" height="31" rx="3" fill="#080a0c" stroke="{color}"/>',
                  f'<rect x="{x-10}" y="{y-12}" class="window" width="20" height="5" rx="1" fill="{color}"/>',
                  f'<text x="{x}" y="{y+7}" text-anchor="middle" fill="#dce0e4" font-family="sans-serif" font-size="8">{html.escape(label)}</text>',
                  f'<text x="{x}" y="{y+25}" text-anchor="middle" fill="#788691" font-family="sans-serif" font-size="8">{i}</text></g>']
    parts += ['<text x="60" y="975" fill="#aeb7bd" font-size="15" font-family="sans-serif">Teal perform · violet edit · scene column follows ConsoleButtons.sceneColor</text>', '</svg>']
    return '\n'.join(parts)


def png(mode, rows, frame):
    image = Image.new('RGB', (910,1020), '#0c0f12')
    d = ImageDraw.Draw(image)
    font_path = next((p for p in (
        '/System/Library/Fonts/Supplemental/Arial.ttf',
        '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf') if Path(p).exists()), None)
    font = lambda size: ImageFont.truetype(font_path,size) if font_path else ImageFont.load_default()
    title, normal, small = font(26), font(14), font(8)
    d.text((60,25), f'GrooveForge · {mode} · 26 key LEDs', font=title, fill='#eeeeee')
    d.text((60,70),f'SHIFT held · DROP armed · t={frame[0]} ms · brightness 28/127 · LED model {frame[1]/1000:.1f} mA',font=normal,fill='#aeb7bd')
    d.text((60,99),'CSV v1 · front at bottom · display gain ×3 · provisional chain',font=normal,fill='#aeb7bd')
    d.rounded_rectangle((60,155,842,937),14,fill='#15191e',outline='#46505c')
    d.rounded_rectangle((245,183,655,418),6,fill='#20262c')
    d.text((450,300),mode,font=title,fill='#586672',anchor='mm')
    coords = [xy(rows[ref]) for ref in KEY_REFS]
    route=coords[:5]+[xy({'x_mm':248,'y_mm':39.5}),xy({'x_mm':248,'y_mm':115.5})]+coords[5:9]+[xy({'x_mm':242,'y_mm':180}),xy({'x_mm':98,'y_mm':180})]+coords[9:]
    d.line(route,fill='#39444e',width=1)
    for i,ref in enumerate(KEY_REFS):
        x,y=coords[i];color=display_rgb(frame,i)
        label='S'+ref[5] if ref.startswith('SCENE') and mode=='LIVE' else LABELS[i]
        d.rounded_rectangle((x-15.5,y-15.5,x+15.5,y+15.5),3,fill='#080a0c',outline=color)
        d.rectangle((x-10,y-12,x+10,y-7),fill=color)
        d.text((x,y+5),label,font=small,fill='#dce0e4',anchor='mm')
        d.text((x,y+24),str(i),font=small,fill='#788691',anchor='mm')
    d.text((60,965),'Teal perform · violet edit · scene column follows ConsoleButtons.sceneColor',font=normal,fill='#aeb7bd')
    image.save(OUT/f'{mode.lower()}.png')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    build = ROOT/'.pio/key-led-preview'
    build.mkdir(parents=True, exist_ok=True)
    binary = build/'render'
    subprocess.run(['c++', '-std=c++17', '-O2', '-I'+str(ROOT/'apps/consola/surface/include'),
                    str(GUI/'tools/key_led_preview.cpp'), str(ROOT/'apps/consola/surface/src/KeyLedReturn.cpp'),
                    '-o', str(binary)], check=True)
    with (ROOT/'apps/hardware/kicad/consola/panel/panel-coordinates-console-v1.csv').open() as f:
        rows={r['ref']:r for r in csv.DictReader(f) if r['kind']=='key'}
    assert set(rows)==set(KEY_REFS) and len(rows)==26
    scenes={mode:evaluate(binary, mode) for mode in ('LIVE','SYNTH','DRUM')}
    for mode, frames in scenes.items():
        assert all(frame[1]<=500000 and max(frame[2:])<=62 for frame in frames)
        (OUT/f'{mode.lower()}.svg').write_text(svg(mode, rows, frames[0]))
        png(mode,rows,frames[0])
    (OUT/'frames.json').write_text(json.dumps(scenes,indent=2)+'\n')
    # JS paints precomputed RGB bytes; all animation evaluation remains in C++.
    markup = '<!doctype html><html lang="en"><meta charset="utf-8"><title>Key LED27 preview</title><style>body{background:#0c0f12;color:#eee;font:16px sans-serif;margin:24px}svg{max-width:100%;height:auto}input{margin:8px}#panels{display:flex;gap:20px}section{width:33%}</style><h1>Key LED27 · production C++ frames</h1><p>CSV v1 positions. SHIFT held; DROP armed. Renderer output displayed at gain ×3. Static captions refer to initial frame.</p><label>Frame <input id="time" type="range" min="0" max="30" value="0"></label><span id="stamp"></span><div id="panels">'
    for mode, frames in scenes.items():
        markup += '<section id="'+mode+'"><h2>'+mode+'</h2>'+svg(mode,rows,frames[0])+'</section>'
    markup += '</div><script>const frames='+json.dumps(scenes)+';'
    markup += """const t=document.getElementById('time');
function show(){for(const mode of Object.keys(frames)){
 const frame=frames[mode][Number(t.value)];
 document.getElementById(mode).querySelectorAll('.key').forEach((key,i)=>{
 const color='rgb('+frame.slice(2+i*3,5+i*3).map(v=>Math.min(255,v*3)).join(',')+')';
 key.querySelector('.border').setAttribute('stroke',color);
 key.querySelector('.window').setAttribute('fill',color);
 });}
 document.getElementById('stamp').textContent=(t.value*34)+' ms';}
t.oninput=show;show();</script></html>"""
    (OUT/'preview.html').write_text(markup)
    print('Rendered 26 keys × 3 modes × 31 frames with production C++; outputs:',OUT)


if __name__=='__main__':
    main()
