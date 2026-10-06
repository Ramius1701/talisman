# Message and file areas

## Message conferences

The shipped local configuration illustrates the two-level structure. `data/msgconfs.toml` contains:

```toml
[[messageconf]]
name = "Local"
config = "mb_local"
sec_level = 10
```

This loads `data/mb_local.toml`. Each `[[messagearea]]` contains:

```toml
[[messagearea]]
name = "General Discussion"
file = "general"
read_sec_level = 10
write_sec_level = 10
qwk_base_no = 100
```

With the default Message Path, `file` selects the Squish base prefix `msgs/general`. Use a base name, not a `.sqd` filename. The configuration and message-base implementation are in [MsgConf.cpp](../../Talisman/MsgConf.cpp), [MsgArea.cpp](../../Talisman/MsgArea.cpp) and [Squish.cpp](../../Common/Squish.cpp).

| Area field | Absent-key fallback | Meaning in loader |
| --- | --- | --- |
| name | `Unknown Name` | Display name |
| file | empty | Empty means area is not added |
| read_sec_level / write_sec_level | `10` / `10` | Minimum read/post levels |
| delete_sec_level / delete_own_sec_level | `-1` / `-1` | Passed to area deletion checks; inspect together with level capabilities |
| aka | empty | FTN origin address string |
| wwivnode | `0` | WWIV origin node value |
| netmail | `false` | Area netmail flag |
| subbed_by_default | `false` | Default subscription behavior |
| real_names | `false` | Posting-name choice |
| qwk_base_no | `-1` | Offline-mail area identifier |

Conference `tagline` defaults empty and is passed into its message areas. Conference/area selections and subscriptions are stored with the user. Reordering or removing areas requires checking saved selections and last-read/subscription identifiers; these are not a schema migration system.

Message deletion first accepts a positive area delete threshold met by the user, then a positive own-message threshold met by the author. Otherwise it consults the exact security-level entry's `can_delete_msgs` / `can_delete_own_msgs`. Consequently the area's default `-1` does not by itself prohibit deletion: level capabilities can still allow it. See `MsgArea::delete_message` in [MsgArea.cpp](../../Talisman/MsgArea.cpp).

Local private email uses `data/email.sqlite3`; it is separate from a Squish netmail area and should not be described as Internet SMTP mail. See [Email.cpp](../../Talisman/Email.cpp). Talisman contains QWK and Blue Wave offline readers/exporters; Qwkie is a separate QWK network utility. See [Qwk.h](../../Talisman/Qwk.h) and [bluewave.h](../../Talisman/bluewave.h).

For FTN, align the BBS area's `file` with Postie's corresponding base and `aka` with your actual assigned address. The sample `21:1/126.2` is historical sample data, not an address allocated to your installation. Routing, packet passwords and archive detection are companion settings, covered in [networking](networking-and-utilities.md).

## File conferences

`data/fileconfs.toml` points to a file-area configuration basename:

```toml
[[fileconf]]
name = "General Files"
config = "fb_general"
sec_level = 10
```

`data/fb_general.toml` can define:

```toml
[[filearea]]
name = "Uploads"
database = "fb_uploads"
file_path = "dloads/general/uploads"
upload_sec_level = 10
download_sec_level = 10
visible_sec_level = 10
delete_sec_level = -1
```

The file index is `data/fb_uploads.sqlite3`; payload files live under `file_path`. The loader skips an area with an empty database or path. Upload/download levels default 10, visibility defaults to the download level, and delete level defaults -1. Source: [FileConf.cpp](../../Talisman/FileConf.cpp) and [FileArea.cpp](../../Talisman/FileArea.cpp).

File listing/tagging operates on index entries; copying payloads into a directory alone does not establish that the file index contains them. Toolbelt provides `uploadbulk` and `uploadindex` for indexing. Back up both indexes and payload directories together.

## External protocols and archivers

`[[protocol]]` fields: `name`, `upload_command`, `download_command`, `ssh_upload_command`, `ssh_download_command`, `batch`, `prompt`. Missing SSH commands fall back to the corresponding ordinary commands; missing `batch` is false and missing `prompt` is true. See [Config.cpp](../../Talisman/Config.cpp) and [Protocol.cpp](../../Talisman/Protocol.cpp).

Protocol command strings are split on whitespace. The first token is the executable. Exact argument tokens `@SOCKET@`, `@UPPATH@` and `@FILENAME@` are expanded for uploads; downloads expand `@SOCKET@` and either batch `@FILELIST@` or single `@FILENAME@`. `@FILELIST@` becomes multiple arguments. These are not general embedded-string substitutions or a shell quoting parser. The inherited command tokenizer cannot express an executable path containing spaces by ordinary quoting. Check platform-specific supplied examples under `Talisman/win32_deps` before adapting Linux `rz`/`sz` commands.

`[[archiver]]` fields for BBS use are `name`, `extension`, `unarc`, `arc`. Extraction expands `@ARCHIVE@`, `@OUTDIR@` and `@FILELIST@`; compression expands `@ARCHIVE@` and `@FILELIST@`. Whole-archive extraction drops the `@FILELIST@` token. See [Archiver.cpp](../../Talisman/Archiver.cpp). Postie reads extra signature/offset settings from the same file for archive identification; an archiver adequate for QWK is not automatically adequate for FTN tossing.

Neither process launch nor updated transfer counters prove successful byte delivery. Runtime validation must compare payload bytes and exercise cancellation, failure and both Telnet/SSH transport paths.
