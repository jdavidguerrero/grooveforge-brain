#!/usr/bin/env python3
"""Real Unix-socket integration coverage against clocked offline audio."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools/synth-host"))
from ctl import Client


def check(value, message):
    if not value:
        raise AssertionError(message)


def main():
    binary = str(Path(sys.argv[1]).resolve())
    commands = set()
    with tempfile.TemporaryDirectory(prefix="gf-contract-", dir="/tmp") as directory:
        root = Path(directory)
        path = str(root / "host.sock")
        log = open(root / "host.log", "w+")
        process = None
        probe = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        inherited = False
        try:
            probe.bind(str(root / "probe.sock"))
        except PermissionError:
            inherited = True
        finally:
            probe.close()
        extra_clients = []
        (root / "imports").mkdir()
        clients = []
        (root / "presets").mkdir()
        # Own voices, never ROM data: construct Yamaha VMEM and VCED fixtures independently.
        voices=[]
        for index in range(32):
            voice=bytearray(128)
            for op in range(6):
                base=op*17
                voice[base:base+11]=bytes([99,75,65,90,99,85,75,0,39,0,0])
                voice[base+12]=7<<3  # centered detune, no rate scaling
                voice[base+14]=99 if op==5 else index*3
                voice[base+15]=2  # ratio mode, coarse 1
            voice[102:110]=bytes([99]*4+[50]*4)
            voice[110]=index
            voice[111]=8  # feedback 0, OSC SYNC 1
            voice[112]=35
            voice[116]=9  # LFO sync, sine
            voice[117]=24
            voice[118:128]=f"SELF{index:02d}    ".encode()
            voices.append(voice)
        payload=b"".join(voices)
        bank=bytes([0xf0,0x43,0,9,0x20,0])+payload+bytes([(-sum(payload))&127,0xf7])
        (root / "presets" / "self-bank.SYX").write_bytes(bank)
        broken=bytearray(bank);broken[-2]^=1
        (root / "imports" / "broken-bank.syx").write_bytes(broken)
        vced=bytearray()
        for op in range(6):
            vced.extend([99,75,65,90,99,85,75,0,39,0,0,0,0,0,0,0,99 if op==5 else 0,0,1,0,7])
        vced.extend([99]*4+[50]*4+[31,0,1,35,0,0,0,1,4,0,24])
        vced.extend(b"OWN SINGLE")
        single=bytes([0xf0,0x43,0,0,1,0x1b])+vced+bytes([(-sum(vced))&127,0xf7])
        (root / "imports" / "self-single.syx").write_bytes(single)

        def start():
            args = [binary, "--backend", "offline", "--socket", path,
                    "--presets", str(root / "presets"), "--repo-presets", str(root / "imports")]
            pairs = [socket.socketpair() for _ in range(3)] if inherited else []
            for _, child in pairs:
                args += ["--control-fd", str(child.fileno())]
            proc = subprocess.Popen(args, stdout=log, stderr=log,
                                    pass_fds=tuple(child.fileno() for _, child in pairs))
            for _, child in pairs:
                child.close()
            if inherited:
                attached = [Client.from_socket(parent) for parent, _ in pairs]
                clients.extend(attached)
                extra_clients[:] = attached[1:]
                attached[0].request("hello", client="ready")
                return proc, attached[0]
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                if proc.poll() is not None:
                    log.seek(0)
                    raise AssertionError(log.read())
                try:
                    c = Client(path)
                    clients.append(c)
                    return proc, c
                except (OSError, ConnectionError):
                    time.sleep(.01)
            raise AssertionError("host startup timeout")

        def request(c, cmd, **fields):
            commands.add(cmd)
            return c.request(cmd, **fields)

        def bad(c, command):
            c.sequence += 1
            command.setdefault("id", c.sequence)
            c.send(command)
            while True:
                r = c.receive()
                if "event" not in r:
                    check(r.get("ok") is False and isinstance(r.get("error"), str), str(r))
                    return r

        try:
            process, c = start()
            observer = extra_clients.pop(0) if inherited else Client(path)
            clients.append(observer)
            hello = request(c, "hello", client="contract-test")
            check(hello["sample_rate"] == 48000 and hello["block"] == 256, "hello default clock")
            check(set(hello["engines"]) == {"prophet", "dx7", "dx7_6op", "juno", "ob6", "model_d", "arp2600", "hybrid"}, "registry")
            state = request(c, "get_state")["parts"]
            check(len(state) == 4 and [p["role"] for p in state] == ["BASS", "CHORDS", "LEAD", "PAD"], "parts/roles")
            # Exercise every catalog, every value read/write, page filters and defaults.
            param_count = 0
            for engine in hello["engines"]:
                result = request(c, "set_engine", part=1, engine=engine)
                check(result["part"]["engine"] == engine and not result["part"]["modified"], "engine defaults")
                pages = request(c, "list_params", engine=engine)["pages"]
                all_values = request(c, "get_params", part=1)["values"]
                descriptors = []
                for page in pages:
                    page_values = request(c, "get_params", part=1, page=page["name"])["values"]
                    ids = []
                    for bank in page["banks"]:
                        check(1 <= len(bank) <= 8, "bank size")
                        for d in bank:
                            check(set(("id", "name", "unit", "min", "max", "default", "step", "kind")) <= set(d), "metadata keys")
                            check(d["kind"] in ("cont", "bipolar", "enum", "bool"), "metadata kind")
                            check(d["min"] <= d["default"] <= d["max"], "default range")
                            ids.append(d["id"])
                            descriptors.append(d)
                    check(set(ids) == set(page_values), "get page contents")
                check(len({d["id"] for d in descriptors}) == len(descriptors) == len(all_values), "catalog unique")
                for d in descriptors:
                    # Initial defaults must come from actual patch, not a second table.
                    check(abs(all_values[d["id"]] - d["default"]) < 1e-3, f"default mismatch {engine} {d}")
                    request(c, "set_param", part=1, param=d["id"], value=d["default"])
                param_count += len(descriptors)
                selected = descriptors[0]
                clipped = request(c, "set_param", part=1, param=selected["id"], value=1e20)
                check(clipped["value"] == selected["max"], "parameter clipping")
                actual = request(c, "get_params", part=1)["values"][selected["id"]]
                check(actual == selected["max"], "ACK/read consistency")
            # Core descriptors are complete and share the VST/firmware grouping.
            six_pages=request(c,"list_params",engine="dx7_6op")["pages"]
            core_pages=[p for p in six_pages if p["name"] in ["OPS",*[f"OP {i}" for i in range(1,7)],"ALGO/FB","PITCH EG","LFO"]]
            check(sum(len(b) for p in core_pages for b in p["banks"])==151,"151 core descriptors")
            check([len(b) for p in core_pages if p["name"]=="OP 6" for b in p["banks"]]==[7,8,6],"operator bank layout")
            request(c,"set_engine",part=1,engine="dx7_6op")
            for key,source,expected in [("op6.r1",-4,0),("op6.r1",104,99),("op6.coarse",2.6,3),("op6.on",.6,1),("algorithm",60,31),("lfo.wave",-1,0)]:
                check(request(c,"set_param",part=1,param=key,value=source)["value"]==expected,"DX7 raw clamp/round")
                check(request(c,"get_params",part=1)["values"][key]==expected,"DX7 clamped read")
            imports=request(c,"list_presets",engine="dx7_6op",tag="imported")["presets"]
            bank_presets=[p for p in imports if p["name"].startswith("dx7_6op:self-bank:")]
            check(len(bank_presets)==32 and len(imports)==33,"bank all-or-nothing import / VCED single")
            check(not any("broken-bank" in p["name"] for p in imports),"bad checksum published voices")
            for i in range(32):
                preset=next(p for p in bank_presets if p["name"].split(":")[2]==str(i+1))
                request(c,"load_preset",part=1,name=preset["name"])
                current=request(c,"get_params",part=1)["values"]
                check(current["algorithm"]==i and current["op6.level"]==i*3 and current["op1.level"]==99,"VMEM voice decode/order")
            single_preset=next(p for p in imports if "self-single" in p["name"])
            request(c,"load_preset",part=1,name=single_preset["name"])
            check(request(c,"get_params",part=1)["values"]["algorithm"]==31,"VCED single decode")
            request(c,"set_param",part=1,param="op6.coarse",value=8)
            request(c,"set_param",part=1,param="fx.0.mix",value=.63)
            request(c,"save_preset",part=1,name="Own six voice")
            six_saved=next(json.loads(p.read_text()) for p in (root / "presets").glob("user-*.json") if json.loads(p.read_text())["name"]=="Own six voice")
            six_bytes=bytes.fromhex(six_saved["patch"]["data"])
            check(six_bytes[:5]==b"GFP\x06\x07" and len(six_bytes)==162,"GFP6 save")
            check(any(p.read_bytes()==six_bytes for p in (root / "presets").glob("user-*.gfp")),"standalone GFP6 save")
            (root / "imports" / "six-fixture.gfp").write_bytes(six_bytes)
            request(c,"set_engine",part=1,engine="dx7")
            request(c,"load_preset",part=1,name="Own six voice")
            six_values=request(c,"get_params",part=1)["values"]
            check(six_values["op6.coarse"]==8 and abs(six_values["fx.0.mix"]-.63)<1e-6,"six GFP / bus roundtrip")
            # Stress the ACK/event ordering with pipelined engine and parameter edits.
            expected_ids = []
            for index in range(35):
                engine = hello["engines"][index % len(hello["engines"]) ]
                for cmd in (dict(cmd="set_engine", part=1, engine=engine),
                            dict(cmd="set_param", part=1, param="a.cutoff" if engine == "hybrid" else "cutoff", value=800),
                            dict(cmd="set_engine", part=1, engine=engine)):
                    c.sequence += 1
                    expected_ids.append(c.sequence)
                    c.send(dict(id=c.sequence, **cmd))
            for expected_id in expected_ids:
                response = c.receive()
                check(response.get("ok") and response.get("id") == expected_id, "pipelined mutation response")
            final_engine = hello["engines"][34 % len(hello["engines"])]
            key = "a.cutoff" if final_engine == "hybrid" else "cutoff"
            descriptor = next(p for page in request(c, "list_params", engine=final_engine)["pages"] for bank in page["banks"] for p in bank if p["id"] == key)
            check(request(c, "get_params", part=1)["values"][key] == descriptor["default"], "stale parameter crossed engine ACK")
            presets = request(c, "list_presets")["presets"]
            check(presets, "factory presets missing")
            for engine in hello["engines"]:
                filtered = request(c, "list_presets", engine=engine)["presets"]
                check(filtered and all(p["engine"] == engine for p in filtered), "preset engine filter")
            tagged = request(c, "list_presets", tag="factory")["presets"]
            check(tagged and all("factory" in p["tags"] for p in tagged), "preset tags")
            by_role = request(c, "list_presets", role="BASS")["presets"]
            check(by_role and all("BASS" in p["tags"] for p in by_role), "preset role filter")
            loaded = request(c, "load_preset", part=2, name=presets[0]["name"])
            check(loaded["part"]["preset"] == presets[0]["name"], "preset load")
            request(c, "set_engine", part=1, engine="model_d")
            request(c, "set_param", part=1, param="cutoff", value=1234)
            request(c, "save_preset", part=1, name="Test ñ patch")
            time.sleep(.04)
            check(not request(c, "get_state")["parts"][0]["modified"], "late parameter echo marked saved preset modified")
            # Import the real shared-codec GFP payload on restart, plus a corrupt file.
            saved = next(json.loads(p.read_text()) for p in (root / "presets").glob("user-*.json") if json.loads(p.read_text())["name"]=="Test ñ patch")
            (root / "imports" / "binary-fixture.gfp").write_bytes(bytes.fromhex(saved["patch"]["data"]))
            (root / "imports" / "broken.gfp").write_bytes(b"bad GFP")
            bad(c, dict(cmd="save_preset", part=1, name="Test ñ patch"))
            bad(c, dict(cmd="save_preset", part=1, name="../escape"))
            request(c, "set_param", part=1, param="cutoff", value=3000)
            request(c, "load_preset", part=1, name="Test ñ patch")
            check(request(c, "get_params", part=1)["values"]["cutoff"] == 1234, "user preset round-trip")
            level = request(c, "set_level", part=1, db=-120)["part"]
            check(level["level_db"] == -60, "level clamp")
            request(c, "set_level", part=1, db=-9)
            check(request(c, "set_mute", part=1, mute=True)["part"]["mute"], "mute")
            request(c, "set_mute", part=1, mute=False)
            # Separate subscriber proves broadcasts from another client and rates.
            request(observer, "subscribe", topics=["meters", "state", "cpu"])
            request(c, "set_param", part=1, param="cutoff", value=1800)
            for part in range(1, 5):
                request(c, "note", part=part, note=48+part, velocity=110)
            events = []
            deadline = time.monotonic() + 1.25
            while time.monotonic() < deadline:
                events.append(observer.receive())
            kinds = {e.get("event") for e in events}
            check({"meters", "state", "param", "cpu"} <= kinds, str(kinds))
            meters = [e for e in events if e.get("event") == "meters"]
            check(25 <= len(meters) <= 48, f"meter rate {len(meters)}")
            check(all(len(e["parts"]) == 4 for e in meters), "meter parts")
            check(any(e["mix"]["peak_db"] > -110 for e in meters), "sounding audio")
            cpu = [e for e in events if e.get("event") == "cpu"]
            check(all(e["mean_pct"] >= 0 and e["max_pct"] >= e["mean_pct"] and e["xruns"] >= 0 for e in cpu), "cpu values")
            for part in range(1, 5):
                request(c, "note", part=part, note=48+part, velocity=0)
            request(observer, "subscribe", topics=["param"])
            request(c, "set_param", part=1, param="cutoff", value=1900)
            seen = []
            deadline = time.monotonic() + .2
            while time.monotonic() < deadline:
                try:
                    seen.append(observer.receive(.05))
                except TimeoutError:
                    pass
            check(any(e.get("event") == "param" for e in seen), "explicit param subscription")
            request(observer, "subscribe", topics=[])
            # Bad commands/types/ranges; keep the same connection alive afterward.
            cases = [dict(cmd="bogus", part=1), dict(cmd="get_state", id="x"),
                     dict(cmd="hello"), dict(cmd="get_params", part=0), dict(cmd="get_params", part=5),
                     dict(cmd="get_params", part=True), dict(cmd="get_params", part=1.0),
                     dict(cmd="get_params", part=1, page="missing"),
                     dict(cmd="set_engine", part=1, engine="missing"),
                     dict(cmd="list_params", engine="missing"), dict(cmd="set_param", part=1, param="missing", value=2),
                     dict(cmd="set_param", part=1, param="cutoff", value="2"),
                     dict(cmd="set_mute", part=1, mute=1), dict(cmd="note", part=1, note=128, velocity=0),
                     dict(cmd="note", part=1, note=48, velocity=-1), dict(cmd="note", part=1, note=48, velocity=1.5),
                     dict(cmd="subscribe", topics=["missing"]), dict(cmd="load_preset", part=1, name="missing")]
            for case in cases:
                bad(c, case)
            c.socket.sendall(b'{broken}\n{"cmd":"get_state"}\n[]\n')
            for _ in range(3):
                check(c.receive()["ok"] is False, "malformed JSON response")
            # Fragmented UTF-8 line and pipelined requests preserve request IDs.
            raw = json.dumps(dict(id=989, cmd="hello", client="ñ"), ensure_ascii=False).encode() + b"\n"
            for byte in raw:
                c.socket.sendall(bytes([byte]))
            check(c.receive()["id"] == 989, "fragmented request")
            c.socket.sendall(b'{"id":990,"cmd":"get_state"}\n{"id":991,"cmd":"get_state"}\n')
            check([c.receive()["id"], c.receive()["id"]] == [990, 991], "pipeline order")
            # A second host cannot unlink the first host's live socket.
            if not inherited:
                duplicate = subprocess.run([binary, "--backend", "offline", "--socket", path,
                                            "--presets", str(root / "other"), "--no-state"], capture_output=True, timeout=5)
                check(duplicate.returncode != 0, "second host accepted occupied socket")
            request(c, "hello", client="still-live")
            # Overlong client is disconnected without affecting other clients.
            noisy = extra_clients.pop(0) if inherited else Client(path)
            noisy.socket.sendall(b"x" * 70000)
            time.sleep(.1)
            try:
                check(not noisy.socket.recv(1), "oversized line still connected")
            except ConnectionResetError:
                pass
            noisy.close()
            request(c, "get_state")
            # Crash recovery reads the checkpoint; notes are intentionally not replayed.
            request(c, "set_level", part=1, db=-7)
            request(c, "set_mute", part=4, mute=True)
            time.sleep(.25)
            expected = request(c, "get_state")["parts"]
            process.kill()
            process.wait(timeout=5)
            for item in clients:
                item.close()
            clients.clear()
            process, c = start()
            check(request(c, "get_state")["parts"] == expected, "restart part state")
            check(request(c, "get_params", part=1)["values"]["cutoff"] == 1900, "restart parameter state")
            check(any(p["name"] == "model_d:binary-fixture" for p in request(c, "list_presets")["presets"]), "GFP import")
            check(any(p["name"] == "Test ñ patch" for p in request(c, "list_presets")["presets"]), "restart user preset")
            request(c,"load_preset",part=2,name="dx7_6op:six-fixture")
            check(request(c,"get_params",part=2)["values"]["op6.coarse"]==8,"GFP6 import after restart")
            request(c,"load_preset",part=2,name="Own six voice")
            check(abs(request(c,"get_params",part=2)["values"]["fx.0.mix"]-.63)<1e-6,"persisted six FX")
            process.terminate()
            check(process.wait(timeout=5) == 0, "clean shutdown")
            # Corrupt checkpoint is diagnosed and factory state resumes safely.
            for item in clients:
                item.close()
            clients.clear()
            (root / "presets" / "last-state.json").write_text("{corrupt}")
            process, c = start()
            check(request(c, "get_state")["parts"][0]["level_db"] < -12, "corrupt state fallback")
            process.terminate()
            check(process.wait(timeout=5) == 0, "corrupt-state shutdown")
            if inherited:
                print("Sandbox blocks bind(): protocol tested with 3 inherited Unix socketpairs; pathname listener and duplicate-bind test require Pi/unsandboxed run")
            print(f"Contract: {len(commands)} commands; {param_count} parameter round-trips; errors, UTF-8, subscriptions, clients, crash recovery: PASS")
            print(f"Telemetry: {len(meters)} meters / 1.25 s; CPU windows={len(cpu)}; peak={max(e['mix']['peak_db'] for e in meters):.2f} dBFS")
        finally:
            for c in clients:
                c.close()
            if process and process.poll() is None:
                process.terminate()
                process.wait(timeout=5)
            log.close()


if __name__ == "__main__":
    main()
