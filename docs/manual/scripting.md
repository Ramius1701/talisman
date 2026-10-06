# Lua scripting

Source: [Script.cpp](../../Talisman/Script.cpp), [Node.cpp](../../Talisman/Node.cpp) and shipped [scripts](../../Talisman/scripts). The [API reference](reference/lua-api.md) includes all 51 registered globals and their exact C wrappers, including return branches. It is the detailed reference for argument order and failure sentinels.

Each `Script::exec`, login, prelogin or message-header invocation creates a new Lua state, opens the standard libraries and registers BBS functions. Globals are not persistent between these invocations. Standard libraries are available; this is not a restricted plugin sandbox. Persistent application state must be explicitly stored.

## Entry points

| Entry point | Contract |
| --- | --- |
| Menu/login `runscript` | `data` basename gets `.lua` appended; executes the script body |
| `prelogin.lua` | Defines `prelogin()`; returns numeric `1` to proceed; other results disconnect |
| `login.lua` | Defines `login()`; returns username and password as two string-convertible values |
| `newuser.lua` | Executes script body after successful user creation in Node's registration flow |
| `predoor.lua` | Optional script body before external-door execution |
| Message-header script | A header `@RUNSCRIPT:basename@` calls `msgheader(file, mid, from, to, subject)` in that Lua file; `mid` is the message serial/UMSGID; see [MessageReader.cpp](../../Talisman/MessageReader.cpp) |

A prelogin return of Lua boolean `true` is not the documented numeric success contract: the implementation uses `lua_tonumber`. `login.lua` supplies credentials to the session flow; it does not by itself create a user or certify authentication. SSH supplies credentials through a separate transport flow, so validate any custom login script on both paths.

## Small script example

Save as `<Script Path>/hello.lua` and expose with menu command `runscript`, `data = "hello"`:

```lua
bbs_clear_screen()
bbs_write_string("Hello, " .. bbs_get_username() .. "\r\n")
bbs_write_string("Welcome to " .. bbs_get_bbs_name() .. "\r\n")
bbs_pause()
```

This example uses registered wrappers and has not been runtime-tested for this reconstruction.

## Frequently used contracts

| Function | Arguments | Returns / effect |
| --- | --- | --- |
| `bbs_write_string` | text | Prints via Node; no Lua return values |
| `bbs_read_string` | maximum length | One string |
| `bbs_read_password` | maximum length | One masked-input string |
| `bbs_getchar` | delay | Character string, or nil when delay reached; inspect Node::getch for delay units |
| `bbs_get_username` | none | Current username |
| `bbs_get_bbs_name` | none | Configured system name |
| `bbs_get_user_attribute` | attribute, default | Current user's attribute string |
| `bbs_get_user_attribute_by_name` | username, attribute, default | Named user's attribute string |
| `bbs_set_user_attribute` | attribute, value | Persists current user's attribute; no return values |
| `bbs_get_term_width` / `bbs_get_term_height` | none | Terminal dimensions |
| `bbs_set_time_left` | minutes | Sets persisted/session remaining time |
| `bbs_get_time_left` | none | Remaining minutes using integer division |
| `bbs_get_message` | base file, message ID | Five values: ID, to, from, subject, body; failure branches return ID 0 with sentinel text |
| `bbs_post_message` | base file, to, from, subject, body | Matches configured non-netmail areas and calls save_message; no return values, including no matching area |
| `bbs_get_calllog_x` | index | Nine values; exact order is in the wrapper |

Use strings for user attributes, including numeric values represented as strings. Several wrappers call `lua_tostring` and construct C++ strings without checking for nil; wrong arity/types can crash rather than raise a clean Lua error. A C wrapper's successful registration is not evidence of validated inputs or per-action authorization.

Errors are logged by Script using the script path and Lua error text. Check `logs/talisman.log` (or configured Log Path) when an action appears to do nothing. Lua version compatibility differs between inherited build paths, as explained in [building](building-and-running.md).
