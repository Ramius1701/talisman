# Menus, login sequence and display

## Menu format

Source: [Menu.cpp](../../Talisman/Menu.cpp). A menu has a `[menu]` table and a `[[menuitem]]` array. This example uses implemented commands:

```toml
[menu]
description = "Development menu"
columns = 2

[[menuitem]]
description = "Messages"
hotkey = "M"
command = "submenu"
data = "message"
sec_level = 10

[[menuitem]]
description = "Hello script"
hotkey = "H"
command = "runscript"
data = "hello"
sec_level = 10

[[menuitem]]
description = "Return"
hotkey = "Q"
command = "prevmenu"
```

`submenu` appends `.toml` to `data` under the selected menu path; `runscript` appends `.lua` under Script Path. Do not include those suffixes. `prevmenu` returns from the current menu; `goodbye` propagates a logoff result through nested menus.

`[menu]` fields are `description` (default empty), `columns` (2), `gfile` (empty), and `prompt` (empty). A nonempty `gfile` replaces the generated menu display. An empty prompt produces the automatic hotkey prompt. Use positive `columns`; zero is not validated and breaks the layout loop. Header/prompt strings beginning `@@GFILE:` take the legacy GFile path; the implementation removes two final characters, so inspect the exact branch before designing this format.

Menu-item fields are `description`, `hotkey`, `command`, `data` (all default empty), and `sec_level` (0). Items below the user's level threshold are filtered when loaded. Hotkeys and command names are compared case-insensitively. This checkout has no `permission` menu field or action-level permission registry. A display restriction is not proof that lower-level APIs enforce equivalent restrictions.

## Command groups

The [complete dispatch inventory](reference/commands.md) records all 53 exact menu strings. Common groups are:

| Task | Command names |
| --- | --- |
| Navigate | `submenu`, `prevmenu`, `goodbye` |
| Message selection | `listconfs`, `listareas`, `nextmailconf`, `prevmailconf`, `nextmailarea`, `prevmailarea` |
| Messages | `listmsgs`, `postmsg`, `mailscan`, `msgreadnew`, `msgupdatelr`, `msgsearch`, `msgsubareas` |
| Private mail | `postemail`, `listemail`, `feedback` |
| File selection | `fileconfs`, `fileareas`, `nextfileconf`, `prevfileconf`, `nextfilearea`, `prevfilearea` |
| Files | `listfiles`, `download`, `upload`, `cleartagged`, `newfiles`, `filesearch` |
| Offline mail | `qwkdown`, `qwkup`, `bwavedown`, `bwaveup` |
| Customization | `runscript`, `rundoor`, `settings`, `editsig` |
| Information | `sysinfo`, `last10`, `listusers`, `bulletins`, `nlbrowse` |
| Other | `phlognew`, `phlogmanage`, `phlogrecent`, `indexreader`, `nodemsg` |
| Outbound sessions | `telnet_ip4`, `telnet_ip6`, `rlogin_ip4`, `rlogin_ip6` |

Outbound Telnet `data` uses comma-separated `HOST=host,PORT=23`; Rlogin adds `LUSER=`, `RUSER=` and `TERM=` with default port 513. Prefixes are uppercase and whitespace is not trimmed. The Rlogin IPv6 branch has different substring offsets from IPv4; see [discrepancies](validation.md). An example is not interoperability certification.

`rundoor` treats `data` as the executable path, supplies node number as the first argument and (on Windows) socket as the second, creates dropfiles, and runs the optional predoor script. It is not a general executable-plus-arguments parser. Dropfiles are `dorinfo1.def`, `chain.txt`, `door32.sys` and `door.sys` under `<Temp Path>/<node>`. POSIX execution uses a pseudoterminal; Windows launches a console process and relies on the selected door's socket/dropfile integration. The launcher waits for termination. See [Door.cpp](../../Talisman/Door.cpp).

## Login items

`[[loginitem]]` fields are `command`, `data`, `clear_screen`, `pause_after`, `sec_level`; absent data is empty, booleans false, level zero. Empty commands are not added. [Node.cpp](../../Talisman/Node.cpp) processes items in order after login, clearing the screen first and pausing afterward when requested.

Its 11 recognized commands are `QUICKLOGIN`, `SELECTTHEME`, `SENDGFILE`, `BULLETINS`, `EMAILCHECK`, `MAILSCAN`, `NEWFILES`, `LAST10`, `RUNSCRIPT`, `RUNDOOR` and `MSGREADNEW`. `QUICKLOGIN` can stop the rest of the login sequence. `SENDGFILE` uses `data` as an asset name; `RUNSCRIPT` uses a Lua basename; `RUNDOOR` uses an executable path. Menu and login dispatchers are distinct.

## Assets and terminal rendering

Use the shipped [gfiles](../../Talisman/gfiles) and [menus](../../Talisman/menus) as revision-matched examples. `Node::send_gfile`, `send_file` and `print_f` implement asset lookup, terminal-dependent rendering, pipe colors and display macros. Their exact tokens and branches are in [Node.cpp](../../Talisman/Node.cpp); do not copy a Mystic/Synchronet macro list into this manual.

Verified macro branches include `MAILCONF`, `MAILAREA`, `FILEAREA` and `PHLOGURL`, enclosed with `@...@`. The [complete token index](reference/display-tokens.md) distinguishes Node macros from message-header macros. Some values are formatted to the token's field width. A `RUNSCRIPT:` branch can invoke Lua from display text when the caller allows scripts. This means an asset can have executable behavior; inspect display assets as well as menu entries when debugging a session.

`send_gfile` selects assets by basename before the first dot. Filename dot-tokens may include dimensions (`80x25`) and `ans`/`asc` (also uppercase). Candidates must fit the terminal; the code computes the largest width and height separately and retains candidates matching both. It randomly selects among equally sized final candidates. ANSI-capable terminals try ANSI assets first, then fall back to non-ANSI assets. Thus `welcome.80x25.ans` and `welcome.asc` are different rendering candidates for the same `welcome` request. Avoid dimension combinations that leave no candidate matching both selected maxima.

The user settings screen supports codepage (`auto`, `utf-8`, `cp437`), theme, full-screen editor/reader choices, signature toggle, node-message preference and screen-size overrides. ANSI capability influences the available full-screen paths. Font/Sixel support depends on the terminal and corresponding code paths, not solely the server having an asset file.

See [extended-color artwork](extended-color-artwork.md) for the new 256-color/RGB editor and renderer support, color literals, terminal limitations and showcase installation.
