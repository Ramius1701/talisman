# Networking, events and operator utilities

This chapter describes source interfaces. No live FTN/WWIV/QWK exchange or companion-service build was verified for the reconstruction. The [TOML evidence index](reference/toml-readers.md) includes the companion readers and defaults; use its links to inspect validation and branch behavior before changing a network configuration.

## FTN: Postie and Binki

[Postie](../../Postie/main.cpp) recognizes:

| Invocation | Responsibility |
| --- | --- |
| `postie scan` | Scan/export outgoing messages |
| `postie toss` | Process inbound messages through protected and ordinary inbound passes |
| `postie ticproc` | Process inbound TIC files |
| `postie tichatch "file" "areatag" "replaces" "desc"` | Hatch a file into a configured TIC area |

[Postie/Config.cpp](../../Postie/Config.cpp) reads `data/postie.toml`, including `[postie]`, `[[address]]`, `[[link]]`, `[[route]]`, `[[area]]`, `[[netarea]]` and file-area configuration. The shipped sample demonstrates `inbound`, `protinbound`, `outbound`, `packetdir`; address `aka`; link `aka`, `ouraka`, `archiver`; route `aka`, `pattern`; area `aka`, `file`, `tag`, `links`; and netarea `aka`, `file`. That sample is not a complete reference for every key. Consult the generated reader index for additional passwords, routing and file-processing fields.

[Binki](../../Binki/Binki.cpp) is BinkP transport, separate from tossing:

| Invocation | Behavior |
| --- | --- |
| `binki -S SOCKET` | Service an inherited socket supplied by Servo |
| `binki -P ADDRESS` | Poll one FTN address; optional `ADDRESS@domain` form |
| `binki -O` | Run all configured outgoing links |

[Binki/Config.cpp](../../Binki/Config.cpp) reads `data/binki.toml`:

| Table | Fields |
| --- | --- |
| `[binki]` | `semaphore`, `inbound`, `secure_inbound`, `outbound`, `default_zone` |
| `[[address]]` | `address`, `domain` |
| `[[link]]` | `domain`, `host`, `port`, `address`, `outbox`, `password`, `cram-md5` |

Link port defaults 24554. A local address needs a parsable address and nonempty domain to be retained. Check paths/passwords against the remote hub and align Binki's inbound/outbound queues with Postie. Neither enabling the listener nor setting an AKA establishes a working network.

## WWIV, QWK and bridges

[Falcon](../../Falcon/Falcon.cpp) recognizes `scan`, `toss`, `add NETNAME HOSTID SUBTYPE`, and `drop NETNAME HOSTID SUBTYPE`. `data/falcon.toml` has `[falcon]` settings and `[[network]]` / `[[area]]` records. Network fields include `outbox`, `name`, `emailbase`, `mynode`, `uplink`; area fields include `net`, `file`, `subtype`, `mynode`, `host`, `hidden`, `manual subscription`, `description`, `category`. Preserve the space in `manual subscription`. `heart_codes` has a loader defect noted in [validation](validation.md).

[Qwkie](../../Qwkie/main.cpp) recognizes `qwkie scan [network]`, `qwkie toss [network]`, and `qwkie poll [network]`; omission selects all configured networks. [qwkie.cpp](../../Qwkie/qwkie.cpp) reads `data/qwkie.toml`. `[[network]]` fields include `name`, `hostqwkid`, `ftpuser`, `ftphost`, `ftpport`, `archiver`, `password`, `tagline`, `curl_in`, `curl_out`; `[[area]]` fields include `network`, `basenumber`, `msgbase`. FTP/curl and archiver behavior require their own runtime checks. This utility is distinct from a caller downloading an offline packet inside Talisman.

[Bridge](../../Bridge/main.cpp) recognizes `bridge linkname` or `bridge ALL`. `data/bridge.toml` uses `[[link]]` fields `name`, `msgfile1`, `msgfile2`, `msgtype1`, `msgtype2`, `address1`, `address2`, `tagline1`, `tagline2`. Message types recognized by the loader are local, FTN, WWIV and QWK. Missing names/base files or required addresses cause records to be skipped. Inspect [bridge.cpp](../../Bridge/bridge.cpp) before using it on connected networks; test duplicate and loop behavior with disposable bases.

## Optional services

Gofer and NewsSrv receive open sockets from Servo; their argument is not a port to listen on independently. [Gofer](../../Gofer/Gofer.cpp) delegates configuration and request behavior to [Request.cpp](../../Gofer/Request.cpp). [NewsSrv](../../NewsSrv/NewsSrv.cpp) delegates to [Request.cpp](../../NewsSrv/Request.cpp). Document/test exposure and authentication from those implementations, rather than inferring them from a service label.

[HttpSrv](../../HttpSrv/HttpSrv.cpp) accepts `httpsrv PORT ROOT [-6]`; Servo launches it only with HTTP enabled and a nonempty root. The inherited HTTP server is not evidence of an administrative web interface, TLS, or WebSocket terminal support.

[Trinket](../../Trinket/Trinket.cpp) has `auth`, `push` and `pull` branches. `auth` reads username/password on standard input and prints `OK`/`FAIL`. `push` connects to MySQL but contains unfinished processing comments; `pull` is a stub. Do not describe this as a complete supported synchronization service. Its historical RethinkDB submodule reference and current source dependencies are inconsistent signals; inspect component build files before attempting this integration.

## Servo events

[EventMgr.cpp](../../Servo/EventMgr.cpp) reads `<Data Path>/events.toml`, using `[[event]]` fields `name`, `interval`, `start`, `watchfile`, `exec`. Choose exactly one of nonzero interval or nonempty watchfile, and provide `exec`. Use a positive interval in minutes; the loader rejects intervals over 10080 (one week), but does not robustly reject negative values.

`start` is parsed as `day,HH:MM` (three-letter day names, Sunday zero). The executor polls once per minute. Watch events react to file creation, deletion or modification-time change. `name = "SILENT"` suppresses its routine launch message. The initial scheduled-time calculation is inherited and needs runtime verification before promising exact weekly alignment.

POSIX events launch `/bin/sh -c`; Windows calls `CreateProcessA` directly. Shell operators/built-ins therefore need an explicit suitable shell on Windows. The event runner starts processes without waiting for completion; overlapping execution is possible. Commands inherit the installation working directory.

## Toolbelt command reference

These signatures are taken from the actual dispatcher and help text in [Toolbelt/main.cpp](../../Toolbelt/main.cpp). Most commands open `talisman.ini` first. Run maintenance against a backed-up development copy until the individual operation has been validated.

| Command | Arguments |
| --- | --- |
| password | `username newpassword` |
| seclevel | `username newlevel` |
| uploadindex | `indexfile folder database [uploadedby]` |
| uploadbulk | `folder database [uploadedby]` |
| filetrim | `database` |
| movefile | `srcfilename destdir srcdatabase destdatabase` |
| allfiles | `sec_level outfile` |
| newfiles | `sec_level yyyy.mm.dd outfile` |
| nodelistp | `domain nodelist database` |
| clearlrall | none |
| clearlruser | `username` |
| clearlrbase | `msgbasefile` |
| clearlrbaseuser | `msgbasefile username` |
| deleteuser | `username` |
| convertmsgna | `[-m|-p] srcfilename read_sl post_sl myaka uplink qwkid_start [prefix]` |
| convertfilena | `[-f|-p] srcfilename up_sl dl_sl vis_sl myaka uplink root create [prefix]` |
| adpost | `message.ini` |
| pack | `msgbasefile` |
| prune | `msgbasefile numbertoleave` |

`seclevel` changes a user attribute; also define that exact level in `seclevels.toml`. Last-read clearing changes caller read state. `pack`/`prune`, user deletion and file moves are mutating operations; read their implementations before use. `adpost` opens its own INI, which explains the extra `[main]` posting keys in the generated INI reference. Password arguments are visible in process/command history; a future operator-interface improvement should avoid that exposure.
