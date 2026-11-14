import pytest
from PySide6.QtCore import QModelIndex, Qt
from PySide6.QtTest import QSignalSpy
from fake_data import ConsoleState, ROLES, CELL_ROLES, CHANNEL_ROLES, KEY_ROLES, OPTION_ROLES


@pytest.mark.parametrize('name,count,roles', [
    ('cells',32,CELL_ROLES),('channels',8,CHANNEL_ROLES),('options',4,OPTION_ROLES),
    ('softKeys',8,KEY_ROLES),('groups',4,'title level meter members'.split()),
    ('hands',2,'side target distance inRange value rangeMin rangeMax'.split()),
    ('connections',4,['title','status']),('toasts',2,'icon message value tone opacity'.split())])
def test_model_contract(app, name, count, roles):
    # Keep the QObject owner alive while reading its child models.
    state = ConsoleState()
    model = getattr(state, name)
    assert model.rowCount() == count
    assert model.rowCount(model.index(0)) == 0
    assert {x.decode() for x in model.roleNames().values()} == set(roles)
    assert model.data(QModelIndex(), Qt.UserRole+1) is None
    for i in range(count):
        for role in model.roleNames():
            assert model.data(model.index(i), role) is not None


def test_fake_data_integrity(app):
    state = ConsoleState()
    assert (state.roomTitle,state.nextTitle,state.keyName,state.bpm) == ('Selva Nocturna','Kalimba Dusk','A MIN',124.)
    assert [r['roleName'] for r in state.channels.rows] == ROLES
    assert [g['title'] for g in state.groups.rows] == ['RHYTHM','BASS','MUSIC','FX']
    assert state.groups.rows[0]['members'] == ROLES[:4]
    assert state.groups.rows[2]['members'] == ROLES[5:]
    for i, row in enumerate(state.cells.rows):
        assert row['channel'] == i%8 and row['row'] == i//8
        assert len(row['steps']) == 16 and set(row['steps']) <= {0,1}
        assert 0 <= row['level'] <= 1
    assert {r['cellState'] for r in state.cells.rows} == {'playing','queued','cue','idle','option','muted','empty','loading'}
    for option in state.options.rows:
        assert 1 <= option['energy'] <= 5
        for note in option['notes']:
            assert 0 <= note['s'] < 16 and 1 <= note['l'] <= 16-note['s']
            assert 0 <= note['n'] < 7


def test_frame_coalescing_and_bounds(app):
    state = ConsoleState()
    spy = QSignalSpy(state.channels.dataChanged)
    for _ in range(1000):
        state.turnEncoder(0,1)
    assert spy.count() == 0
    state.channels.flush()
    assert spy.count() == 1
    assert state.channels.rows[0]['level'] == 1
    for _ in range(1000):
        state.turnEncoder(0,-1)
    state.channels.flush()
    assert state.channels.rows[0]['level'] == 0
    with pytest.raises(ValueError):
        state.channels.queue_update(0, {'unknown':123})
    with pytest.raises(IndexError):
        state.channels.queue_update(8, {'level':1})


@pytest.mark.parametrize('live_state', ['normal','offline','empty','loading'])
def test_live_states_keep_room_and_next(app,live_state):
    state = ConsoleState()
    before = state.cells.rows[:16]
    state.setLiveState(live_state)
    state.connections.flush()
    assert state.cells.rows[:16] == before
    if live_state != 'normal':
        assert {r['cellState'] for r in state.cells.rows[16:]} == {'loading' if live_state == 'loading' else 'empty'}
    # ENGINE offline is independent of the Teensy BRAIN connection.
    assert state.connections.rows[0]['status'] == 'ok'


def test_soft_key_navigation_and_actions(app):
    state = ConsoleState()
    state.setScreen(2)
    state.pressEncoder(1)
    assert state.screen == 5
    state.pressEncoder(4)
    assert state.screen == 6
    assert {'accept','generate','skip'} <= {k['action'] for k in state.softKeys.rows}
    state.pressEncoder(4)
    state.options.flush()
    assert state.options.rows[3]['title'] == 'Juno Sub'
    state.pressEncoder(5)
    state.cells.flush()
    assert state.screen == 5 and state.cells.rows[12]['cellState'] == 'queued'
    state.setScreen(7)
    assert 'drop' in {k['action'] for k in state.softKeys.rows}
    for screen in range(1,10):
        state.setScreen(screen)
        assert state.softKeys.rowCount() == 8


def test_control_property_notifications_are_batched(app):
    state = ConsoleState()
    spy = QSignalSpy(state.changed)
    for i in range(1000):
        state.turnEncoder(i%8,1)
    assert spy.count() == 0
    state.flush_properties()
    assert spy.count() == 1
    state.setScreen(6)
    state.turnEncoder(0,1)
    state.options.flush()
    assert state.options.rows[0]['energy'] == 4
    state.pressEncoder(0)
    state.pressEncoder(5)
    state.cells.flush()
    assert state.cells.rows[12]['title'] == 'Sub Prophet'
