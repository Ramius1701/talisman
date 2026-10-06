# Menu and login dispatch inventory

Generated from the checkout by `generate_reference.py`. These are the exact strings recognized by separate dispatchers; comparison is case insensitive. Do not assume a menu command is also a login command. Semantics and data arguments are explained in the menu guide.

## Menu commands

| Command | Dispatcher |
| --- | --- |
| goodbye | [Talisman/Menu.cpp:257](../../../Talisman/Menu.cpp#L257) |
| prevmenu | [Talisman/Menu.cpp:259](../../../Talisman/Menu.cpp#L259) |
| submenu | [Talisman/Menu.cpp:262](../../../Talisman/Menu.cpp#L262) |
| listconfs | [Talisman/Menu.cpp:269](../../../Talisman/Menu.cpp#L269) |
| listareas | [Talisman/Menu.cpp:290](../../../Talisman/Menu.cpp#L290) |
| nextmailconf | [Talisman/Menu.cpp:309](../../../Talisman/Menu.cpp#L309) |
| prevmailconf | [Talisman/Menu.cpp:327](../../../Talisman/Menu.cpp#L327) |
| nextfileconf | [Talisman/Menu.cpp:346](../../../Talisman/Menu.cpp#L346) |
| prevfileconf | [Talisman/Menu.cpp:364](../../../Talisman/Menu.cpp#L364) |
| nextmailarea | [Talisman/Menu.cpp:383](../../../Talisman/Menu.cpp#L383) |
| prevmailarea | [Talisman/Menu.cpp:397](../../../Talisman/Menu.cpp#L397) |
| nextfilearea | [Talisman/Menu.cpp:411](../../../Talisman/Menu.cpp#L411) |
| prevfilearea | [Talisman/Menu.cpp:426](../../../Talisman/Menu.cpp#L426) |
| listmsgs | [Talisman/Menu.cpp:440](../../../Talisman/Menu.cpp#L440) |
| postmsg | [Talisman/Menu.cpp:488](../../../Talisman/Menu.cpp#L488) |
| mailscan | [Talisman/Menu.cpp:572](../../../Talisman/Menu.cpp#L572) |
| last10 | [Talisman/Menu.cpp:577](../../../Talisman/Menu.cpp#L577) |
| rundoor | [Talisman/Menu.cpp:583](../../../Talisman/Menu.cpp#L583) |
| sysinfo | [Talisman/Menu.cpp:598](../../../Talisman/Menu.cpp#L598) |
| settings | [Talisman/Menu.cpp:603](../../../Talisman/Menu.cpp#L603) |
| postemail | [Talisman/Menu.cpp:607](../../../Talisman/Menu.cpp#L607) |
| listemail | [Talisman/Menu.cpp:630](../../../Talisman/Menu.cpp#L630) |
| feedback | [Talisman/Menu.cpp:634](../../../Talisman/Menu.cpp#L634) |
| listusers | [Talisman/Menu.cpp:651](../../../Talisman/Menu.cpp#L651) |
| bulletins | [Talisman/Menu.cpp:656](../../../Talisman/Menu.cpp#L656) |
| fileconfs | [Talisman/Menu.cpp:660](../../../Talisman/Menu.cpp#L660) |
| fileareas | [Talisman/Menu.cpp:681](../../../Talisman/Menu.cpp#L681) |
| listfiles | [Talisman/Menu.cpp:700](../../../Talisman/Menu.cpp#L700) |
| download | [Talisman/Menu.cpp:715](../../../Talisman/Menu.cpp#L715) |
| cleartagged | [Talisman/Menu.cpp:738](../../../Talisman/Menu.cpp#L738) |
| upload | [Talisman/Menu.cpp:741](../../../Talisman/Menu.cpp#L741) |
| runscript | [Talisman/Menu.cpp:765](../../../Talisman/Menu.cpp#L765) |
| nlbrowse | [Talisman/Menu.cpp:772](../../../Talisman/Menu.cpp#L772) |
| msgreadnew | [Talisman/Menu.cpp:776](../../../Talisman/Menu.cpp#L776) |
| msgupdatelr | [Talisman/Menu.cpp:811](../../../Talisman/Menu.cpp#L811) |
| newfiles | [Talisman/Menu.cpp:879](../../../Talisman/Menu.cpp#L879) |
| filesearch | [Talisman/Menu.cpp:900](../../../Talisman/Menu.cpp#L900) |
| msgsearch | [Talisman/Menu.cpp:946](../../../Talisman/Menu.cpp#L946) |
| msgsubareas | [Talisman/Menu.cpp:1017](../../../Talisman/Menu.cpp#L1017) |
| bwavedown | [Talisman/Menu.cpp:1143](../../../Talisman/Menu.cpp#L1143) |
| qwkdown | [Talisman/Menu.cpp:1146](../../../Talisman/Menu.cpp#L1146) |
| qwkup | [Talisman/Menu.cpp:1149](../../../Talisman/Menu.cpp#L1149) |
| bwaveup | [Talisman/Menu.cpp:1152](../../../Talisman/Menu.cpp#L1152) |
| phlognew | [Talisman/Menu.cpp:1155](../../../Talisman/Menu.cpp#L1155) |
| phlogmanage | [Talisman/Menu.cpp:1163](../../../Talisman/Menu.cpp#L1163) |
| phlogrecent | [Talisman/Menu.cpp:1165](../../../Talisman/Menu.cpp#L1165) |
| editsig | [Talisman/Menu.cpp:1167](../../../Talisman/Menu.cpp#L1167) |
| indexreader | [Talisman/Menu.cpp:1185](../../../Talisman/Menu.cpp#L1185) |
| telnet_ip4 | [Talisman/Menu.cpp:1193](../../../Talisman/Menu.cpp#L1193) |
| telnet_ip6 | [Talisman/Menu.cpp:1218](../../../Talisman/Menu.cpp#L1218) |
| rlogin_ip4 | [Talisman/Menu.cpp:1243](../../../Talisman/Menu.cpp#L1243) |
| rlogin_ip6 | [Talisman/Menu.cpp:1289](../../../Talisman/Menu.cpp#L1289) |
| nodemsg | [Talisman/Menu.cpp:1334](../../../Talisman/Menu.cpp#L1334) |

## Login commands

| Command | Dispatcher |
| --- | --- |
| QUICKLOGIN | [Talisman/Node.cpp:2023](../../../Talisman/Node.cpp#L2023) |
| SELECTTHEME | [Talisman/Node.cpp:2031](../../../Talisman/Node.cpp#L2031) |
| SENDGFILE | [Talisman/Node.cpp:2033](../../../Talisman/Node.cpp#L2033) |
| BULLETINS | [Talisman/Node.cpp:2035](../../../Talisman/Node.cpp#L2035) |
| EMAILCHECK | [Talisman/Node.cpp:2037](../../../Talisman/Node.cpp#L2037) |
| MAILSCAN | [Talisman/Node.cpp:2056](../../../Talisman/Node.cpp#L2056) |
| NEWFILES | [Talisman/Node.cpp:2061](../../../Talisman/Node.cpp#L2061) |
| LAST10 | [Talisman/Node.cpp:2084](../../../Talisman/Node.cpp#L2084) |
| RUNSCRIPT | [Talisman/Node.cpp:2086](../../../Talisman/Node.cpp#L2086) |
| RUNDOOR | [Talisman/Node.cpp:2091](../../../Talisman/Node.cpp#L2091) |
| MSGREADNEW | [Talisman/Node.cpp:2107](../../../Talisman/Node.cpp#L2107) |
