#!/usr/bin/env python3
"""Collect cpu/xrun JSON events and vcgencmd probes, without installing software."""
import argparse
import json
import os
import platform
import shutil
import signal
import subprocess
import time
from ctl import Client


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--socket", required=True)
    p.add_argument("--seconds", required=True, type=float)
    p.add_argument("--output", required=True)
    args = p.parse_args()
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))
    client = Client(args.socket)
    hello = client.request("hello", client="pi-test-collector")
    client.request("subscribe", topics=["cpu"])
    result = dict(hello=hello, platform=platform.platform(), seconds=args.seconds,
                  events=[], probes=[], completed=False,
                  gui="Start GUI externally; this collector cannot certify its presence or FPS")
    begin = time.monotonic()
    next_probe = 0
    executable = shutil.which("vcgencmd")
    try:
        while time.monotonic() - begin < args.seconds:
            elapsed = time.monotonic() - begin
            if elapsed >= next_probe:
                probe = dict(elapsed=elapsed)
                for arg in ("measure_temp", "get_throttled"):
                    if executable:
                        try:
                            command = subprocess.run([executable, arg], text=True, capture_output=True, timeout=2)
                            probe[arg] = command.stdout.strip()
                            if command.returncode:
                                probe[arg + "_error"] = command.stderr.strip()
                        except (subprocess.TimeoutExpired, OSError) as error:
                            probe[arg + "_error"] = str(error)
                    else:
                        probe[arg + "_error"] = "vcgencmd unavailable"
                result["probes"].append(probe)
                next_probe += 10
            try:
                event = client.receive(min(1, max(.001, args.seconds - elapsed)))
                event["elapsed"] = time.monotonic() - begin
                result["events"].append(event)
            except TimeoutError:
                continue
        result["completed"] = True
    finally:
        client.close()
        events = [e for e in result["events"] if e.get("event") == "cpu"]
        if events:
            means = sorted(e["mean_pct"] for e in events)
            result["summary"] = dict(windows=len(events), mean_pct=sum(means) / len(means),
                p99_window_mean_pct=means[min(len(means)-1, int(.99 * len(means)))],
                max_pct=max(e["max_pct"] for e in events),
                max_window_p99_upper_pct=max(e.get("p99_upper_pct", e["max_pct"]) for e in events),
                xruns=events[-1]["xruns"], deadline_misses=events[-1].get("deadline_misses"),
                backend_xruns=events[-1].get("backend_xruns"),
                clock_discontinuities=events[-1].get("clock_discontinuities"),
                dropped_events=events[-1].get("dropped_events", 0))
        # p99 of 1s means is not p99 of blocks; max remains the block max.
        temp = args.output + ".tmp"
        with open(temp, "w") as f:
            json.dump(result, f, indent=2)
            f.flush()
            os.fsync(f.fileno())
        os.replace(temp, args.output)


if __name__ == "__main__":
    main()
