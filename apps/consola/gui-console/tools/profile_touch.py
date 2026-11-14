"""Rendered main-thread replay: 30 clock + 30 drum_meters events/s, no DSP.

Run from gui-console: python3 tools/profile_touch.py --output screenshots/touch/after.json
Use --platform eglfs on the Pi with the existing runner's environment.
"""
import argparse
import cProfile
import io
import json
import math
import os
from pathlib import Path
import platform
import pstats
import statistics
import sys
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--duration', type=float, default=6)
parser.add_argument('--platform', default='offscreen')
parser.add_argument('--screens', nargs='+', default=['DrumBrowser', 'DrumSteps', 'DrumMixer'])
args = parser.parse_args()
os.environ['QT_QPA_PLATFORM'] = args.platform
if args.platform == 'offscreen': os.environ['QT_QUICK_BACKEND'] = 'software'
os.environ['GF_CONSOLE_SETTINGS'] = str(args.output.with_suffix('.settings.json'))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PySide6 import __version__ as qt_version
from PySide6.QtCore import QTimer, Qt
from PySide6.QtGui import QGuiApplication
from PySide6.QtTest import QTest
from main import create_engine
import shiboken6

app = QGuiApplication([])
results = []
for screen in args.screens:
    engine, console = create_engine(rotation={'qml_angle': 90})
    console.navigate(screen)
    drum = engine.drum_state
    # Representative browse list; the old bug invalidates all these delegates.
    drum._racks = [dict(name=f'TRIBE {i+1}', path=f'racks/tribe{i+1}.json') for i in range(8)]
    sample = drum._samples[0]
    drum._samples = [dict(sample, name=f'Sound {i:03d}') for i in range(128)]
    drum._touch(); drum.flush_frame()
    profiler = cProfile.Profile()
    timings = []
    count = 0
    def tick():
        global count
        count += 1
        step = int(count*122/(30*15))
        clock = dict(drum._clock, running=True, step=step%16, pattern_step=str(step),
                     sample_counter=str(count*1600), bar=step//16+1)
        pads = [max(0., math.sin(count/30*8+i))*.8 for i in range(16)]
        begin = time.perf_counter_ns()
        drum._handle(dict(event='clock', clock=clock))
        drum._handle(dict(event='drum_meters', pads=pads,
                          groups=[max(pads[i::4]) for i in range(4)], room=max(pads), cue=0.))
        engine.surface_batcher.drain_frame()
        timings.append((time.perf_counter_ns()-begin)/1e6/2)
    timer = QTimer(); timer.setTimerType(Qt.PreciseTimer); timer.setInterval(33)
    timer.timeout.connect(tick); timer.start()
    QTest.qWait(1000)
    timings.clear(); count = 0
    wall, cpu, main_cpu = time.perf_counter(), time.process_time(), time.thread_time()
    profiler.enable()
    QTest.qWait(round(args.duration*1000))
    profiler.disable()
    elapsed = time.perf_counter()-wall
    result = dict(screen=screen, seconds=elapsed, events=len(timings)*2,
                  events_per_second=len(timings)*2/elapsed,
                  ms_per_event=statistics.mean(timings),
                  p95_ms_per_event=sorted(timings)[int(len(timings)*.95)],
                  main_thread_cpu_percent=100*(time.thread_time()-main_cpu)/elapsed,
                  process_cpu_percent=100*(time.process_time()-cpu)/elapsed)
    stream = io.StringIO()
    pstats.Stats(profiler, stream=stream).strip_dirs().sort_stats('cumulative').print_stats(25)
    result['profile'] = stream.getvalue()
    results.append(result)
    timer.stop()
    engine.drum_surface.close(); engine.synth_surface.close()
    shiboken6.delete(engine); app.processEvents()
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(dict(platform=platform.platform(), python=platform.python_version(),
    qt=qt_version, qpa=args.platform, rotation=90, results=results), indent=2)+'\n')
print(json.dumps([{k:v for k,v in r.items() if k!='profile'} for r in results], indent=2))
