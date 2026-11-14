"""Láminas de montaje PCB-P en milímetros; no intervienen en fabricación."""
from html import escape
import math


def write_drawings(out, board, sensor_rows, hole_rows, dimensions):
    """Exportar planta 1:1 y sección de la cola a partir de la placa verificada."""
    import pcbnew as p
    docs=out/'docs'
    radius,thickness,height,film,insertion,length,distance,bottom_run=dimensions
    labels=('V1','V2','V3','V4','X1','X2')
    footprints={fp.GetReference():fp for fp in board.GetFootprints()}
    def xy(v): return p.ToMM(v.x)+44,371-p.ToMM(v.y)
    def page(w,h):
        return [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}mm" height="{h}mm" viewBox="0 0 {w} {h}">',
                '<rect width="100%" height="100%" fill="white"/>',
                '<g font-family="sans-serif" font-size="2.1" fill="#111">']
    def text(svg,x,y,value,size=2.1):
        svg.append(f'<text x="{x}" y="{y}" font-size="{size}">{escape(value)}</text>')
    def mapped(x,y):return x-9+10,100-y+15
    svg=page(243,75)
    text(svg,10,5,'GROOVEFORGE · PCB-P · montaje 1:1 · vista superior',3)
    text(svg,10,10,'Imprimir al 100 %, sin ajustar. Cotas y coordenadas en mm. Componentes inferiores: línea discontinua.')
    svg.append('<rect x="10" y="15" width="223" height="31" fill="none" stroke="black" stroke-width="0.15"/>')
    for i,row in enumerate(hole_rows,1):
        x,y=mapped(float(row['x_mm']),float(row['y_mm']))
        svg.append(f'<circle cx="{x}" cy="{y}" r="1.35" fill="none" stroke="black" stroke-width="0.15"/>')
        svg.append(f'<path d="M{x-1.9} {y}h3.8 M{x} {y-1.9}v3.8" fill="none" stroke="#777" stroke-width="0.08"/>')
        text(svg,x-1.2,y+3.3,f'H{i}',1.5)
    for i,row in enumerate(sensor_rows,1):
        cx,cy=float(row['x_mm']),float(row['y_mm']);label=row['notes'].removeprefix('FSR ')
        index=labels.index(label)+1;x,y=mapped(cx,cy)
        svg.append(f'<circle cx="{x}" cy="{y}" r="9.15" fill="none" stroke="#111" stroke-width="0.15"/>')
        text(svg,x-3.5,y+0.6,f'{label} / FSR{index}',1.8)
        slot=next(iter(footprints[f'SLOT{i}'].Pads()));sx,sy=mapped(*xy(slot.GetPosition()))
        a=slot.GetOrientationDegrees()
        svg.append(f'<rect x="-1.5" y="-4.4" width="3" height="8.8" rx="1.5" transform="translate({sx} {sy}) rotate({-a})" fill="none" stroke="black" stroke-width="0.15"/>')
        fp=footprints[f'FSR{index}'];pads={pad.GetNumber():xy(pad.GetPosition()) for pad in fp.Pads()}
        centre=tuple((pads['1'][j]+pads['2'][j])/2 for j in (0,1));bx,by=mapped(*centre)
        svg.append(f'<rect x="-1.27" y="-2.54" width="2.54" height="5.08" transform="translate({bx} {by}) rotate({-fp.GetOrientationDegrees()})" fill="none" stroke="#555" stroke-dasharray="0.6 0.4" stroke-width="0.12"/>')
        for n,point in pads.items():
            px,py=mapped(*point)
            svg.append(f'<circle cx="{px}" cy="{py}" r="0.5" fill="none" stroke="black" stroke-width="0.1"/>')
            text(svg,px+.7,py+.5,n,1.3)
        jp=footprints[f'J{index}'];jx,jy=mapped(*xy(jp.GetPosition()))
        text(svg,jx-1,jy-2.5,f'J{index}',1.7)
        for pad in jp.Pads():
            px,py=mapped(*xy(pad.GetPosition()))
            svg.append(f'<circle cx="{px}" cy="{py}" r="0.375" fill="none" stroke="black" stroke-width="0.1"/>')
    text(svg,10,52,'Placa 223 × 31 × 1,6 · 10 NPTH Ø2,7 · 6 ranuras NPTH 3 × 8,8 · sockets y J1–J6 por debajo')
    text(svg,10,57,'H1–H10: x = 40 / 89,5 / 139 / 170 / 201; y = 72 y 97. Centros conservados del CSV v5.')
    svg.append('<path d="M10 63h100 M10 61v4 M110 61v4" stroke="black" stroke-width="0.15" fill="none"/>')
    text(svg,45,68,'Control de escala: 100 mm')
    svg.append('</g></svg>')
    (docs/'pcb-p-assembly-1to1.svg').write_text('\n'.join(svg)+'\n')

    svg=page(160,70)
    text(svg,7,6,'PCB-P · sección radial de una cola · escala 1:1',3)
    text(svg,7,11,'Misma sección para los seis sensores; girada 25° en planta, 205° para V4.')
    # El origen radial está en el borde del disco, z=0 en la cara superior P.
    ox,oy=34,26
    socket_x=distance-9.15
    slot_x=12.3-9.15
    for x,w in ((-18.3,18.3+slot_x-1.5),(slot_x+1.5,socket_x+4-slot_x-1.5)):
        svg.append(f'<rect x="{ox+x}" y="{oy}" width="{w}" height="{thickness}" fill="#ddd" stroke="black" stroke-width="0.1"/>')
    svg.append(f'<rect x="{ox-18.3}" y="{oy-film}" width="18.3" height="{film}" fill="#888"/>')
    svg.append(f'<rect x="{ox+socket_x-1.27}" y="{oy+thickness}" width="2.54" height="{height}" fill="#bbb" stroke="black" stroke-width="0.1"/>')
    r=radius;v=thickness+height+film/2
    x1=1+r;x2=1+2*r;x3=x2+bottom_run;x4=x3+r
    y0=-film/2;y1=y0+r;y2=y1+v;y3=y2+r;mouth=thickness+height
    path=(f'M{ox} {oy+y0}h1 a{r} {r} 0 0 1 {r} {r} v{v} '
          f'a{r} {r} 0 0 0 {r} {r} h{bottom_run} a{r} {r} 0 0 0 {r} {-r} '
          f'v{-r-insertion}')
    svg.append(f'<path d="{path}" fill="none" stroke="#222" stroke-width="{film}"/>')
    text(svg,7,22,'Disco Ø18,3',2)
    text(svg,57,29,'Socket inferior H=8,5',2)
    text(svg,57,34,'Boca a TOP −16,6',2)
    text(svg,57,39,'Inserción nominal: 3',2)
    text(svg,7,46,'P superior = TOP −6,5; espesor = 1,6; K inferior = TOP −5,0. Luz superior: 1,5.')
    text(svg,7,51,f'L = {length:g} = 1 + 3πR/2 + V + B + R + 3; R = {r:g}; V = {v:.2f}; B = {bottom_run:.3f}.')
    text(svg,7,56,f'D centro disco → eje socket = 9,15 + 1 + B + 3R = {distance:.3f}. Cola hasta TOP −21,83.')
    text(svg,7,61,'No marcar la película ni tapar su respiradero. Recortar metal/soldadura superior a ≤0,8.')
    text(svg,7,66,'37,4 se interpreta desde borde de disco a punta metálica. Comprobar inserción y recorrido con pieza real.')
    svg.append('</g></svg>')
    (docs/'pcb-p-tail-section-1to1.svg').write_text('\n'.join(svg)+'\n')
