# INI reader inventory

Generated from the checkout by `generate_reference.py`. Every matched literal section/key read is listed, including repeated readers with different defaults. Defaults are C++ expressions, not sample configuration values. Reader names help distinguish talisman.ini from other INI files; inspect the linked function for the filename. Dynamic section reads are outside this inventory.

| Section | Key | Reader type | Fallback expression | Reader variable | Source |
| --- | --- | --- | --- | --- | --- |
| Paths | Data Path | Get | `"data"` | inir | [Binki/Server.cpp:1303](../../../Binki/Server.cpp#L1303) |
| Paths | Log Path | Get | `"logs"` | inir | [Binki/Server.cpp:1304](../../../Binki/Server.cpp#L1304) |
| Paths | Temp Path | Get | `"temp"` | inir | [Binki/Server.cpp:1305](../../../Binki/Server.cpp#L1305) |
| Main | Sysop Name | Get | `"Sysop"` | inir | [Binki/Server.cpp:1306](../../../Binki/Server.cpp#L1306) |
| Main | System Name | Get | `"A Talisman BBS"` | inir | [Binki/Server.cpp:1307](../../../Binki/Server.cpp#L1307) |
| Main | Location | Get | `"Somewhere, The World"` | inir | [Binki/Server.cpp:1308](../../../Binki/Server.cpp#L1308) |
| Paths | Data Path | Get | `"data"` | inir | [Bridge/main.cpp:29](../../../Bridge/main.cpp#L29) |
| Paths | Message Path | Get | `"msgs"` | inir | [Bridge/main.cpp:30](../../../Bridge/main.cpp#L30) |
| Paths | Log Path | Get | `"logs"` | inir | [Bridge/main.cpp:31](../../../Bridge/main.cpp#L31) |
| Paths | Data Path | Get | `"data"` | inir | [Falcon/Scanner.cpp:101](../../../Falcon/Scanner.cpp#L101) |
| Paths | Message Path | Get | `"msgs"` | inir | [Falcon/Scanner.cpp:102](../../../Falcon/Scanner.cpp#L102) |
| Paths | Log Path | Get | `"logs"` | inir | [Falcon/Scanner.cpp:103](../../../Falcon/Scanner.cpp#L103) |
| Paths | Temp Path | Get | `"temp"` | inir | [Falcon/Scanner.cpp:104](../../../Falcon/Scanner.cpp#L104) |
| Paths | Data Path | Get | `"data"` | inir | [Falcon/SubReq.cpp:53](../../../Falcon/SubReq.cpp#L53) |
| Paths | Message Path | Get | `"msgs"` | inir | [Falcon/SubReq.cpp:54](../../../Falcon/SubReq.cpp#L54) |
| Paths | Log Path | Get | `"logs"` | inir | [Falcon/SubReq.cpp:55](../../../Falcon/SubReq.cpp#L55) |
| Paths | Temp Path | Get | `"temp"` | inir | [Falcon/SubReq.cpp:56](../../../Falcon/SubReq.cpp#L56) |
| Paths | Data Path | Get | `"data"` | inir | [Falcon/SubReq.cpp:116](../../../Falcon/SubReq.cpp#L116) |
| Paths | Message Path | Get | `"msgs"` | inir | [Falcon/SubReq.cpp:117](../../../Falcon/SubReq.cpp#L117) |
| Paths | Log Path | Get | `"logs"` | inir | [Falcon/SubReq.cpp:118](../../../Falcon/SubReq.cpp#L118) |
| Paths | Temp Path | Get | `"temp"` | inir | [Falcon/SubReq.cpp:119](../../../Falcon/SubReq.cpp#L119) |
| Paths | Data Path | Get | `"data"` | inir | [Falcon/Tosser.cpp:571](../../../Falcon/Tosser.cpp#L571) |
| Paths | Message Path | Get | `"msgs"` | inir | [Falcon/Tosser.cpp:572](../../../Falcon/Tosser.cpp#L572) |
| Paths | Log Path | Get | `"logs"` | inir | [Falcon/Tosser.cpp:573](../../../Falcon/Tosser.cpp#L573) |
| Paths | Temp Path | Get | `"temp"` | inir | [Falcon/Tosser.cpp:574](../../../Falcon/Tosser.cpp#L574) |
| main | hostname | Get | `"localhost"` | inir | [Gofer/Request.cpp:377](../../../Gofer/Request.cpp#L377) |
| main | gopher port | GetInteger | `7070` | inir | [Gofer/Request.cpp:378](../../../Gofer/Request.cpp#L378) |
| paths | data path | Get | `"data"` | inir | [Gofer/Request.cpp:379](../../../Gofer/Request.cpp#L379) |
| paths | log path | Get | `"logs"` | inir | [Gofer/Request.cpp:380](../../../Gofer/Request.cpp#L380) |
| paths | gopher root | Get | `"gopher"` | inir | [Gofer/Request.cpp:406](../../../Gofer/Request.cpp#L406) |
| Paths | Data Path | Get | `"data"` | inir | [NewsSrv/Request.cpp:132](../../../NewsSrv/Request.cpp#L132) |
| Paths | Message Path | Get | `"msgs"` | inir | [NewsSrv/Request.cpp:133](../../../NewsSrv/Request.cpp#L133) |
| Paths | Log Path | Get | `"logs"` | inir | [NewsSrv/Request.cpp:134](../../../NewsSrv/Request.cpp#L134) |
| main | hostname | Get | `"localhost"` | inir | [NewsSrv/Request.cpp:135](../../../NewsSrv/Request.cpp#L135) |
| Paths | Data Path | Get | `"data"` | inir | [Postie/main.cpp:24](../../../Postie/main.cpp#L24) |
| Paths | Data Path | Get | `"data"` | inir | [Postie/Scanner.cpp:763](../../../Postie/Scanner.cpp#L763) |
| Paths | Message Path | Get | `"msgs"` | inir | [Postie/Scanner.cpp:764](../../../Postie/Scanner.cpp#L764) |
| Paths | Log Path | Get | `"logs"` | inir | [Postie/Scanner.cpp:765](../../../Postie/Scanner.cpp#L765) |
| Paths | Temp Path | Get | `"temp"` | inir | [Postie/Scanner.cpp:766](../../../Postie/Scanner.cpp#L766) |
| Paths | Data Path | Get | `"data"` | inir | [Postie/TicProc.cpp:161](../../../Postie/TicProc.cpp#L161) |
| Paths | Log Path | Get | `"logs"` | inir | [Postie/TicProc.cpp:162](../../../Postie/TicProc.cpp#L162) |
| Paths | Temp Path | Get | `"temp"` | inir | [Postie/TicProc.cpp:163](../../../Postie/TicProc.cpp#L163) |
| Paths | Data Path | Get | `"data"` | inir | [Postie/TicProc.cpp:408](../../../Postie/TicProc.cpp#L408) |
| Paths | Log Path | Get | `"logs"` | inir | [Postie/TicProc.cpp:409](../../../Postie/TicProc.cpp#L409) |
| Paths | Temp Path | Get | `"temp"` | inir | [Postie/TicProc.cpp:410](../../../Postie/TicProc.cpp#L410) |
| Paths | Data Path | Get | `"data"` | inir | [Postie/Tosser.cpp:877](../../../Postie/Tosser.cpp#L877) |
| Paths | Message Path | Get | `"msgs"` | inir | [Postie/Tosser.cpp:878](../../../Postie/Tosser.cpp#L878) |
| Paths | Log Path | Get | `"logs"` | inir | [Postie/Tosser.cpp:879](../../../Postie/Tosser.cpp#L879) |
| Paths | Temp Path | Get | `"temp"` | inir | [Postie/Tosser.cpp:880](../../../Postie/Tosser.cpp#L880) |
| Paths | Data Path | Get | `"data"` | inir | [Qwkie/main.cpp:24](../../../Qwkie/main.cpp#L24) |
| Paths | Temp Path | Get | `"temp"` | inir | [Qwkie/main.cpp:25](../../../Qwkie/main.cpp#L25) |
| Paths | Message Path | Get | `"msgs"` | inir | [Qwkie/main.cpp:26](../../../Qwkie/main.cpp#L26) |
| Paths | Log Path | Get | `"logs"` | inir | [Qwkie/main.cpp:27](../../../Qwkie/main.cpp#L27) |
| main | telnet port | GetInteger | `2323` | inir | [Servo/Servo.cpp:166](../../../Servo/Servo.cpp#L166) |
| main | ssh port | GetInteger | `-1` | inir | [Servo/Servo.cpp:167](../../../Servo/Servo.cpp#L167) |
| main | max nodes | GetInteger | `4` | inir | [Servo/Servo.cpp:168](../../../Servo/Servo.cpp#L168) |
| main | gopher port | GetInteger | `-1` | inir | [Servo/Servo.cpp:169](../../../Servo/Servo.cpp#L169) |
| main | nntp port | GetInteger | `-1` | inir | [Servo/Servo.cpp:170](../../../Servo/Servo.cpp#L170) |
| main | binkp port | GetInteger | `-1` | inir | [Servo/Servo.cpp:171](../../../Servo/Servo.cpp#L171) |
| main | http port | GetInteger | `-1` | inir | [Servo/Servo.cpp:172](../../../Servo/Servo.cpp#L172) |
| paths | http root | Get | `""` | inir | [Servo/Servo.cpp:173](../../../Servo/Servo.cpp#L173) |
| paths | data path | Get | `"data"` | inir | [Servo/Servo.cpp:174](../../../Servo/Servo.cpp#L174) |
| main | enable ipv6 | GetBoolean | `false` | inir | [Servo/Servo.cpp:175](../../../Servo/Servo.cpp#L175) |
| main | ip block timeout | GetInteger | `300` | inir | [Servo/Servo.cpp:176](../../../Servo/Servo.cpp#L176) |
| main | ip block attempts | GetInteger | `5` | inir | [Servo/Servo.cpp:177](../../../Servo/Servo.cpp#L177) |
| Paths | GFile Path | Get | `"gfiles"` | inir | [Talisman/Config.cpp:243](../../../Talisman/Config.cpp#L243) |
| Paths | Data Path | Get | `"data"` | inir | [Talisman/Config.cpp:244](../../../Talisman/Config.cpp#L244) |
| Paths | Menu Path | Get | `"menus"` | inir | [Talisman/Config.cpp:245](../../../Talisman/Config.cpp#L245) |
| Main | Root Menu | Get | `"main"` | inir | [Talisman/Config.cpp:246](../../../Talisman/Config.cpp#L246) |
| Main | Qwk ID | Get | `"TALISMAN"` | inir | [Talisman/Config.cpp:247](../../../Talisman/Config.cpp#L247) |
| Main | Location | Get | `"Somewhere, The World"` | inir | [Talisman/Config.cpp:248](../../../Talisman/Config.cpp#L248) |
| Paths | Message Path | Get | `"msgs"` | inir | [Talisman/Config.cpp:249](../../../Talisman/Config.cpp#L249) |
| Paths | Temp Path | Get | `"temp"` | inir | [Talisman/Config.cpp:250](../../../Talisman/Config.cpp#L250) |
| Paths | Script Path | Get | `"scripts"` | inir | [Talisman/Config.cpp:251](../../../Talisman/Config.cpp#L251) |
| Main | Sysop Name | Get | `"Sysop"` | inir | [Talisman/Config.cpp:252](../../../Talisman/Config.cpp#L252) |
| Main | System Name | Get | `"Talisman"` | inir | [Talisman/Config.cpp:253](../../../Talisman/Config.cpp#L253) |
| Paths | Netmail Semaphore | Get | `"netmail.sem"` | inir | [Talisman/Config.cpp:254](../../../Talisman/Config.cpp#L254) |
| Paths | Echomail Semaphore | Get | `"echomail.sem"` | inir | [Talisman/Config.cpp:255](../../../Talisman/Config.cpp#L255) |
| Paths | External Editor | Get | `""` | inir | [Talisman/Config.cpp:256](../../../Talisman/Config.cpp#L256) |
| Paths | Log Path | Get | `"logs"` | inir | [Talisman/Config.cpp:257](../../../Talisman/Config.cpp#L257) |
| Main | Input Background | Get | `"red"` | inir | [Talisman/Config.cpp:258](../../../Talisman/Config.cpp#L258) |
| Main | Input Foreground | Get | `"bright white"` | inir | [Talisman/Config.cpp:259](../../../Talisman/Config.cpp#L259) |
| Main | Max Nodes | GetInteger | `4` | inir | [Talisman/Config.cpp:260](../../../Talisman/Config.cpp#L260) |
| Main | New User Sec Level | GetInteger | `10` | inir | [Talisman/Config.cpp:261](../../../Talisman/Config.cpp#L261) |
| Main | New User Feedback | GetBoolean | `false` | inir | [Talisman/Config.cpp:262](../../../Talisman/Config.cpp#L262) |
| Main | Hostname | Get | `"localhost"` | inir | [Talisman/Config.cpp:263](../../../Talisman/Config.cpp#L263) |
| Main | Gopher Port | GetInteger | `-1` | inir | [Talisman/Config.cpp:264](../../../Talisman/Config.cpp#L264) |
| Main | New User Password | Get | `""` | inir | [Talisman/Config.cpp:265](../../../Talisman/Config.cpp#L265) |
| Main | Windows Local Echo | GetBoolean | `true` | inir | [Talisman/Config.cpp:266](../../../Talisman/Config.cpp#L266) |
| Main | Main AKA | Get | `"0:0/0"` | inir | [Talisman/Config.cpp:268](../../../Talisman/Config.cpp#L268) |
| Paths | Data Path | Get | `"data"` | inir | [Talisman/main.cpp:71](../../../Talisman/main.cpp#L71) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:244](../../../Toolbelt/main.cpp#L244) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:264](../../../Toolbelt/main.cpp#L264) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:336](../../../Toolbelt/main.cpp#L336) |
| paths | temp path | Get | `"data"` | inir | [Toolbelt/main.cpp:340](../../../Toolbelt/main.cpp#L340) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:385](../../../Toolbelt/main.cpp#L385) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:398](../../../Toolbelt/main.cpp#L398) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:437](../../../Toolbelt/main.cpp#L437) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:442](../../../Toolbelt/main.cpp#L442) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:449](../../../Toolbelt/main.cpp#L449) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:457](../../../Toolbelt/main.cpp#L457) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:465](../../../Toolbelt/main.cpp#L465) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:473](../../../Toolbelt/main.cpp#L473) |
| main | Message Base | Get | `""` | mailinir | [Toolbelt/main.cpp:593](../../../Toolbelt/main.cpp#L593) |
| main | Origin Address | Get | `""` | mailinir | [Toolbelt/main.cpp:594](../../../Toolbelt/main.cpp#L594) |
| main | Tagline | Get | `"A Talisman BBS"` | mailinir | [Toolbelt/main.cpp:595](../../../Toolbelt/main.cpp#L595) |
| main | To | Get | `"All"` | mailinir | [Toolbelt/main.cpp:596](../../../Toolbelt/main.cpp#L596) |
| main | From | Get | `"Toolbelt"` | mailinir | [Toolbelt/main.cpp:597](../../../Toolbelt/main.cpp#L597) |
| main | Subject | Get | `""` | mailinir | [Toolbelt/main.cpp:598](../../../Toolbelt/main.cpp#L598) |
| main | Message File | Get | `""` | mailinir | [Toolbelt/main.cpp:599](../../../Toolbelt/main.cpp#L599) |
| paths | data path | Get | `"data"` | inir | [Toolbelt/main.cpp:622](../../../Toolbelt/main.cpp#L622) |
| paths | message path | Get | `"msgs"` | inir | [Toolbelt/main.cpp:622](../../../Toolbelt/main.cpp#L622) |
| paths | message path | Get | `"msgs"` | inir | [Toolbelt/main.cpp:631](../../../Toolbelt/main.cpp#L631) |
| paths | message path | Get | `"msgs"` | inir | [Toolbelt/main.cpp:644](../../../Toolbelt/main.cpp#L644) |
| paths | data path | Get | `"data"` | inir | [Trinket/Trinket.cpp:144](../../../Trinket/Trinket.cpp#L144) |
| trinket | db host | Get | `"localhost"` | inir | [Trinket/Trinket.cpp:158](../../../Trinket/Trinket.cpp#L158) |
| trinket | db user | Get | `""` | inir | [Trinket/Trinket.cpp:158](../../../Trinket/Trinket.cpp#L158) |
| trinket | db password | Get | `""` | inir | [Trinket/Trinket.cpp:158](../../../Trinket/Trinket.cpp#L158) |
| trinket | db name | Get | `"trinket"` | inir | [Trinket/Trinket.cpp:158](../../../Trinket/Trinket.cpp#L158) |
