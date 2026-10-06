# Windows 32-bit and 64-bit door verification

Tested 7 October 2026 against preserved application revision `ec7161ef66ad62a089ad6d555ca54074b56aaac3`.

**Result: the 32-bit Talisman session successfully ran both a 32-bit and a 64-bit native Door32-compatible door, exchanged data in both directions, and returned to the menu. A complete native x64 Talisman BBS build remains unverified.**

## Measured results

| Check | Result |
| --- | --- |
| Session binary PE machine | `0x014c` (x86) |
| 32-bit fixture PE machine / pointer width | `0x014c` / 32 bits |
| 64-bit fixture PE machine / pointer width | `0x8664` (AMD64) / 64 bits |
| Session -> 32-bit door | Eleven-line Door32 file, node and socket checked; two-way challenge/reply passed |
| Session -> 64-bit door | Eleven-line Door32 file, node and socket checked; two-way challenge/reply passed |
| Return to session | Both launches returned to `VERIFY>` and permitted the next action |
| Logout | Session log records graceful logoff; inherited application exit status was `4294967295` (its disconnect path returns -1) |
| Unmodified Servo built x64 through test wrapper | Build passed; not exercised as the runtime supervisor |
| Shipped Talisman x64 Release project | Failed: requested `v143` toolset is unavailable on the test machine |
| Same project with command-line `v145` override | Failed on CRT/POSIX deprecation errors including `fopen`, `tell`, `lseek`; no successful native x64 session binary established |

Raw runtime results and build logs are under [verification/windows-doors](verification/windows-doors). The reproducible fixture sources are under [tests/windows-doors](../../tests/windows-doors/README.md).

## Method and provenance

The test wrapper compiled the preserved session files directly from the checkout, without changing application code. It supplied the installed dependency headers/libraries, C++17, exception handling (`/EHsc`), `_CRT_SECURE_NO_WARNINGS` and Windows system libraries. These are test build settings; success does not mean the inherited solution builds unchanged.

The recorded compiler was MSVC 19.51.36256 through Visual Studio 18 2026; CMake selected Windows SDK 10.0.26100.0. Available x86 dependencies were OpenSSL 3.6.4, libssh 0.12.0, Lua 5.5.1 and SQLite 3.53.4. This differs from the original Lua 5.3/5.4.2 build references; the result is scoped to the tested build environment and does not certify all Lua scripts or historical bundled binaries.

Python created a loopback TCP connection, made its accepted socket inheritable, and launched the x86 Talisman session with `-N 1 -S <socket> -T`. A synthetic account logged in and selected each door through the actual `RUNDOOR` menu dispatcher. The dispatcher executed the preserved `Door::createDropfiles` and Windows `Door::runExternal` implementation. Each fixture read `temp/1/door32.sys`, checked communication type 2, node/socket identity and eleven lines, reported its actual pointer width, received a challenge and sent the expected reply. The client then verified return to the menu. The fixture socket value observed was 368.

This method bypasses Servo's connection acceptance. It exercises a Telnet session, not SSH, and uses synthetic native doors rather than third-party games or DOS emulation. Socket values with unusual high-order bits and other Winsock providers were not exercised. The inherited `int` socket storage therefore remains a portability concern; this successful observed handoff does not prove every x64 session path safe.

## What can be claimed

The preserved source's Windows door launcher can run a native x64 executable from the tested x86 BBS session and supply a usable inherited Door32 socket. The door must implement the expected interface; running any arbitrary console executable is not sufficient.

The presence of x64 configurations in the solution does not establish a supported native x64 BBS. Complete x64 dependency configuration, consistent C++ build settings, a socket-width review, and actual x64 session/SSH/transfer tests remain necessary before making that claim.

Microsoft documents cross-architecture process creation and inherited handles in [Windows interprocess communication](https://learn.microsoft.com/en-us/windows/win32/winprog64/interprocess-communication) and [CreateProcessA](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessa). Those explain why this handoff is possible; the runtime fixture supplies the evidence specific to Talisman.
