# Building and running

## Build entry points

There is no root `CMakeLists.txt` at the reviewed revision. Each component has its own CMake project; `Talisman.sln` is the Windows solution. Source evidence: [Talisman CMake](../../Talisman/CMakeLists.txt), [Servo CMake](../../Servo/CMakeLists.txt), [Visual Studio project](../../Talisman/Talisman.vcxproj), and [.gitmodules](../../.gitmodules).

The Talisman CMake project requests C++17, SQLite3, OpenSSL, Lua 5.3, libssh and Iconv. POSIX link branches add platform libraries such as `util`, `pthread`, and in some cases `stdc++fs`. It uses the project's `cmake/` find modules. These files describe inherited build requirements, not a currently validated toolchain lock.

With dependencies and a suitable POSIX compiler installed, the source's build layout supports commands such as:

```sh
cmake -S Talisman -B build/talisman -DCMAKE_BUILD_TYPE=Release
cmake --build build/talisman
cmake -S Servo -B build/servo -DCMAKE_BUILD_TYPE=Release
cmake --build build/servo
```

These commands are reconstructed from the project layout and were not executed for this manual. Build companion components separately when enabling their services. Do not infer that all programs share Talisman's dependencies; inspect each component's `CMakeLists.txt`.

On Windows, inspect and adjust solution dependency paths before building. The Win32 Talisman project references `C:\Program Files (x86)\OpenSSL-Win32` and bundled libraries under `Talisman/win32_deps`, including `lua5.4.2.lib`, `sqlite3.lib` and `ssh.lib`. CMake requests Lua 5.3 whereas this project links Lua 5.4.2. Win32 and x64 configurations exist; their existence is not evidence that both are complete or usable. No fresh Windows build was verified here.

`Trinket/librethinkdbxx` is a historical submodule. Do not initialize it just to build the BBS or Servo; determine whether the Trinket integration is needed first.

## Runtime layout

Use a separate development installation directory. Stage the built `servo` and `talisman` executables in it, with `talisman.ini` and copies of the shipped `Talisman/data`, `menus`, `gfiles` and `scripts` directories. Create writable message, temporary, log and download directories matching the INI and area configurations. Treat the shipped INI as an example: it contains the original author's Gopher path and enabled optional ports.

Relative INI paths, area paths and child-program paths depend on the process working directory. Launch Servo from the installation directory. POSIX Servo explicitly launches `./talisman`, `./gofer`, `./newssrv`, `./binki`, and `./httpsrv`; enabling one of those services requires its executable and configuration. Windows uses the corresponding `.exe` names. See [Servo](../../Servo/Servo.cpp).

Servo listens for incoming connections, assigns a node, and starts a Talisman session. `talisman -N NUMBER -S SOCKET -T` and `talisman -N NUMBER -S SOCKET -SSH` are child-process forms; `SOCKET` is an inherited open socket, not a TCP port. Supply values after `-N` and `-S`; the argument parser does not guard missing values. See [session entry point](../../Talisman/main.cpp).

For a first isolated development run, disable SSH, Gopher, NNTP, BinkP and HTTP with their port settings set to `-1`, use a nonprivileged Telnet port, and ensure the configured new-user security level has an exact entry in `seclevels.toml`. Then launch `servo` and connect a terminal to the configured Telnet port. This is a proposed smoke-test procedure, not a result recorded by this manual.

**Binding behavior:** this revision binds listeners to all interfaces (`INADDR_ANY` / `in6addr_any`). It has no `loopback only` or `headless` INI option. A private VM or suitable host firewall is required to make the proposed run isolated. There is no equivalent of the newer workspace's staging fixture in this checkout.
