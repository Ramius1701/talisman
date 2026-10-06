# Validation ledger and known discrepancies

Reviewed 6 October 2026 against clean preserved revision `ec7161ef66ad62a089ad6d555ca54074b56aaac3`. This ledger separates observed source behavior from runtime validation. It records baseline documentation findings. Later application changes and their evidence are tracked separately; see [extended-color artwork](extended-color-artwork.md).

## Completed reconstruction checks

Source review covered build entry points, runtime dispatch, core config/area loaders, storage, external process paths, Lua registration/wrappers and companion command/config interfaces. Generated evidence inventories literal INI reads, TOML read statements, menu/login dispatch and Lua wrappers. A source-hash manifest permits detecting drift at the file level; `--check` compares all generated output without modifying it.

Documentation links, referenced source paths, listed command names and sample TOML syntax are checked during reconstruction. Detailed results are in [verification-results.json](verification-results.json). These checks do not substitute for compiling or executing the programs.

## Discrepancies affecting instructions

| Finding | Evidence | Documentation consequence |
| --- | --- | --- |
| Sample uses `scripts path`; loader reads `Script Path` | [sample INI](../../Talisman/talisman.ini), [Config.cpp](../../Talisman/Config.cpp) | Use singular spelling; custom directory otherwise falls back to `scripts` |
| CMake requests Lua 5.3; Windows project links Lua 5.4.2 | [CMake](../../Talisman/CMakeLists.txt), [vcxproj](../../Talisman/Talisman.vcxproj) | Separate dependency/build paths and test scripts on the selected version |
| Core TOML arrays and typed values often dereferenced unchecked | [Config.cpp](../../Talisman/Config.cpp), [Menu.cpp](../../Talisman/Menu.cpp), [MsgConf.cpp](../../Talisman/MsgConf.cpp), [FileConf.cpp](../../Talisman/FileConf.cpp) | Valid TOML syntax does not imply safe/complete configuration |
| Font loader checks `slot` before dereferencing `filename` | [Config.cpp](../../Talisman/Config.cpp) | Include both fields; slot with missing filename can dereference null |
| New-user security level lookup is exact and dereferenced | [Node.cpp](../../Talisman/Node.cpp), [Config.cpp](../../Talisman/Config.cpp) | Define every assigned level, especially registration's level |
| Listeners bind all interfaces; Telnet listener unconditional | [Servo.cpp](../../Servo/Servo.cpp) | Do not document a nonexistent loopback setting or Telnet disable switch |
| IP timeout controls attempt window; blocks persist | [IPBlockItem.cpp](../../Servo/IPBlockItem.cpp) | Do not promise automatic timed unblocking or call all attempts failed logins |
| Menu columns not range checked | [Menu.cpp](../../Talisman/Menu.cpp) | Use positive values; zero is not a usable layout |
| Outbound Rlogin IPv6 removes one extra character from option values | [Menu.cpp](../../Talisman/Menu.cpp) | Do not certify IPv4 example syntax as working IPv6 behavior |
| Falcon `heart_codes` branch reads `_inbound` rather than `_hearts` | [Config.cpp](../../Falcon/Config.cpp) | Do not promise requested conversion mode takes effect |
| Transfer/door command tokenizer splits whitespace | [Protocol.cpp](../../Talisman/Protocol.cpp), [Menu.cpp](../../Talisman/Menu.cpp) | Do not advertise full shell-style command quoting/substitution |
| Windows door failure prints an error but returns true | [Door.cpp](../../Talisman/Door.cpp) | Boolean return alone does not establish launch success |
| Trinket push/pull unfinished | [Trinket.cpp](../../Trinket/Trinket.cpp) | Describe integration status accurately |

These are source findings, not exploit tests or a full defect audit. Make fixes and regression fixtures as separate code changes so compatibility implications can be reviewed.

## Runtime work still required

| Area | Meaningful acceptance evidence |
| --- | --- |
| Build | Clean Windows and POSIX builds with recorded dependency/compiler versions |
| Registration/login | New user, existing user, incorrect password, configured password gate, undefined-level rejection after fixing loader |
| Menus/permissions | Low/high level navigation, nested return/logoff, unknown commands, malformed/missing TOML structures |
| Messages/email | Post/reply/read/delete, last-read/subscription behavior, private-mail recipient isolation, Squish reopen |
| Files/transfers | Index/payload consistency; actual upload/download byte comparison on Telnet and SSH; failure/cancellation |
| Doors/editors | Dropfile contents and transport behavior with a synthetic door; exit/disconnect and path handling |
| Lua | Every documented contract, wrong types/arity after validation fixes, hooks, error logging, chosen Lua version |
| Networking | Disposable hub fixtures for packet route/password/duplicate handling, TIC, WWIV, QWK and bridge loops |
| Optional listeners | Request/authentication behavior, IPv4/IPv6 and startup failure handling |
| Events | First-run alignment, watchfile create/delete/modify, overlapping command behavior |
| Recovery | Restore backed-up SQLite/Squish/payload/queue data into a fresh installation and compare contents |

Record each result with source revision, platform, command/fixture, expected and actual behavior, and remaining limits. Results from a modified checkout must identify its changes; they cannot silently be attributed to this preserved baseline.

## Windows door verification, 7 October 2026

The [runtime report](windows-door-verification.md) records successful native 32-bit and 64-bit Door32 fixture execution from the preserved x86 session, including two-way socket traffic and return to menu. The original x64 Talisman project failed its recorded build attempts; native x64 BBS support remains unverified. This narrows the outstanding door checks to other integrations, failures, SSH and third-party doors.
