# Waystone BBS

Waystone BBS is a continuation of Talisman BBS for Windows and Linux, maintained in Ramius1701's development fork. It carries forward Talisman's terminal BBS, message and file areas, doors, scripting and networking components, with reconstructed documentation and new extended-color ANSI support.

Talisman was originally written by Andrew Pamment (apam), drawing on his earlier Magicka BBS and Titan BBS projects. Development passed to Lawrence Stockman in 2024. Waystone preserves that lineage and the existing copyright notices. The project remains licensed under the GNU GPLv3; see [LICENSE](LICENSE).

The new name identifies this continuing project. Configuration remains `talisman.ini`, and existing executable names, source directories, message formats and door interfaces remain compatible. Network identifiers such as QWK ID are installation settings and are not automatically renamed. See [the naming and compatibility notes](docs/manual/waystone-name.md).

Inherited features (implementation presence does not mean every path has been runtime-verified):

Features:

    Squish Message bases with FTN support via HPT & Binkd
    Door32.sys, door.sys, chain.txt dropfile support for Doors
    Using INI & TOML configuration formats
    Works on Windows 10 and Linux
    IP Blocklist / Passlist support, with auto blocking
    Configurable security level time-outs & time limits
    File Conferences / Areas, External Protocols & Archivers
    SSH Support with automatic key generation
    QWK-E & Bluewave offline mail
    Private Email Support
    LUA Scripting
    Support for Terminal Sizes greater than 80×25
    Built in Full Screen Editor, with the option of an External (QuickBBS) Editor
    Native FTN, WWIVnet, and QWK networking support

## Documentation

The [reconstructed manual](docs/manual/README.md) covers the preserved Talisman baseline and subsequent Waystone changes using source evidence and shipped configurations, including development entry points, configuration, menus, Lua, storage and companion utilities. It distinguishes source evidence from runtime validation and includes reproducible reference inventories.
