import json
from pathlib import Path
import subprocess
import sys
from threading import Thread

import pytest
from PySide6.QtCore import QObject
from PySide6.QtTest import QTest

from fake_data import ConsoleState
from main import create_engine
from rotation import configure_rotation
from surface_batch import SurfaceBatcher
from tools.perf_gate import arguments, parse_qsg_timing, synthetic_event, cpu_seconds

ROOT = Path(__file__).resolve().parents[1]


def test_surface_fifo_once_per_frame_without_loss(app):
    state = ConsoleState()
    batcher = SurfaceBatcher(state)
    changes, batches = [], []
    state.channels.dataChanged.connect(lambda *args: changes.append(args))
    batcher.on_batch = lambda events: batches.append([event.sequence for event in events])
    # Clamp after each delta: reversing this sequence produces a different result.
    for delta in [100, -50] + [1, -1]*49:
        batcher.enqueue(dict(type='Encoder', indice=0, delta=delta))
    QTest.qWait(20)
    assert changes == []  # Independent timers cannot flush the managed models.
    assert state.channels.rows[0]['level'] == .82
    batcher.drain_frame()
    assert state.channels.rows[0]['level'] == pytest.approx(.5)
    assert len(changes) == 1
    assert batches == [list(range(1, 101))]
    assert batcher.applied == batcher.enqueued == 100
    assert batcher.pending == 0
    batcher.drain_frame()
    assert len(changes) == 1
    batcher.enqueue(dict(type='Encoder', indice=0, delta=1))
    batcher.drain_frame()
    assert len(changes) == 2
    assert state.channels.rows[0]['level'] == pytest.approx(.51)


def test_surface_thread_producer_and_all_types(app):
    state = ConsoleState()
    batcher = SurfaceBatcher(state)
    changes = {name: [] for name in ('channels', 'cells', 'groups', 'hands')}
    for name, signals in changes.items():
        getattr(state, name).dataChanged.connect(lambda *args, target=signals: target.append(args))
    seen = []
    batcher.on_batch = lambda events: seen.extend(event.sequence for event in events)
    producer = Thread(target=lambda: [batcher.enqueue(synthetic_event(i)) for i in range(2000)])
    producer.start()
    while producer.is_alive():
        batcher.drain_frame()
    producer.join()
    batcher.drain_frame()
    assert seen == list(range(1, 2001))
    assert batcher.enqueued == batcher.applied == 2000
    assert batcher.pending == 0
    assert len(batcher.kind_counts) == 10
    assert all(len(signals) <= batcher.batch_count for signals in changes.values())


def test_real_surface_dataclass_contract(app):
    from dataclasses import make_dataclass
    encoder = make_dataclass('Encoder', [('indice', int), ('delta', int)], frozen=True)
    state = ConsoleState()
    batcher = SurfaceBatcher(state)
    batcher.enqueue(encoder(0, 2))
    batcher.drain_frame()
    assert state.channels.rows[0]['level'] == pytest.approx(.84)
    with pytest.raises(ValueError, match='Unsupported'):
        batcher.enqueue(dict(type='Unknown'))


def test_window_drains_surface_on_frame(gui):
    engine, state, window, _ = gui
    batcher = engine.surface_batcher
    batcher.enqueue(dict(type='Fader', indice=0, valor=.23))
    assert state.groups.rows[0]['level'] != .23
    # enqueue must wake an otherwise idle window without a polling timer.
    QTest.qWait(80)
    assert batcher.applied == 1
    assert state.groups.rows[0]['level'] == .23


@pytest.mark.parametrize('angle', [0, 90, 180, 270])
def test_rotation_parsing_and_qml_geometry(app, angle, monkeypatch):
    monkeypatch.setenv('QT_QPA_PLATFORM', 'offscreen')
    args = arguments(['--rotate', str(angle), '--rotation-method', 'qml'])
    rotation = configure_rotation(args.rotate, args.rotation_method)
    engine, _ = create_engine(rotation=rotation)
    try:
        window = engine.rootObjects()[0]
        scene = window.findChild(QObject, 'rotatedScene')
        assert scene.property('rotation') == angle
        assert (window.width(), window.height()) == ((720, 1280) if angle % 180 else (1280, 720))
        assert scene.property('scale') == 1
    finally:
        import shiboken6
        shiboken6.delete(engine)


def test_rotation_env_limitation_and_invalid_flags(monkeypatch):
    monkeypatch.setenv('QT_QPA_EGLFS_ROTATION', '0')
    rotation = configure_rotation(270, 'eglfs')
    import os
    assert os.environ['QT_QPA_EGLFS_ROTATION'] == '-90'
    assert rotation['active_method'] == 'eglfs-env-unverified'
    assert rotation['qml_angle'] == 0
    for argv in (['--rotate', '45'], ['--rotation-method', 'invalid'], ['--rate', '0'],
                 ['--duration', 'nan'], ['--warmup', '-1']):
        with pytest.raises(SystemExit):
            arguments(argv)


def test_qsg_total_parser_and_cpu():
    assert parse_qsg_timing('syncAndRender: frame rendered in 15ms, sync=2, render=3, swap=10') == 15
    assert parse_qsg_timing('Frame rendered in 4.25 ms') == 4.25
    assert parse_qsg_timing("Frame rendered with 'software' renderloop in 21ms, polish=1, sync=2, render=18, swap=0") == 21
    assert parse_qsg_timing('render=3, swap=10') is None
    assert cpu_seconds() >= 0


def test_perf_smoke_three_seconds(tmp_path):
    output = tmp_path/'gate.json'
    result = subprocess.run([sys.executable, str(ROOT/'tools/perf_gate.py'), '--offscreen',
                             '--duration', '3', '--warmup', '0', '--rate', '1000',
                             '--qsg-timing', '--output', str(output)],
                            capture_output=True, text=True, timeout=40)
    assert result.returncode == 1, result.stdout + result.stderr  # Smoke never certifies Pi.
    report = json.loads(output.read_text())
    assert {'environment', 'rotation', 'workload', 'metrics', 'events', 'samples',
            'checks', 'passed', 'warnings', 'limitations', 'timestamp_utc'} <= report.keys()
    assert {'fps', 'fps_min_1s', 'frame_interval_ms', 'render_callback_ms', 'qsg_frame_ms',
            'event_to_frame_ms', 'cpu_percent', 'cpu_max_1s_percent', 'cpu_source'} <= report['metrics'].keys()
    assert report['metrics']['frame_interval_ms']['count'] > 0
    assert report['metrics']['event_to_frame_ms']['count'] > 0
    assert report['events']['enqueued'] > 0
    assert report['events']['enqueued'] == report['events']['applied'] == report['events']['presented']
    assert report['events']['pending'] == 0
    assert report['workload']['simultaneous'] is True
    assert report['warnings'] == []
    assert report['passed'] is False
    assert 'not a Raspberry Pi 5' in report['checks']['eligibility']['value']
    assert output.with_suffix('.qt.log').exists()


def test_latency_waits_for_synchronized_frame(app):
    from time import perf_counter_ns
    from tools.perf_gate import Measurements
    state = ConsoleState()
    batcher = SurfaceBatcher(state)
    measurements = Measurements(batcher)
    measurements.start_ns = perf_counter_ns() - 1_000_000
    measurements.end_ns = perf_counter_ns() + 1_000_000_000
    batcher.on_batch = measurements.batch
    batcher.enqueue(dict(type='Fader', indice=0, valor=.4))
    batcher.drain_frame()
    measurements.swapped()
    assert measurements.presented == 0
    assert measurements.latencies == []
    measurements.sync()
    measurements.swapped()
    assert measurements.presented == 1
    assert len(measurements.latencies) == 1
