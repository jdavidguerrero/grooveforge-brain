"""LED27 intentions in split PCB-K chain order (doc27); no animation timer."""
from dataclasses import dataclass

OFF, DIM, ON, BLINK, PULSE = range(5)
TEAL, VIOLET, BLUE, AMBER, WHITE = range(5)
# Single Python source for wire order and CSV centres (doc27 §1).
KEY_CHAIN = (
    ('BACK', 64, 305),
    ('MENU', 64, 286.5),
    ('NAV_L', 64, 268),
    ('NAV_R', 64, 249.5),
    ('BROWSE', 64, 231),
    ('TAP', 58, 174),
    ('FILL', 39, 174),
    ('CUE_ENG', 20, 174),
    ('BUILD', 20, 155),
    ('DROP', 39, 155),
    ('BREAK', 58, 155),
    ('SHIFT', 20, 132.5),
    ('UNDO', 39, 132.5),
    ('DELETE', 98, 160.5),
    ('DUPLICATE', 98, 145.5),
    ('QUANTIZE', 98, 130.5),
    ('NEW', 98, 115.5),
    ('PLAY', 132, 39.5),
    ('STOP', 151, 39.5),
    ('REC', 170, 39.5),
    ('LOOP', 189, 39.5),
    ('METRO', 208, 39.5),
    ('SCENE4_PAGE_R', 242, 115.5),
    ('SCENE3_PAGE_L', 242, 130.5),
    ('SCENE2_OCT-', 242, 145.5),
    ('SCENE1_OCT+', 242, 160.5),
)
KEY_REFS = tuple(ref for ref, _, _ in KEY_CHAIN)


@dataclass(frozen=True)
class KeyState:
    state: int = DIM
    palette: int = TEAL


def key_messages(index, state):
    from pad_leds import cc
    if (type(index) is not int or not 0 <= index < len(KEY_REFS) or
            type(state.state) is not int or not OFF <= state.state <= PULSE or
            type(state.palette) is not int or not TEAL <= state.palette <= WHITE):
        raise ValueError('LED27 key/state/palette outside protocol')
    return [cc(103, index), cc(104, state.state), cc(105, state.palette), cc(106, 1)]


def key_frame(console, clock, fresh):
    buttons = console.buttons
    scene = {'#5DCAA5': TEAL, '#3D8CFF': BLUE, '#E9AD50': AMBER}[buttons.sceneColor]
    frame = []
    for ref in KEY_REFS:
        if ref.startswith('SCENE'):
            state = KeyState(ON, scene)
        elif ref in ('DELETE', 'DUPLICATE', 'QUANTIZE', 'NEW', 'UNDO',
                     'BROWSE', 'NAV_R', 'NAV_L', 'MENU', 'BACK'):
            state = KeyState(ON, VIOLET)
        elif ref == 'SHIFT':
            state = KeyState(ON if buttons.shift else DIM, VIOLET)
        elif ref in ('BUILD', 'DROP', 'BREAK'):
            state = KeyState(BLINK if buttons.armed == ref else DIM)
        elif fresh and ref in ('PLAY', 'STOP', 'REC'):
            active = (clock.get('running', False) if ref == 'PLAY' else
                      not clock.get('running', False) if ref == 'STOP' else clock.get('armed', False))
            state = KeyState(ON if active else DIM)
        else:
            state = KeyState()
        frame.append(state)
    return tuple(frame)
