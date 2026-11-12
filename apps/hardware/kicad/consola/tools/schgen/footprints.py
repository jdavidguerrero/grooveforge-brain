"""Console footprint sources. Provisional socket/FSR landings are visibly marked."""

from pathlib import Path
from .sexp import parse, dumps, children, Quoted as Q

DEFAULT_FOOTPRINTS = Path('/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints')

CHOC = '''(footprint "Kailh_Choc_V2_PG1353_Hotswap"
  (version 20241229) (generator "schgen") (layer "F.Cu")
  (descr "PG1353 + CPG135001S30. Panel +Y rear = KiCad -Y. Dual solder PTH dia1.3 pad1.9; socket SMD pad centres/sizes provisional: VERIFICAR CON ZÓCALO REAL. Not fabrication approved.")
  (tags "Kailh Choc V2 PG1353 CPG135001S30 hot-swap solder dual provisional")
  (attr through_hole)
  (property "Reference" "REF**" (at 0 -8.5) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))
  (property "Value" "PG1353 + CPG135001S30" (at 0 8.5) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))
  (fp_text user "VERIFICAR CON ZÓCALO REAL" (at 0 10) (layer "B.Fab") (effects (font (size 0.8 0.8) (thickness 0.12)) (justify mirror)))
  (fp_text user "PADS 1/2 PROVISIONALES" (at 0 11.5) (layer "B.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12)) (justify mirror)))
  (fp_rect (start -7.5 -7.5) (end 7.5 7.5) (stroke (width 0.1) (type solid)) (fill none) (layer "F.Fab"))
  (fp_rect (start -7.75 -7.75) (end 7.75 7.75) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))
  (fp_rect (start -4.825 2.25) (end 9.825 7.45) (stroke (width 0.05) (type solid)) (fill none) (layer "B.CrtYd"))
  (fp_rect (start -1.7 -6.43) (end 1.7 -3.43) (stroke (width 0.1) (type solid)) (fill none) (layer "Dwgs.User"))
  (fp_text user "LED CUTOUT VIA Dn" (at 0 -4.93) (layer "Dwgs.User") (effects (font (size 0.5 0.5) (thickness 0.1))))
  (pad "" np_thru_hole circle (at 0 0) (size 5 5) (drill 5) (layers "*.Cu" "*.Mask"))
  (pad "" np_thru_hole circle (at -5 -5.15) (size 1.5 1.5) (drill 1.5) (layers "*.Cu" "*.Mask"))
  (pad "1" thru_hole circle (at 0 5.9) (size 1.9 1.9) (drill 1.3) (layers "*.Cu" "*.Mask"))
  (pad "2" thru_hole circle (at 5 3.8) (size 1.9 1.9) (drill 1.3) (layers "*.Cu" "*.Mask"))
  (pad "1" smd roundrect (at -3.275 5.9) (size 2.6 2.6) (layers "B.Cu" "B.Paste" "B.Mask") (roundrect_rratio 0.15))
  (pad "2" smd roundrect (at 8.275 3.8) (size 2.6 2.6) (layers "B.Cu" "B.Paste" "B.Mask") (roundrect_rratio 0.15))
)'''

FSR = '''(footprint "FSR402_SolderTail_Provisional"
  (version 20241229) (generator "schgen") (layer "F.Cu")
  (descr "Interlink 402 sensor centre datum, dia18.3 from panel CSV. Generic solder-tail landing, NOT a certified Interlink termination. VERIFY actual variant/tail pitch, fold and solderability.")
  (attr smd)
  (property "Reference" "REF**" (at 0 -10.5) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))
  (property "Value" "FSR402 TAIL VERIFY" (at 0 14) (layer "F.Fab") (effects (font (size 0.8 0.8) (thickness 0.12))))
  (fp_circle (center 0 0) (end 9.15 0) (stroke (width 0.1) (type solid)) (fill none) (layer "F.Fab"))
  (fp_rect (start -9.4 -9.4) (end 9.4 13.25) (stroke (width 0.05) (type solid)) (fill none) (layer "F.CrtYd"))
  (fp_text user "VERIFICAR COLA FSR REAL" (at 0 -12) (layer "F.SilkS") (effects (font (size 0.8 0.8) (thickness 0.12))))
  (pad "1" smd rect (at -1.27 11) (size 2 4) (layers "F.Cu" "F.Mask"))
  (pad "2" smd rect (at 1.27 11) (size 2 4) (layers "F.Cu" "F.Mask"))
)'''


def write_footprints(directory, libraries=DEFAULT_FOOTPRINTS):
    """Write two marked provisional patterns and a vendored KiCad reverse-mount LED."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    for source in (CHOC, FSR):
        tree = parse(source)
        (directory / f'{tree[1]}.kicad_mod').write_text(dumps(tree)+'\n')
    source = Path(libraries) / 'LED_SMD.pretty/LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount.kicad_mod'
    tree = parse(source.read_text())
    tree[1] = Q('SK6812MINI-EA_ReverseMount')
    for node in children(tree, 'descr'):
        node[1] = Q('SK6812MINI-EA C5378731; OPSCO B/2 pp4-5; KiCad reverse-mount geometry, compatible pinout 1 GND 2 DIN 3 VDD 4 DOUT')
    for prop in children(tree, 'property'):
        if prop[1] == 'Value':
            prop[2] = Q('SK6812MINI-EA_ReverseMount')
    (directory / f'{tree[1]}.kicad_mod').write_text(dumps(tree)+'\n')


if __name__ == '__main__':
    write_footprints(Path(__file__).resolve().parents[2] / 'lib/gf_consola.pretty')
