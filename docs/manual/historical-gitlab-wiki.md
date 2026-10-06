# Historical GitLab wiki evidence

Allen supplied a transcription of the GitLab wiki Home page on 7 October 2026. Its attribution reports Lawrence Stockman as the last editor on 6 July 2024, and its scope states Talisman v0.42, Postie v0.12 and Binki v0.4. Source URL: https://gitlab.com/lawrencestockman/talisman/-/wikis/home. An attempted direct retrieval did not succeed; this record is based on the supplied excerpt, not an independently retrieved page or complete wiki archive.

The current checkout declares Talisman 0.54 with version suffix `dev` in [GenDefs.h](../../Talisman/GenDefs.h), Postie 0.16 in [GenDefs.h](../../Postie/GenDefs.h), and Binki 0.6 in [Server.h](../../Binki/Server.h). These source constants identify the checkout; they do not establish published release status. The historical Home page cannot serve as a complete current-version specification.

## Reconciliation with the current implementation

| Historical subject | Current-source evidence and qualification |
| --- | --- |
| INI/TOML configuration index | Broadly consistent with the existing manual. Conference/base filenames are configuration references, rather than a guarantee that every installation uses the sample names. |
| Multiple sessions from selected addresses | Servo reads `multiallow.ip` under Data Path and compares exact stored address strings when checking an existing connection. This is separate from passlist/blocklist admission. |
| ANSI/ASCII and terminal-size assets | Implemented. Current lookup selects the largest fitting width and height separately, then requires both maxima to match a candidate. It is not simply an exact-size lookup followed by an unconditional default. See [menus and display](menus-and-display.md). |
| SAUCE records ignored | Normal file display stops at the first SUB byte (`0x1a`). This skips a conventional trailing SAUCE section preceded by SUB; it is not a general SAUCE metadata parser. |
| Animated pause prompts | Implemented in `Node::pause`: a random file under GFile Path/pause, milliseconds on the first line and one frame per subsequent line. Frames use `print_f`, so pipe colors work. This feature predates the new extended-color change. |
| Custom strings | Implemented through Data Path/strings.dat, exact format-string keys, escape processing and paired msgid/msgstr lines. |
| G-files in replacement strings | Implemented via the lowercase `@gfile:basename@` tag in an allowed format string. |
| Windows installer updates | Historical instructions; no maintained current installer/update workflow was established by our source/build checks. |
| Linux update commands | Historical installation layout and component rebuild procedure. The tarball example refers to v0.30 despite the page's v0.42 heading. Use the current build chapter and actual installation paths. |
| Menu, Lua, Toolbelt, Gopher and quick-start headings | The supplied excerpt names these subjects but does not contain their linked pages. Their complete historical instructions remain unrecovered. |

Source review confirms these mechanisms; this documentation update adds no runtime tests for animated prompts or string replacement. It also does not establish full feature parity with another BBS package.
