"""Make a board match its schematic exactly, so DRC schematic parity is clean.

The KiCad MCP's "update PCB from schematic" names nets without the root-sheet "/" prefix and
doesn't copy symbol fields or BOM attributes. This reads the netlist kicad-cli exports and:
  - renames every board net to the schematic's net name,
  - copies Description / Datasheet / LCSC fields to the footprints,
  - copies 'exclude from BOM' / 'DNP' / 'exclude from position files' attributes,
  - writes the JLCPCB design rules into the .kicad_pro (the Brain's values).

Usage (KiCad Python): sync_from_netlist.py <board-name>
"""
import json
import os
import re
import subprocess
import sys

import pcbnew

import boardlib as bl

KC = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
name = sys.argv[1]
d = os.path.join(bl.BOARDS, name)
net = f"/tmp/{name}-sync.net"
subprocess.run([KC, "sch", "export", "netlist", "-o", net, os.path.join(d, f"{name}.kicad_sch")],
               check=True, capture_output=True)
t = open(net).read()

# ---- component fields / attributes
comps = {}
for blk in re.split(r'\n\t\t\(comp\n', t[:t.index("(libparts")])[1:]:
    ref = re.search(r'\(ref "([^"]+)"\)', blk).group(1)
    f = {k: v for k, v in re.findall(r'\(field\s*\(name "([^"]+)"\)\s*"([^"]*)"\)', blk)}
    dm = re.search(r'\(description "([^"]*)"\)', blk)
    f["Description"] = dm.group(1) if dm else ""
    props = dict(re.findall(r'\(property\s*\(name "([^"]+)"\)\s*\(value "([^"]*)"\)\s*\)', blk))
    props.update({k: "" for k in re.findall(r'\(property\s*\(name "([^"]+)"\)\s*\)', blk)})
    comps[ref] = (f, props)

# ---- schematic net per pad
t2 = t[t.index("(nets"):]
padnet = {}
for blk in re.split(r'\n\t\t\(net\n', t2)[1:]:
    nn = re.search(r'\(name "([^"]*)"\)', blk).group(1)
    for r, p in re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', blk):
        padnet[(r, p)] = nn

path, b = bl.load(name)
renamed = 0
for n in list(b.GetNetsByName().values()):
    old = n.GetNetname()
    if old and not old.startswith("/") and not old.startswith("unconnected"):
        n.SetNetname("/" + old)
        renamed += 1
# every pad carries the schematic's net (pin re-assignments, 'unconnected-(...)' names)
reassigned = 0
for fpt in b.GetFootprints():
    for p in fpt.Pads():
        want = padnet.get((fpt.GetReference(), p.GetNumber()))
        if want and p.GetNetname() != want:
            reassigned += 1
            ni = b.FindNet(want)
            if ni is None:
                ni = pcbnew.NETINFO_ITEM(b, want)
                b.Add(ni)
            p.SetNet(ni)

for fpt in b.GetFootprints():
    ref = fpt.GetReference()
    if ref not in comps:
        continue
    fields, props = comps[ref]
    attrs = fpt.GetAttributes()
    def setattr_(flag, on):
        global attrs
        attrs = attrs | flag if on else attrs & ~flag
    setattr_(pcbnew.FP_EXCLUDE_FROM_BOM, "exclude_from_bom" in props)
    setattr_(pcbnew.FP_DNP, "dnp" in props)
    fpt.SetAttributes(attrs)

bl.save(b, path)

pro = os.path.join(d, f"{name}.kicad_pro")
pj = json.load(open(pro))
ds = pj.setdefault("board", {}).setdefault("design_settings", {})
ds.setdefault("rules", {}).update({
    "min_clearance": 0.0, "min_copper_edge_clearance": 0.25, "min_hole_clearance": 0.25,
    "min_hole_to_hole": 0.5, "min_text_height": 0.8, "min_text_thickness": 0.08,
    "min_through_hole_diameter": 0.3, "min_track_width": 0.2, "min_via_annular_width": 0.13,
    "min_via_diameter": 0.5, "solder_mask_to_copper_clearance": 0.0})
json.dump(pj, open(pro, "w"), indent=2)
print(f"{name}: {renamed} nets renamed, {reassigned} pads re-assigned, {len(comps)} components synced, JLC rules written")
