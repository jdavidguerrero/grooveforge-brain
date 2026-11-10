#!/usr/bin/env python3
"""ADR-007 instrumented visual workload. Offscreen/Mac results never certify Pi."""
import argparse
from collections import deque
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import platform
import re
import resource
import sys
from threading import Event, Lock, Thread
from time import perf_counter_ns

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from rotation import add_rotation_arguments, configure_rotation


def positive(value):
    number = float(value)
    if not math.isfinite(number) or number <= 0:
        raise argparse.ArgumentTypeError('must be finite and positive')
    return number


def arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--duration', type=positive, default=120.)
    parser.add_argument('--rate', type=positive, default=1000.)
    parser.add_argument('--warmup', type=float, default=2.)
    parser.add_argument('--offscreen', action='store_true')
    parser.add_argument('--cycle-screens', action='store_true')
    parser.add_argument('--qsg-timing', action='store_true')
    parser.add_argument('--output', type=Path, default=ROOT/'perf-results'/'latest.json')
    parser.add_argument('--display-screen')
    add_rotation_arguments(parser)
    args = parser.parse_args(argv)
    if not math.isfinite(args.warmup) or args.warmup < 0:
        parser.error('--warmup must be finite and nonnegative')
    return args


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered)*fraction)-1)]


def distribution(values):
    return dict(count=len(values), p50=percentile(values, .5), p95=percentile(values, .95),
                p99=percentile(values, .99), maximum=max(values) if values else None)


def parse_qsg_timing(message):
    # Qt 6.8 threaded renderloop's total includes sync, render and swap.
    match = re.search(r"frame rendered (?:with '[^']+' renderloop )?in ([\d.]+)\s*ms", message, re.I)
    if match:
        return float(match[1])
    return None


def cpu_seconds():
    if sys.platform.startswith('linux'):
        # comm may contain spaces/parentheses; fields after final ')' start at field 3.
        fields = Path('/proc/self/stat').read_text().rsplit(')', 1)[1].split()
        return (int(fields[11]) + int(fields[12])) / os.sysconf('SC_CLK_TCK')
    usage = resource.getrusage(resource.RUSAGE_SELF)
    return usage.ru_utime + usage.ru_stime


def synthetic_event(number):
    """Deterministic 40% encoders, 20% faders, 10% each FSR/hands/pads, 10% others."""
    slot = number % 20
    value = (math.sin(number*.07) + 1) / 2
    if slot < 8:
        return dict(type='Encoder', indice=(number//20+slot) % 8, delta=1 if number % 40 < 20 else -1)
    if slot < 12:
        return dict(type='Fader', indice=slot-8, valor=value)
    if slot < 14:
        return dict(type='Fsr', indice=(number//20+slot) % 4, presionado=bool(number % 40 < 20), velocidad=value)
    if slot < 16:
        return dict(type='Mano', indice=slot-14, valor=value)
    if slot < 18:
        # First row is playing and its level is visible in LIVE.
        return dict(type='Pad', indice=(number//20+slot) % 8, presionado=bool(number % 40 < 20), velocidad=value)
    kind = ('Jog', 'Expresion', 'Perilla', 'ManoPresencia', 'Boton')[(number//20) % 5]
    return {'Jog': dict(type=kind, delta=1),
            'Expresion': dict(type=kind, indice=number % 2, valor=value),
            'Perilla': dict(type=kind, nombre='MASTER', valor=value),
            'ManoPresencia': dict(type=kind, indice=number % 2, presente=bool(number % 40 < 20)),
            'Boton': dict(type=kind, nombre='PLAY', presionado=bool(number % 40 < 20))}[kind]


class Producer(Thread):
    def __init__(self, batcher, rate):
        super().__init__(name='synthetic-surface', daemon=True)
        self.batcher, self.rate, self.stop_event = batcher, rate, Event()
        self.generated = 0
        self.error = None
        self.start_ns = self.end_ns = 0

    def run(self):
        start = self.start_ns or perf_counter_ns()
        try:
            while not self.stop_event.is_set():
                now = perf_counter_ns()
                if self.end_ns and now >= self.end_ns:
                    break
                due = int((now-start) / 1e9 * self.rate)
                for _ in range(min(due-self.generated, 5000)):
                    self.batcher.enqueue(synthetic_event(self.generated))
                    self.generated += 1
                self.stop_event.wait(.002)
        except Exception as error:
            self.error = repr(error)


class Measurements:
    def __init__(self, batcher):
        self.batcher = batcher
        self.lock = Lock()
        self.start_ns = self.end_ns = 0
        self.swaps, self.render_ms, self.qsg_ms, self.latencies = [], [], [], []
        self.frames = []
        self.waiting = deque()
        self.synced_serial = self.render_start = self.render_end = self.presented = 0
        self.qsg_lines = 0
        self.warnings = []

    def active(self, now):
        return self.start_ns and self.start_ns <= now <= self.end_ns

    def batch(self, events):
        with self.lock:
            self.waiting.append(events)

    def sync(self):
        # DirectConnection: GUI is blocked during sync. Only plain Python state here.
        self.synced_serial = self.batcher._serial

    def before_render(self):
        self.render_start = perf_counter_ns()

    def after_render(self):
        now = perf_counter_ns()
        self.render_end = now
        if self.active(now) and self.render_start:
            self.render_ms.append((now-self.render_start)/1e6)

    def swapped(self):
        now = perf_counter_ns()
        with self.lock:
            acknowledged = 0
            while self.waiting and self.waiting[0][-1].sequence <= self.synced_serial:
                batch = self.waiting.popleft()
                for event in batch:
                    if self.start_ns <= event.enqueued_ns <= self.end_ns:
                        self.latencies.append((now-event.enqueued_ns)/1e6)
                        acknowledged += 1
                self.presented += len(batch)
            if self.active(now):
                self.swaps.append(now)
                self.frames.append(dict(t_ms=(now-self.start_ns)/1e6,
                                        batch_serial=self.synced_serial, acknowledged_events=acknowledged,
                                        before_render_ms=(self.render_start-self.start_ns)/1e6,
                                        after_render_ms=(self.render_end-self.start_ns)/1e6))


def evaluate(report):
    def check(value, limit, less=True):
        return 'UNVERIFIED' if value is None else 'PASS' if (value < limit if less else value >= limit) else 'FAIL'
    metrics = report['metrics']
    checks = {
        'fps_sustained': dict(value=metrics['fps_min_1s'], target='>=60 every complete 1s window',
                              status=check(metrics['fps_min_1s'], 60, False)),
        'frame_p99_qsg_ms': dict(value=metrics['qsg_frame_ms']['p99'], target='<16.7 ms',
                                 status=check(metrics['qsg_frame_ms']['p99'], 16.7)),
        'event_latency_p95_ms': dict(value=metrics['event_to_frame_ms']['p95'], target='<32.6667 ms',
                                    status=check(metrics['event_to_frame_ms']['p95'], 1000/60+16)),
        'cpu_one_core_percent': dict(value=metrics['cpu_percent'], target='<25% of one core',
                                     status=check(metrics['cpu_percent'], 25)),
    }
    reasons = []
    env, workload, events = report['environment'], report['workload'], report['events']
    if env['system'] != 'Linux' or not env['is_pi5']:
        reasons.append('not a Raspberry Pi 5')
    if env['qpa_platform'] != 'eglfs' or 'OpenGL' not in env['graphics_api']:
        reasons.append('requires EGLFS/OpenGL hardware rendering')
    if workload['duration_s'] < 120:
        reasons.append('duration below 120 seconds (smoke only)')
    if workload['rate_requested'] < 1000 or events['rate_actual'] < workload['rate_requested']*.99:
        reasons.append('insufficient event rate')
    if (events['enqueued'] != events['applied'] or events['enqueued'] != events['presented']
            or events['pending'] or metrics['event_to_frame_ms']['count'] != events['enqueued']):
        reasons.append('events not fully applied/presented')
    if report['rotation']['active_method'] == 'eglfs-env-unverified':
        reasons.append('EGLFS environment rotation does not apply to Qt Quick')
    if not all(env['fonts'].values()):
        reasons.append('missing local fonts')
    if report['warnings'] or events['producer_error']:
        reasons.append('Qt warning or producer error')
    if not metrics['qsg_frame_ms']['count']:
        reasons.append('no recognized QSG renderloop total samples')
    elif metrics['qsg_frame_ms']['count'] < len(report['samples']['frames'])*.95:
        reasons.append('incomplete QSG frame timing coverage')
    checks['eligibility'] = dict(value=reasons, target='Pi5 complete workload', status='UNVERIFIED' if reasons else 'PASS')
    passed = all(item['status'] == 'PASS' for item in checks.values())
    return checks, passed


def main(argv=None):
    args = arguments(argv)
    if args.offscreen:
        os.environ['QT_QPA_PLATFORM'] = 'offscreen'
        os.environ['QT_QUICK_BACKEND'] = 'software'
    if args.qsg_timing:
        os.environ['QSG_RENDER_TIMING'] = '1'
        os.environ['QT_LOGGING_RULES'] = os.environ.get('QT_LOGGING_RULES', '')+';qt.scenegraph.time.renderloop=true'
    rotation = configure_rotation(args.rotate, args.rotation_method)
    from PySide6 import __version__ as pyside_version
    from PySide6.QtCore import QTimer, Qt, qInstallMessageHandler, QtMsgType, qVersion
    from PySide6.QtGui import QGuiApplication
    from main import create_engine
    measurements = Measurements(None)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    log = args.output.with_suffix('.qt.log').open('w')
    def message_handler(kind, context, message):
        now = perf_counter_ns()
        with measurements.lock:
            log.write(f'{(now-measurements.start_ns)/1e6 if measurements.start_ns else -1:.3f} [{context.category}] {message}\n')
            if measurements.active(now):
                timing = parse_qsg_timing(message)
                if timing is not None:
                    measurements.qsg_ms.append(timing)
                measurements.qsg_lines += int('render' in message.lower())
            if kind in (QtMsgType.QtWarningMsg, QtMsgType.QtCriticalMsg, QtMsgType.QtFatalMsg):
                measurements.warnings.append(message)
    previous = qInstallMessageHandler(message_handler)
    app = QGuiApplication(sys.argv[:1])
    engine, state = create_engine(frameless=True, rotation=rotation, perf_mode=True, display_screen=args.display_screen, cycle_screens=args.cycle_screens)
    if not engine.rootObjects():
        raise RuntimeError('QML window failed to load')
    window = engine.rootObjects()[0]
    measurements.batcher = engine.surface_batcher
    engine.surface_batcher.on_batch = measurements.batch
    window.beforeSynchronizing.connect(measurements.sync, Qt.DirectConnection)
    window.beforeRendering.connect(measurements.before_render, Qt.DirectConnection)
    window.afterRendering.connect(measurements.after_render, Qt.DirectConnection)
    window.frameSwapped.connect(measurements.swapped, Qt.DirectConnection)
    producer = Producer(engine.surface_batcher, args.rate)
    cpu_samples = []
    cpu_base = [0., 0, 0., 0]
    sampler = QTimer()
    sampler.setInterval(1000)
    sampler.setTimerType(Qt.PreciseTimer)
    def sample_cpu():
        now, cpu = perf_counter_ns(), cpu_seconds()
        cpu_samples.append(100*(cpu-cpu_base[2])/((now-cpu_base[3])/1e9))
        cpu_base[2:] = [cpu, now]
    sampler.timeout.connect(sample_cpu)
    cycler = QTimer()
    cycler.setInterval(5000)
    cycler.timeout.connect(lambda: state.cycleScreen(1))
    def finish():
        producer.stop_event.set()
        producer.join(timeout=2)
        sampler.stop()
        cycler.stop()
        cpu_base[0] = cpu_seconds()-cpu_base[0]
        # Allow the final queue and scenegraph sync to complete, but exclude cooldown frames.
        QTimer.singleShot(250, app.quit)
    deadline_timer = QTimer()
    deadline_timer.setSingleShot(True)
    deadline_timer.setTimerType(Qt.PreciseTimer)
    deadline_timer.timeout.connect(finish)
    def start():
        now = perf_counter_ns()
        measurements.start_ns = now
        measurements.end_ns = now + int(args.duration*1e9)
        cpu = cpu_seconds()
        cpu_base[:] = [cpu, now, cpu, now]
        producer.start_ns, producer.end_ns = now, measurements.end_ns
        producer.start()
        sampler.start()
        if args.cycle_screens:
            cycler.start()
        deadline_timer.start(round(args.duration*1000))
    # A render request every ~frame also supports software/offscreen and a quiet surface.
    ticker = QTimer()
    ticker.setInterval(16)
    ticker.setTimerType(Qt.PreciseTimer)
    ticker.timeout.connect(window.update)
    ticker.start()
    QTimer.singleShot(round(args.warmup*1000), start)
    app.exec()
    ticker.stop()
    qInstallMessageHandler(previous)
    log.close()
    intervals = [(b-a)/1e6 for a,b in zip(measurements.swaps, measurements.swaps[1:])]
    seconds = [0]*int(args.duration)
    for timestamp in measurements.swaps:
        bucket = int((timestamp-measurements.start_ns)/1e9)
        if bucket < len(seconds):
            seconds[bucket] += 1
    pi_model = Path('/proc/device-tree/model')
    model = pi_model.read_text().strip('\x00') if pi_model.exists() else ''
    report = dict(schema_version=1, timestamp_utc=datetime.now(timezone.utc).isoformat(),
        environment=dict(system=platform.system(), machine=platform.machine(), python=platform.python_version(),
                         qt=qVersion(), pyside=pyside_version, pi_model=model, is_pi5='Raspberry Pi 5' in model,
                         qpa_platform=app.platformName(), graphics_api=window.rendererInterface().graphicsApi().name,
                         render_loop=os.environ.get('QSG_RENDER_LOOP', 'Qt default'),
                         qsg_render_timing=os.environ.get('QSG_RENDER_TIMING', '0'),
                         kms_config=os.environ.get('QT_QPA_EGLFS_KMS_CONFIG'),
                         touch_evdev=os.environ.get('QT_QPA_EVDEV_TOUCHSCREEN_PARAMETERS'),
                         touch_matrix=os.environ.get('QT_QPA_LIBINPUT_TOUCH_MATRIX'),
                         display=dict(name=window.screen().name(), width=window.width(), height=window.height(),
                                      refresh_rate=window.screen().refreshRate()),
                         fonts={name: (ROOT/'fonts'/name).exists() for name in
                                ('GeneralSans.ttf','BigShouldersDisplay.ttf','IBMPlexMono-Regular.ttf',
                                 'IBMPlexMono-Medium.ttf','IBMPlexMono-SemiBold.ttf','IBMPlexMono-Bold.ttf',
                                 'Archivo.ttf','JetBrainsMono.ttf')}),
        rotation=rotation,
        workload=dict(duration_s=args.duration, warmup_s=args.warmup, rate_requested=args.rate,
                      screens=['LiveSession','Mixer'], simultaneous=True,
                      composition='full-resolution content textures, half-scale side-by-side previews',
                      cycle_screens=args.cycle_screens),
        metrics=dict(fps=len(measurements.swaps)/args.duration, fps_min_1s=min(seconds) if seconds else None,
                     fps_windows_1s=seconds, frame_interval_ms=distribution(intervals),
                     render_callback_ms=distribution(measurements.render_ms),
                     qsg_render_log_lines=measurements.qsg_lines,
                     qsg_frame_ms=distribution(measurements.qsg_ms),
                     event_to_frame_ms=distribution(measurements.latencies),
                     cpu_percent=100*cpu_base[0]/args.duration,
                     cpu_max_1s_percent=max(cpu_samples) if cpu_samples else None,
                     cpu_samples_percent=cpu_samples,
                     cpu_source='/proc/self/stat' if sys.platform.startswith('linux') else 'resource.RUSAGE_SELF'),
        events=dict(enqueued=engine.surface_batcher.enqueued, applied=engine.surface_batcher.applied,
                    presented=measurements.presented, pending=engine.surface_batcher.pending,
                    batch_count=engine.surface_batcher.batch_count, max_queue_depth=engine.surface_batcher.max_depth,
                    kinds=engine.surface_batcher.kind_counts, producer_error=producer.error,
                    rate_actual=producer.generated/args.duration),
        samples=dict(frames=measurements.frames, render_ms=measurements.render_ms, qsg_frame_ms=measurements.qsg_ms),
        warnings=measurements.warnings,
        limitations=['frameSwapped is not optical scanout', 'coalesced values acknowledged with final batch state',
                     'two content FBO previews differ from standalone fill rate', 'physical rotation/touch unverified'])
    report['checks'], report['passed'] = evaluate(report)
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('CHECK                       RESULT       VALUE / TARGET')
    for name, item in report['checks'].items():
        print(f"{name:27} {item['status']:12} {item['value']} / {item['target']}")
    print(f"JSON: {args.output}\nQt log: {args.output.with_suffix('.qt.log')}")
    print(json.dumps({k:v for k,v in report.items() if k != 'samples'}))
    import shiboken6
    # Disconnect Python callbacks before scenegraph teardown.
    window.beforeSynchronizing.disconnect(measurements.sync)
    window.beforeRendering.disconnect(measurements.before_render)
    window.afterRendering.disconnect(measurements.after_render)
    window.frameSwapped.disconnect(measurements.swapped)
    shiboken6.delete(engine)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
