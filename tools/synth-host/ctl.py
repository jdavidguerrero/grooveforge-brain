#!/usr/bin/env python3
"""Unix JSON-lines client. Commands from arguments or stdin; events on stdout."""
import argparse
import json
import os
import socket
import sys
import time


def default_socket():
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    return f"{runtime}/gf-synth.sock" if runtime else f"/tmp/gf-synth-{os.getuid()}.sock"


class Client:
    def __init__(self, path=None, timeout=2):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.socket.settimeout(timeout)
        self.socket.connect(path or default_socket())
        self.buffer = b""
        self.sequence = 0

    @classmethod
    def from_socket(cls, sock):
        client = cls.__new__(cls)
        client.socket, client.buffer, client.sequence = sock, b"", 0
        return client

    def send(self, command):
        self.socket.sendall((json.dumps(command, ensure_ascii=False) + "\n").encode())

    def receive(self, timeout=2):
        deadline = time.monotonic() + timeout
        while b"\n" not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("socket response timeout")
            self.socket.settimeout(remaining)
            chunk = self.socket.recv(65536)
            if not chunk:
                raise ConnectionError("host closed socket")
            self.buffer += chunk
        line, self.buffer = self.buffer.split(b"\n", 1)
        return json.loads(line)

    def request(self, cmd, **fields):
        self.sequence += 1
        self.send(dict(id=self.sequence, cmd=cmd, **fields))
        while True:
            response = self.receive()
            if response.get("id") == self.sequence:
                if not response.get("ok"):
                    raise RuntimeError(response.get("error", "host error"))
                return response

    def close(self):
        self.socket.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("commands", nargs="*", help="JSON objects (id auto-added if absent)")
    parser.add_argument("--socket", default=default_socket())
    parser.add_argument("--subscribe", help="comma separated: meters,state,param,cpu")
    parser.add_argument("--seconds", type=float, default=0, help="event collection duration; 0 = until Ctrl-C")
    args = parser.parse_args()
    client = Client(args.socket)
    try:
        lines = args.commands
        if not lines and not args.subscribe:
            lines = (line for line in sys.stdin if line.strip())
        for index, line in enumerate(lines, 1):
            command = json.loads(line)
            command.setdefault("id", index)
            client.send(command)
            while True:
                response = client.receive()
                print(json.dumps(response, ensure_ascii=False), flush=True)
                if response.get("id") == command["id"]:
                    break
        if args.subscribe:
            client.send(dict(id=2147483647, cmd="subscribe", topics=args.subscribe.split(",")))
            end = time.monotonic() + args.seconds if args.seconds else float("inf")
            while time.monotonic() < end:
                try:
                    item = client.receive(min(2, end - time.monotonic()))
                except (TimeoutError, socket.timeout):
                    continue
                print(json.dumps(item, ensure_ascii=False), flush=True)
    finally:
        client.close()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
