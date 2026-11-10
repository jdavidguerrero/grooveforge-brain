"""Console-only PJRC Wire overlay. Never changes the installed framework.

Teensyduino 1.160.0 WireIMXRT has no transaction timeout setter. Stream's
setTimeout does not affect I2C. Keep the driver, add a microsecond deadline
shared by bus acquisition and transfer, plus a cumulative timeout counter.
"""
import hashlib
from pathlib import Path
import shutil

Import("env")
platform = env.PioPlatform()
original_get_package_dir = platform.get_package_dir
base = Path(original_get_package_dir("framework-arduinoteensy"))
source = base / "libraries/Wire"
private = Path(env.subst("$BUILD_DIR")) / "framework-consola-wire"
key = hashlib.sha256((source / "WireIMXRT.h").read_bytes() +
                     (source / "WireIMXRT.cpp").read_bytes() +
                     next(p for p in (Path(env.subst("$PROJECT_DIR")) / "../consola/surface/tools/wire_timeout.py",
                                         Path(env.subst("$PROJECT_DIR")) / "../surface/tools/wire_timeout.py") if p.exists()).read_bytes() +
                     (base / "package.json").read_bytes() +
                     ((base / ".gf-overlay-sha256").read_bytes() if (base / ".gf-overlay-sha256").exists() else b"")).hexdigest()
stamp = private / ".gf-wire-sha256"
if not stamp.exists() or stamp.read_text() != key:
    if private.exists():
        shutil.rmtree(private)
    shutil.copytree(base, private)
    header = private / "libraries/Wire/WireIMXRT.h"
    cpp = private / "libraries/Wire/WireIMXRT.cpp"
    h, c = header.read_text(), cpp.read_text()
    def replace_once(text, before, after):
        if text.count(before) != 1:
            raise RuntimeError(f"Wire 1.160.0 changed; audit timeout patch: {before!r}")
        return text.replace(before, after)
    h = replace_once(h, "\tvoid setClock(uint32_t frequency);", """\tvoid setClock(uint32_t frequency);
    void setWireTimeout(uint32_t us) { timeout_us_ = us ? us : 250; }
    uint32_t getWireTimeoutCount() const { return timeout_count_; }
""")
    h = replace_once(h, "\tbool wait_idle();", """\tbool wait_idle(uint32_t started);
    uint32_t timeout_us_ = GF_WIRE_TIMEOUT_US;
    uint32_t timeout_count_ = 0;
""")
    c = replace_once(c, "bool TwoWire::wait_idle()", "bool TwoWire::wait_idle(uint32_t started)")
    if c.count("\telapsedMillis timeout = 0;") != 3:
        raise RuntimeError("Wire timeout sites changed")
    c = c.replace("\telapsedMillis timeout = 0;", "", 1)
    c = replace_once(c, "if (timeout > 16)", "if (uint32_t(micros() - started) >= timeout_us_)")
    c = replace_once(c, "if (force_clock()) break;", "++timeout_count_;\n            force_clock(); // At most nine 10 us pulses; retry on a later loop.")
    # Both transfer APIs share one deadline with wait_idle, not two timeouts.
    if c.count("if (!wait_idle()) return 4;") != 2:
        raise RuntimeError("Wire wait_idle call sites changed")
    c = c.replace("if (!wait_idle()) return 4;", "const uint32_t started = micros();\n\tif (!wait_idle(started)) return 0;")
    # endTransmission error must be nonzero, requestFrom returns zero bytes.
    c = replace_once(c, "if (!wait_idle(started)) return 0;\n\tuint32_t tx_index", "if (!wait_idle(started)) return 4;\n\tuint32_t tx_index")
    if c.count("\telapsedMillis timeout = 0;") != 2 or c.count("timeout > 50") != 2:
        raise RuntimeError("Wire transfer timeout sites changed")
    c = c.replace("\telapsedMillis timeout = 0;", "")
    c = c.replace("timeout > 50", "uint32_t(micros() - started) >= timeout_us_")
    c = c.replace("port->MTDR = LPI2C_MTDR_CMD_STOP; // try to send a stop", "port->MTDR = LPI2C_MTDR_CMD_STOP; // try to send a stop\n            ++timeout_count_;")
    c = replace_once(c, "#define CLOCK_STRETCH_TIMEOUT 15000", "#define CLOCK_STRETCH_TIMEOUT GF_WIRE_TIMEOUT_US")
    # NACK is not a timeout; requestFrom's combined branch needs a distinction.
    c = replace_once(c, "++timeout_count_;\n\t\t\tbreak;", "if (!(status & LPI2C_MSR_NDF)) ++timeout_count_;\n\t\t\tbreak;")
    header.write_text(h)
    cpp.write_text(c)
    stamp.write_text(key)

def private_package_dir(name, *args, **kwargs):
    if name == "framework-arduinoteensy":
        return str(private)
    return original_get_package_dir(name, *args, **kwargs)

platform.get_package_dir = private_package_dir
print("console Wire: private PJRC driver + bounded microsecond transaction timeout")
