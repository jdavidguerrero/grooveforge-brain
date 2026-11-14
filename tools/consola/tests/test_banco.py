from types import SimpleNamespace
import pytest
import banco


def client(**kwargs):
    port = banco.SimulatedPort(**kwargs)
    return banco.Client(port, port.clock)


def test_source_checklist_has_exactly_26_keys_and_all_controls():
    plan = banco.checklist()
    assert len(plan['pcb-k-izq']) == 17
    assert len(plan['pcb-k-der']) == 9
    assert len(plan['pcb-a']) == 16
    assert len(plan['pcb-p']) == 6
    assert len(plan['pcb-f']) == 4
    assert len(plan['pcb-m']) == 9
    notes = [c.index for board in ('pcb-k-izq', 'pcb-k-der') for c in plan[board]]
    assert len(set(notes)) == 26
    assert set(notes) == {n for row in banco.note_map()[1:] for n in row if n not in (255, 88, 122)}


def test_discovery_product_description_missing_and_ambiguous():
    ports = [SimpleNamespace(device='a', product='GF Consola', description='USB Serial')]
    assert banco.discover_port(ports) == 'a'
    assert banco.discover_port([SimpleNamespace(device='b', description='GF Consola CDC')]) == 'b'
    with pytest.raises(RuntimeError):
        banco.discover_port([])
    with pytest.raises(RuntimeError):
        banco.discover_port(ports * 2)
    with pytest.raises(RuntimeError):
        banco.discover_port([SimpleNamespace(device='c', product='GF Superficie')])


def test_all_boards_simulated_and_led_cleanup():
    c = client()
    results = banco.run(c, 'todo', confirm=lambda _: 's', output=lambda _: None)
    assert results and all(r.passed for r in results)
    assert sum(r.name == 'SCAN' for r in results) == 6
    assert c.port.writes.count('@GF KEYLEDS 26') == 2
    assert c.port.writes.count('@GF KEYLEDS 0') == 2
    assert not c.port.watch


@pytest.mark.parametrize('board', banco.BOARDS)
def test_per_board_scan_includes_dependencies(board):
    c = client(addresses=banco.EXPECTED[board])
    results = banco.run(c, board, confirm=lambda _: 's', output=lambda _: None)
    assert all(r.passed for r in results)


def test_missing_devices_fail_but_extra_devices_allowed():
    c = client(addresses={0x20, 0x77})
    results = banco.run(c, 'pcb-k-der', confirm=lambda _: 's', output=lambda _: None)
    scan = next(r for r in results if r.name == 'SCAN')
    assert not scan.passed and '21' in scan.detail
    assert '77' in scan.detail


@pytest.mark.parametrize('fault', ['silent', 'wrong'])
def test_no_event_or_wrong_control_cannot_pass(fault):
    c = client(**{fault: ['F1']})
    control = banco.Control('F1', 'ADS', 0)
    passed, detail = banco.exercise(c, control, 0.2, banco.note_map(), output=lambda _: None)
    assert not passed and 'Tiempo agotado' in detail


def test_inactive_profile_and_unread_inputs_fail():
    c = client()
    c.port.snap['ADS_VALID'] = 0
    passed, detail = banco.exercise(c, banco.Control('F1', 'ADS', 0), 0.2, banco.note_map(), output=lambda _: None)
    assert not passed and 'Sin muestra' in detail


def test_lost_events_are_reported():
    c = client(drops=1)
    results = banco.run(c, 'pcb-f', output=lambda _: None)
    integrity = next(r for r in results if r.name == 'Integridad WATCH')
    assert not integrity.passed and '4' in integrity.detail


def test_rejected_led_and_exception_still_turn_leds_off():
    c = client()
    results = banco.run(c, 'pcb-k-izq', confirm=lambda _: 'n', output=lambda _: None)
    assert not next(r for r in results if r.name == 'LEDs').passed
    assert c.port.writes[-1] == '@GF KEYLEDS 0'
    def interrupt(_):
        raise KeyboardInterrupt()
    with pytest.raises(KeyboardInterrupt):
        banco.run(c, 'pcb-k-der', confirm=interrupt, output=lambda _: None)
    assert c.port.writes[-1] == '@GF KEYLEDS 0'


def test_snapshot_preserves_raw_signed_and_bit_values():
    c = client()
    c.port.snap['ADS'][7] = -32768
    c.port.snap['ENC'][8] = -2147483648
    c.port.snap['M4_BITS'] = 0x80000001
    snap = c.snapshot()
    assert snap['ADS'][7] == -32768
    assert snap['ENC'][8] == -2147483648
    assert snap['M4_BITS'] == 0x80000001
    with pytest.raises(ValueError):
        banco.parse_inputs('@GF OK INPUTS MCP=0')


def test_command_errors_incomplete_lines_and_timeout():
    c = client()
    with pytest.raises(RuntimeError, match='COMANDO'):
        # Generic firmware errors must fail promptly, too.
        c.command('BAD', timeout=0.1)


def test_report_simulated_and_partial(tmp_path):
    path = tmp_path / 'banco.md'
    banco.report(path, 'pcb-a', 'sim|port', True,
                 [banco.Result('pcb-a', 'E1', False, 'sin\nevento')], 'desconexión')
    text = path.read_text()
    assert 'SIMULADO' in text and 'FALLA / INCOMPLETO' in text
    assert 'desconexión' in text and 'sim\\|port' in text
    assert 'no certifica' in text


def test_main_simulation_report_and_watch_cleanup(tmp_path, monkeypatch):
    monkeypatch.setattr(banco, 'ROOT', tmp_path)
    port = banco.SimulatedPort()
    monkeypatch.setattr(banco, 'SimulatedPort', lambda: port)
    # Keep isinstance meaningful when replacing the port factory.
    monkeypatch.setattr(banco, 'run', lambda c, *a, results, **kw: results.append(banco.Result('pcb-f', 'F1', True, 'simulado')))
    assert banco.main(['--placa', 'pcb-f', '--simular']) == 0
    assert port.closed and port.writes[-1] == '@GF WATCH 0'
    report = next((tmp_path / 'apps/docs/consola/mediciones').glob('banco-pcb-f-*.md'))
    assert 'SIMULADO' in report.read_text()


def test_main_disconnect_keeps_partial_report_and_cleans_watch(tmp_path, monkeypatch):
    port = banco.SimulatedPort(disconnect='F1')
    monkeypatch.setattr(banco, 'ROOT', tmp_path)
    # A real serial constructor returning our fake preserves its type and clock.
    import serial
    monkeypatch.setattr(serial, 'Serial', lambda *a, **kw: port)
    assert banco.main(['--placa', 'pcb-f', '--puerto', 'fake']) == 1
    assert port.closed and port.writes[-1] == '@GF WATCH 0'
    text = next(tmp_path.rglob('banco-*.md')).read_text()
    assert 'Desconexión simulada' in text and 'INCOMPLETO' in text


@pytest.mark.parametrize('value', ['0', '-1', 'nan', 'inf'])
def test_invalid_timeout(value):
    with pytest.raises(SystemExit):
        banco.main(['--placa', 'pcb-f', '--timeout', value])


@pytest.mark.parametrize('control,values', [
    (banco.Control('PLAY', 'KEY', 94), [1]),
    (banco.Control('PLAY', 'KEY', 94), [0]),
    (banco.Control('E1', 'ENC', 0), [4, 8, 12]),
    (banco.Control('F1', 'ADS', 0), [1100, 1200, 1300]),
    (banco.Control('SEL_PAD', 'SEL', 0), [2048]),
])
def test_incomplete_gesture_never_passes(control, values):
    class PartialPort(banco.SimulatedPort):
        def exercise(self, c, baseline):
            for value in values:
                self.enqueue(f'@GF OK EVENT {c.kind} {c.index} {value}')
    port = PartialPort()
    c = banco.Client(port, port.clock)
    passed, _ = banco.exercise(c, control, 0.15, banco.note_map(), output=lambda _: None)
    assert not passed


def test_watch_renewal_for_long_deadline():
    port = banco.SimulatedPort(silent=['F1'])
    c = banco.Client(port, port.clock)
    passed, _ = banco.exercise(c, banco.Control('F1', 'ADS', 0), 125, banco.note_map(), output=lambda _: None)
    assert not passed
    assert port.writes.count('@GF WATCH 1') == 2


def test_wrong_watch_ack_is_not_accepted():
    port = banco.SimulatedPort()
    port.enqueue('@GF OK WATCH 0 EXPIRED')
    c = banco.Client(port, port.clock)
    assert c.command('WATCH 1') == '@GF OK WATCH 1'
    assert not port.rx


def test_protocol_rejects_partial_line_and_unresponsive_device():
    c = client()
    c.port.rx.append(b'@GF OK INPU')
    with pytest.raises(RuntimeError, match='incompleta'):
        c.snapshot()
    class NoReply(banco.SimulatedPort):
        def write(self, data):
            return len(data)
    port = NoReply()
    with pytest.raises(TimeoutError):
        banco.Client(port, port.clock).command('SCAN', timeout=0.05)


def test_encoder_wrap_is_a_single_direction_not_false_both():
    class WrapPort(banco.SimulatedPort):
        def exercise(self, c, baseline):
            self.enqueue('@GF OK EVENT ENC 0 -2147483648')
    port = WrapPort()
    port.snap['ENC'][0] = 2147483647
    passed, _ = banco.exercise(banco.Client(port, port.clock), banco.Control('E1', 'ENC', 0),
                              0.15, banco.note_map(), output=lambda _: None)
    assert not passed
