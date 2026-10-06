# Configuration

## INI interpretation

Talisman and most companion tools open `talisman.ini` in the working directory. [INIReader](../../Common/INIReader.h) normalizes section/key names for case-insensitive lookup; spaces and singular/plural spelling still matter. TOML keys must use their exact names. Unknown settings are not an extension mechanism: a setting has no effect unless a reader consumes it.

The [generated INI inventory](reference/ini-readers.md) lists 119 literal reader calls across the components, including their source locations and fallback expressions. Some belong to secondary files, such as Toolbelt's posting INI, so consult the reader context before adding a key to `talisman.ini`.

## Core paths

Source: [Config::load](../../Talisman/Config.cpp).

| `[Paths]` key | Fallback | Purpose |
| --- | --- | --- |
| GFile Path | `gfiles` | Display assets; selected theme can override it |
| Data Path | `data` | TOML configuration and SQLite data |
| Menu Path | `menus` | Menu TOML; selected theme can override it |
| Message Path | `msgs` | Prefix for Squish message-base names |
| Temp Path | `temp` | Session/door working assets |
| Script Path | `scripts` | Lua scripts; singular `Script` |
| Log Path | `logs` | Logs; session writes `talisman.log` |
| Netmail Semaphore | `netmail.sem` | Netmail notification path |
| Echomail Semaphore | `echomail.sem` | Echomail notification path |
| External Editor | empty | External-editor setting; inspect [Editor.cpp](../../Talisman/Editor.cpp) for invocation |

Other services consume `Gopher Root` (Gofer; fallback `gopher`) and `HTTP Root` (Servo; fallback empty). Paths are strings, not an automatically installed directory tree.

## Core identity and session settings

| `[Main]` key | Parser fallback |
| --- | --- |
| Root Menu | `main` (loads `<menu path>/main.toml`) |
| Qwk ID | `TALISMAN` |
| Location | `Somewhere, The World` |
| Sysop Name | `Sysop` |
| System Name | `Talisman` |
| Max Nodes | `4` |
| New User Sec Level | `10` |
| New User Feedback | `false` |
| New User Password | empty |
| Hostname | `localhost` |
| Main AKA | `0:0/0` |
| Windows Local Echo | `true` |
| Input Background | `red` |
| Input Foreground | `bright white` |

Names/location are converted to CP437 in the config loader. The prompt color mapper supports black, red, green, brown, blue, magenta, cyan and white; the foreground also supports their `bright ` variants. See [Config.cpp](../../Talisman/Config.cpp).

## Listener settings read by Servo

| `[Main]` key | Fallback | Behavior |
| --- | --- | --- |
| Telnet Port | `2323` | Listener is created unconditionally; `-1` is not a documented Telnet-disable switch |
| SSH Port | `-1` | `-1` disables |
| Gopher Port | `-1` | `-1` disables; Gofer independently defaults its advertised port to 7070 |
| NNTP Port | `-1` | `-1` disables |
| BinkP Port | `-1` | `-1` disables |
| HTTP Port | `-1` | Requires enabled port and nonempty `[Paths] HTTP Root` |
| Enable IPv6 | `false` | Additional IPv6 listener paths |
| IP Block Timeout | `300` | Attempt-counting window in seconds |
| IP Block Attempts | `5` | Threshold used by IPBlockItem |

Source: [Servo.cpp](../../Servo/Servo.cpp) and [IPBlockItem.cpp](../../Servo/IPBlockItem.cpp). `IP Block Timeout` is not a timed unblock: an auto-block is appended to `blocklist.ip` and remains blocked. Connections are counted by Servo's admission path, not solely failed password logins. `passlist.ip`, `blocklist.ip` and `multiallow.ip` are read from Data Path; matching uses stored address strings, not a documented CIDR parser.

The shipped sample enables SSH at 2222 and Gopher at 7070. Those are sample values, not defaults. It also contains `scripts path`, which does not match `Script Path`; its intended default directory still works only because the fallback is `scripts`.

## Loaded TOML files

| File under Data Path | Array of tables | Notes |
| --- | --- | --- |
| msgconfs.toml | `[[messageconf]]` | `name`, `config`, `sec_level`, `tagline`; referenced config basename gets `.toml` appended |
| seclevels.toml | `[[seclevel]]` | Exact level definitions, time and legacy capabilities |
| loginitems.toml | `[[loginitem]]` | Login actions in file order |
| protocols.toml | `[[protocol]]` | External transfer commands |
| archivers.toml | `[[archiver]]` | Compression/extraction commands |
| fileconfs.toml | `[[fileconf]]` | `name`, `config`, `sec_level`; config basename gets `.toml` appended |
| fonts.toml | `[[font]]` | `slot`, `filename`; parse failure is logged, not fatal in Config::load |
| themes.toml | `[[theme]]` | Read only if file exists; adds to built-in default theme |
| bulletins.toml | `[[bulletin]]` | Loaded separately by [Bulletins.cpp](../../Talisman/Bulletins.cpp) |
| events.toml | `[[event]]` | Servo's separate event loader |

Most core arrays are dereferenced without null/type checks. A syntactically valid TOML file can therefore still crash if an expected array is missing or a key has the wrong type. Do not interpret fallback values as comprehensive input validation. See [TOML evidence](reference/toml-readers.md) and [known discrepancies](validation.md).

## Security levels and themes

`[[seclevel]]` fields are `name`, `sec_level`, `mins_per_day`, `timeout_mins`, `bulk_msg_allowed`, `can_delete_msgs`, `can_delete_own_msgs`, and `invisible`. Missing numeric time values default to zero and missing capabilities default to false; a zero `sec_level` entry is discarded. Time settings use minutes. The loader's level lookup is **exact**, while many access checks compare the user level to a required minimum. Define every level you assign; an undefined new-user level is dereferenced during registration. Zero time values should not be presented as a verified unlimited-time convention. See [Config.cpp](../../Talisman/Config.cpp) and [Node.cpp](../../Talisman/Node.cpp).

Theme entries use `name`, `gfile_path`, `menu_path` and `req_ansi`. Path defaults are the INI paths; `req_ansi` defaults false. Default Theme is inserted first (index zero). The user stores a numeric `theme` index; reordering themes can change the meaning of saved preferences. Theme/codepage settings in the user settings screen are applied next login. See [Settings.cpp](../../Talisman/Settings.cpp).
