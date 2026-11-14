"""DX7 routing facts decoded from Google's MSFA bus table (Apache-2.0).

Reference: google/music-synthesizer-for-android, app/src/main/jni/fm_core.cc.
Operators execute 6→1; edges run modulator→carrier. FB input/output may differ
for algorithms 4 and 6: retain both endpoints rather than inventing a self-loop.
"""
# The six hexadecimal flags encode bus input, bus output, add, feedback in/out.
from synth_catalog import METADATA

_DX7_BUSES = tuple(' '.join(f'{flag:02x}' for flag in route) for route in METADATA['dx7_algorithms'])


def _decode(number, flags):
    buses = {1: [], 2: []}
    carriers, edges = [], []
    feedback = feedback_source = None
    for op, token in zip(range(6,0,-1), flags.split()):
        f = int(token,16)
        inp, out = (f >> 4) & 3, f & 3
        if inp:
            edges.extend((source,op) for source in buses[inp])
        if out:
            buses[out] = buses[out]+[op] if f & 4 else [op]
        else:
            carriers.append(op)
        if f & 64:
            feedback = op
        if f & 128:
            feedback_source = op
    return dict(number=number, operators=6, carriers=sorted(carriers), edges=[list(e) for e in edges],
                feedback=feedback, feedback_source=feedback_source)


DX7_ALGORITHMS = tuple(_decode(i, flags) for i,flags in enumerate(_DX7_BUSES,1))
# The eight four-operator topologies used in the supplied design's simple mode.
FM4_ALGORITHMS = tuple(dict(number=i, operators=4, carriers=c, edges=[list(edge) for edge in e], feedback=4, feedback_source=4)
    for i,c,e in [
        (1,[1],[(4,3),(3,2),(2,1)]), (2,[1],[(4,2),(3,2),(2,1)]),
        (3,[1],[(4,1),(3,2),(2,1)]), (4,[1],[(4,3),(3,1),(2,1)]),
        (5,[1,3],[(4,3),(2,1)]), (6,[1,2,3],[(4,1),(4,2),(4,3)]),
        (7,[1,2,3],[(4,3)]), (8,[1,2,3,4],[])])


def algorithm(count, number):
    table = DX7_ALGORITHMS if count == 6 else FM4_ALGORITHMS
    return table[max(0,min(len(table)-1, int(number)-1))]
