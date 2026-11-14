# USB audio overlay (custom community core)

Source: https://github.com/alex6679/teensy-4-usbAudio
Revision: f731821 (2026-05-20), copied offline from the supplied local clone.
Based on Teensyduino 1.59; evaluated on installed framework 1.160.0 / Teensy 5.1.0.
Only `consola_usb` uses these nine core files. See `apps/docs/consola/10-usb-audio-teensy.md`.

License: upstream LICENSE is MIT (included). Individual core files preserve PJRC's
permission notice, including its additional target-device selection condition.
Retain both notices. No GUI or Arduino IDE configuration is used.

Local change: return after draining input queues when the output interface is not
running. Upstream continues with a negative buffer index if channels are nonzero.
The identical branch exists in the grrrr candidate. This guard avoids indexing
`txBuffer[-1]` on a stopped interface.

All other overlay files are unchanged. `ASYNC_TX_ENDPOINT` is enabled upstream;
the README's description of sample skipping refers to its disabled fallback.
