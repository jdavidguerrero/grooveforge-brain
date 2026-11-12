#!/usr/bin/env python3
"""Wait for a live host ACK and enforce D4 before each GUI service start."""
import os
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / 'tools/synth-host'))
from ctl import Client


def stop_for_gui(client):
    client.request('hello', client='gf-console-start')
    client.request('transport', action='stop')
    client.request('preview', stop=True)
    state = client.request('get_drum_state')['clock']
    if state['running'] or state['armed'] or state['queued'] != -1:
        raise RuntimeError('D4: host did not acknowledge stopped/disarmed/queue empty')


def main():
    deadline = time.monotonic() + 60
    error = None
    while time.monotonic() < deadline:
        client = None
        try:
            client = Client(os.path.join(os.environ['XDG_RUNTIME_DIR'], 'gf-synth.sock'))
            stop_for_gui(client)
            print('Host ready; D4 STOP acknowledged', flush=True)
            return
        except (OSError, RuntimeError, ValueError, ConnectionError) as exc:
            error = exc
        finally:
            if client is not None:
                client.close()
        time.sleep(1)
    raise RuntimeError(f'Host not ready after 60 s: {error}')


if __name__ == '__main__':
    main()
