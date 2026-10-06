# ANSI color regression tests

These build the current session sources and a probe with MSVC, C++17 and matching x86 dependency libraries. They do not use the incomplete original x64 project configuration. Supply a dependency prefix containing `include`, `lib` and `bin` for OpenSSL, Lua, SQLite and libssh. Existing source compatibility with the dependency versions still applies.

```powershell
python tests/colors/setup.py C:/path/to/x86-windows
python tests/colors/build.py
python tests/colors/run_probe.py
python tests/colors/session.py runtime-01
python tests/colors/session.py runtime-plain-01 --plain
```

Run each session with a fresh destination. The tests create a synthetic account and isolated runtime, inherit a loopback socket directly into Talisman and bypass Servo. They do not start public listeners or change installed BBS data. The probe checks parser boundaries, classic/bright/default colors, extended colors in message conversion, colored spaces, final lines and editor save/load. The session checks real socket bytes, pipe codes, prompt colors, ANSI asset output and interactive extended color selection, drawing and saving.

The capture proves emitted sequences and file preservation, not a particular caller terminal's visual rendering. Normal disconnect currently returns `4294967295`; the session checks its interactions before recording that code. Checked-in evidence is in [the manual](../../docs/manual/extended-color-artwork.md).
