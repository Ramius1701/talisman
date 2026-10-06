# Windows door verification fixture

This fixture builds the **unmodified** preserved Talisman session sources through a separate CMake wrapper. It tests synthetic 32-bit and 64-bit Windows Door32-compatible executables over an inherited TCP socket bound only to loopback. It creates synthetic accounts and a new disposable installation; it does not use a live BBS, launch Servo's listeners or alter the application sources.

Requirements: a Windows x64 machine, Visual Studio C++ desktop tools, CMake, Python 3, and a compatible x86 dependency installation containing headers, import libraries and DLLs for OpenSSL, libssh, Lua and SQLite. `setup.py` accepts their paths explicitly. The compiler environment must allow MSBuild's file tracker to run. In the Codex restricted process environment this required an approved compiler execution outside that restriction.

From this directory:

```powershell
python setup.py S:/Github/talisman C:/path/to/vcpkg_installed/x86-windows
python build.py x86 talisman door-probe
python build.py x64 door-probe servo
python run.py runtime-01
python check_original_x64.py
python check_original_x64.py --retarget
```

`runtime-01` must not already exist; use a new name for a rerun. `run.py` writes `results.json` and retains the transcript, session log, dropfiles and door reports in the runtime directory. It verifies PE machine types, fixture pointer width, the eleven-line Door32 file, matching inherited socket/node, both directions of communication and return to the menu. The fixture door explicitly changes its socket to blocking mode and imposes a receive timeout. Its initial console is hidden immediately.

The original-project checker records failure or success in build logs. It writes compiler products to a directory here, disables automatic vcpkg integration, and optionally overrides the project toolset to the installed `v145` for the recorded test machine. It does not edit the original project. Failure is a reported finding, not a passing x64 application certification.

The synthetic door is an integration probe, not a representative test of every third-party door's transport, terminal handling, timing or dependencies. The loopback harness supplies the socket directly to Talisman and bypasses Servo; SSH is not exercised. Cross-architecture launch success does not certify a native x64 Talisman build. See the associated manual report for the exact tested revision, dependencies and results.
