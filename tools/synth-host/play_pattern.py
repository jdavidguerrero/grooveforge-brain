#!/usr/bin/env python3
"""Four-part 120 BPM test pattern over the contract's note command."""
import argparse
import signal
import time
from ctl import Client, default_socket

ROOTS = (36, 48, 60, 48)
VOICES = (1, 6, 4, 6)
OFFSETS = (0, 3, 7, 10, 12, 15)


def run(path, seconds, configure=False):
    client = Client(path)
    held = []
    try:
        if configure:
            for part, engine in enumerate(("model_d", "juno", "ob6", "hybrid"), 1):
                client.request("set_engine", part=part, engine=engine)
                client.request("set_level", part=part, db=-12.0411998266)
                client.request("set_mute", part=part, mute=False)
        start = time.monotonic()
        cycle = 0
        while time.monotonic() - start < seconds:
            shift = (0, 2, -2, 5)[cycle % 4]
            for part, (root, voices) in enumerate(zip(ROOTS, VOICES), 1):
                for voice in range(voices):
                    note = root + OFFSETS[voice] + shift
                    client.request("note", part=part, note=note, velocity=round((.78 - .035 * voice) * 127))
                    held.append((part, note))
            release_at = min(start + cycle * 2 + 1.5, start + seconds)
            time.sleep(max(0, release_at - time.monotonic()))
            for part, note in held:
                client.request("note", part=part, note=note, velocity=0)
            held.clear()
            cycle += 1
            time.sleep(max(0, min(start + cycle * 2, start + seconds) - time.monotonic()))
    finally:
        for part, note in held:
            try:
                client.request("note", part=part, note=note, velocity=0)
            except (OSError, RuntimeError):
                break
        client.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--socket", default=default_socket())
    parser.add_argument("--minutes", type=float, default=60)
    parser.add_argument("--configure", action="store_true", help="reset engines/levels; host --benchmark supplies wet FX")
    args = parser.parse_args()
    if args.minutes <= 0:
        parser.error("--minutes must be positive")
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))
    run(args.socket, args.minutes * 60, args.configure)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
