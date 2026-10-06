# Extended-color ANSI artwork

Added to the development checkout on 7 October 2026 after the preserved baseline `ec7161ef66ad62a089ad6d555ca54074b56aaac3`. This is new functionality, not a claim about the lost original documentation or full Mystic compatibility.

## Supported colors

ANSI files displayed by `send_gfile` retain their escape sequences. The message ANSI converter and ANSI editor now also preserve foreground/background `38;5;n` / `48;5;n` indexed colors and `38;2;r;g;b` / `48;2;r;g;b` RGB colors. Indices and channels range from 0 through 255. Standard `30–37`, `40–47`, bright `90–97`, `100–107`, default `39`/`49`, reset `0`, bold `1` and normal intensity `22` are handled. The converter retains colored trailing spaces and the final populated line without requiring a trailing newline.

These are semicolon-separated SGR sequences. The editor/converter do not support colon-separated RGB notation, every SGR attribute or every terminal control sequence. Blink/iCE interpretation and CP437 glyph appearance remain terminal dependent; this change does not add RIP or Sixel capabilities. The normal file-display path passes raw ANSI through, so its capabilities differ from the editor/converter.

ANSI detection identifies ANSI capability, not 256-color or truecolor capability. There is no automatic color-depth negotiation or color quantization in this change. Provide a classic variant for legacy callers and select extended artwork explicitly. Asset lookup does not distinguish color depth by itself; do not put all three color variants under the same basename. Existing pipe codes `|00` through `|23` keep their original mapping.

## Showcase installation

Three original CP437 demo assets are included under [gfiles](../../Talisman/gfiles), with separate basenames `color-demo-16`, `color-demo-256` and `color-demo-truecolor`. Each has an ASCII fallback. The [showcase script](../../Talisman/scripts/color-demo.lua) lets a caller choose the color depth. Rebuild Talisman to use the new editor, converter and color tokens. Copy the assets to the active GFile Path and the script to Script Path when updating an existing installation. The [sample submenu](../../Talisman/menus/color-demo.toml) is optional; existing menus and login items are not changed.

Add this entry to your active menu to launch the showcase:

```toml
[[menuitem]]
description = "ANSI color artwork"
hotkey = "C"
command = "RUNSCRIPT"
data = "color-demo"
```

Choose an unused hotkey. For a permanent splash, use the desired asset basename in an existing `SENDGFILE` login item or a menu `gfile` field. Keep artwork within the caller's terminal dimensions; filename sizing is independent of color depth.

## Script/menu colors and prompt configuration

`Node::print_f` (including Lua `bbs_write_string` and generated menu prompts) recognizes `|FG:196|`, `|BG:21|`, `|FG:#RRGGBB|` and `|BG:#RRGGBB|`. The closing pipe is required; hexadecimal digits may use either case. Palette numbers use the terminal's 256-color palette, independent of Talisman's classic DOS pipe mapping. Valid tokens are removed for callers without ANSI; malformed tokens remain literal. They change only the chosen channel. Raw `.ans` files should contain actual ANSI escape sequences, since ordinary file bytes do not pass through `print_f`.

```lua
bbs_write_string("|FG:196|Red|BG:21| on blue|FG:#40C0FF| RGB foreground|07")
```

The INI `[Main]` `Input Foreground` and `Input Background` settings accept a palette index or `#RRGGBB` in addition to existing color names:

```ini
[Main]
Input Foreground = #40C0FF
Input Background = 17
```

The ANSI editor adds **Ctrl-Z → C → F/B**. Enter `0–255` or `#RRGGBB`; blank or invalid input leaves the current color unchanged. Existing F/B palette selection remains available. Loading, drawing and saving retain extended colors; switching to a classic foreground replaces it as expected.

## Verification

Windows x86 Release builds of the changed session and native regression probe passed with MSVC 19.51 and the previously documented local dependency libraries. The probe passed 30 checks. A real session on an inherited loopback socket verified classic/indexed/RGB pipe output, configured prompt colors, 256/RGB asset transmission, interactive RGB foreground and indexed background selection, typed character output and the saved ANSI file. It also exercised the shipped showcase script for all three depths. A separate non-ANSI session verified token stripping and ASCII asset selection. The tests bypass Servo and capture bytes; a human visual check in the intended BBS terminal is still needed. No native x64 BBS claim is made by these tests.

Reproduce with [tests/colors](../../tests/colors/README.md). Recorded [session results](verification/colors/session-results.json), [probe results](verification/colors/probe-results.txt) , [plain-terminal results](verification/colors/session-plain-results.json), [tested source hashes](verification/colors/tested-source.json) and [build log](verification/colors/build.log) describe the actual validation. Xterm documents the relevant sequences in its [control sequence reference](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html); Microsoft documents terminal color processing in [console virtual terminal sequences](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences).
