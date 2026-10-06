# Talisman reconstructed manual

This manual describes Ramius1701's preserved Talisman checkout at revision `ec7161ef66ad62a089ad6d555ca54074b56aaac3`, reviewed 6 October 2026. It is an original reconstruction from the implementation and shipped samples, not a recovered copy of the lost website. A subsequently supplied [GitLab Home excerpt](historical-gitlab-wiki.md) now provides partial historical documentation, reconciled against the implementation. The source is authoritative for behavior. The [extended-color chapter](extended-color-artwork.md) records development changes after this baseline.

Start with [building and running](building-and-running.md), then [configuration](configuration.md), [messages and files](messages-and-files.md), [menus and display](menus-and-display.md), and [Lua scripting](scripting.md). Development entry points are in [architecture and storage](development.md). Companion programs are covered in [networking and utilities](networking-and-utilities.md). Read [known discrepancies and validation](validation.md) before treating an example as production-tested.

## Evidence and maintenance

**Source verified** means the implementation was inspected; it does not mean the behavior was exercised. **Sample verified** means syntax was found in shipped assets. **Runtime verified** requires an actual recorded test against this checkout. The initial reconstruction used source and sample evidence. A later [Windows door verification](windows-door-verification.md) on 7 October 2026 built the preserved x86 session and tested native x86/x64 doors; its results and limits are recorded separately. A later [extended-color artwork change](extended-color-artwork.md) adds and runtime-tests color preservation in this development checkout. Other runtime acceptance areas remain unverified. Examples are development starting points, with platform-specific paths to adjust.

Generated references inventory [INI reads](reference/ini-readers.md), [TOML reader statements](reference/toml-readers.md), [menu/login dispatch](reference/commands.md), [display tokens](reference/display-tokens.md), and [all registered Lua wrappers](reference/lua-api.md). Their [source manifest](reference/source-manifest.json) records file hashes and revision. The extraction scope and limitations are stated in each reference. These indexes expose evidence and omissions rather than silently inventing defaults.

From the repository root, regenerate after implementation changes:

```powershell
python docs/manual/generate_reference.py . docs/manual/reference
python docs/manual/generate_reference.py . docs/manual/reference --check
python docs/manual/verify_documentation.py . docs/manual
```

Review prose alongside the changed source, regenerate the indexes, and record runtime results in [validation.md](validation.md). A successful `--check` establishes reference freshness, not correctness of every explanation or runtime behavior. Keep documentation with the source in Git so future development does not depend on an external website.

The separate BBS planning workspace contains a modified development copy with shared identity, permissions, rooms and other changes. Those features, its root CMake build, staging scripts and validation results are not part of this preserved checkout. Do not use them as evidence that this revision implements those features.

## Provenance

The preserved [README](../../README.md) credits Andrew Pamment and states that development passed to Lawrence Stockman in 2024; the GitLab project describes itself as a continuation fork. Allen's repository is the preservation/development source for this manual. Statements about why or when a maintainer stopped work are not established by the implementation. No documentation from Titan or Magicka has been assumed compatible. The repository [GPLv3 license](../../LICENSE) remains applicable to source excerpts.
