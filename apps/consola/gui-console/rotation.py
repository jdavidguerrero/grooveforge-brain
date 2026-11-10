"""Configure rotation before QGuiApplication exists; Quick needs a scene transform."""
import os


def add_rotation_arguments(parser):
    parser.add_argument('--rotate', type=int, choices=(0, 90, 180, 270), default=0)
    parser.add_argument('--rotation-method', choices=('auto', 'qml', 'eglfs'), default='auto')


def configure_rotation(angle, method):
    if method == 'eglfs':
        os.environ['QT_QPA_EGLFS_ROTATION'] = str(-90 if angle == 270 else angle)
        return dict(angle=angle, requested_method=method, active_method='eglfs-env-unverified',
                    qml_angle=0, orientation_verified=False,
                    warning='QT_QPA_EGLFS_ROTATION does not rotate Qt Quick/OpenGL or touch')
    os.environ['QT_QPA_EGLFS_ROTATION'] = '0'
    return dict(angle=angle, requested_method=method, active_method='qml', qml_angle=angle,
                orientation_verified=angle == 0, warning='Physical orientation/touch requires Pi verification')
