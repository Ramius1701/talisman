"""Generate source evidence for a Talisman checkout; standard-library only.

Usage: python generate_reference.py SOURCE_ROOT OUTPUT_DIRECTORY [--check]
--check compares generated output without writing. This is an evidence extractor,
not a C++ parser or proof of runtime behavior. Review prose after source changes.
"""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
check = '--check' in sys.argv[3:]
components = ['Talisman', 'Servo', 'Toolbelt', 'Postie', 'Binki', 'Falcon',
              'Qwkie', 'Bridge', 'Gofer', 'NewsSrv', 'HttpSrv', 'Trinket', 'Common', 'cmake']
files = sorted(p for component in components for p in (root/component).rglob('*')
               if p.is_file() and p.suffix.lower() in {'.cpp', '.h', '.hpp', '.ini', '.toml', '.lua', '.txt', '.vcxproj'}
               and 'win32_deps' not in p.parts and 'librethinkdbxx' not in p.parts)
texts = {p.relative_to(root).as_posix(): p.read_text(encoding='utf-8', errors='replace') for p in files}
def cell(value):
    return str(value).replace('|', '&#124;').replace('\n', ' ')
def link(path, line):
    # Generated pages live at docs/manual/reference/.
    return f'[{path}:{line}](../../../{path}#L{line})'
def table(headers, rows):
    return '\n'.join(['| '+' | '.join(headers)+' |', '| '+' | '.join(['---']*len(headers))+' |']
                     + ['| '+' | '.join(cell(x) for x in row)+' |' for row in rows])+'\n'
def page(title, intro):
    return f'# {title}\n\nGenerated from the checkout by `generate_reference.py`. '+intro+'\n\n'
products = {}
ini = []
for path, text in texts.items():
    if not path.endswith('.cpp'):
        continue
    for no, line in enumerate(text.splitlines(), 1):
        # Simple literal reads only; dynamic section names deliberately excluded.
        for m in re.finditer(r'(\w+)\.(Get(?:Integer|Boolean|Real)?)\("([^"]*)",\s*"([^"]*)",\s*(.*?)\)', line):
            ini.append((m[3], m[4], m[2], '`'+m[5]+'`', m[1], link(path,no)))
products['ini-readers.md'] = page('INI reader inventory',
    'Every matched literal section/key read is listed, including repeated readers with different defaults. '
    'Defaults are C++ expressions, not sample configuration values. Reader names help distinguish talisman.ini from other INI files; inspect the linked function for the filename. '
    'Dynamic section reads are outside this inventory.') + table(['Section','Key','Reader type','Fallback expression','Reader variable','Source'], ini)

toml_rows=[]
for path,text in texts.items():
    if not path.endswith('.cpp'):
        continue
    for no,line in enumerate(text.splitlines(),1):
        if any(token in line for token in ['toml::parse_file(', 'get_as<toml::array>', '->get("', '.value_or(', '->value_or(']) or re.search(r'data\["',line):
            toml_rows.append((link(path,no), '`'+line.strip()+'`'))
products['toml-readers.md'] = page('TOML reader evidence',
    'Ordered source statements show filenames, table names, key reads and fallback expressions. '
    'This is an evidence index, not a normalized schema: null branches, cross-field defaults, required arrays, and type handling must be read in context. '
    'An absent key and a wrong type are not necessarily equivalent.') + table(['Source','Statement'],toml_rows)

menu=texts['Talisman/Menu.cpp']
commands=[(m[1],link('Talisman/Menu.cpp',menu[:m.start()].count('\n')+1))
          for m in re.finditer(r'strcasecmp\(items\[i\]\.command\.c_str\(\),\s*"([^"]+)"\)',menu)]
node=texts['Talisman/Node.cpp']
login=[(m[1],link('Talisman/Node.cpp',node[:m.start()].count('\n')+1))
       for m in re.finditer(r'strcasecmp\(config\.get_login_items\(\)->at\(i\)\.command\.c_str\(\),\s*"([^"]+)"\)',node)]
products['commands.md']=page('Menu and login dispatch inventory',
    'These are the exact strings recognized by separate dispatchers; comparison is case insensitive. '
    'Do not assume a menu command is also a login command. Semantics and data arguments are explained in the menu guide.')+'## Menu commands\n\n'+table(['Command','Dispatcher'],commands)+'\n## Login commands\n\n'+table(['Command','Dispatcher'],login)

tokens=[]
for path in ['Talisman/Node.cpp', 'Talisman/MessageReader.cpp']:
    for no,line in enumerate(texts[path].splitlines(),1):
        match=re.search(r'compare_token\(ss\.str\(\), "([^"]+)"\)',line)
        if match:
            tokens.append((match[1],link(path,no)))
products['display-tokens.md']=page('Display macro inventory',
    'Literal compare_token branches in Node and MessageReader are listed. Enclose tokens in @ delimiters in display assets. '
    'This index does not certify padding behavior or enumerate pipe colors/escape sequences; read the linked rendering branch. '
    'RUNSCRIPT: is handled separately as an executable macro prefix.')+table(['Token','Source'],tokens)

script=texts['Talisman/Script.cpp']
regs=list(re.finditer(r'lua_pushcfunction\(l,\s*(\w+)\);\s*lua_setglobal\(l,\s*"([^"]+)"\);',script))
lua_rows=[]
lua_bodies=[]
for reg in regs:
    match=re.search(r'extern "C" int '+re.escape(reg[1])+r'\(lua_State \*L\) \{',script)
    if match is None:
        raise ValueError('Missing Lua wrapper '+reg[1])
    # All wrappers in this revision terminate at a column-zero closing brace.
    end=script.index('\n}',match.end())+2
    body=script[match.start():end]
    line=script[:match.start()].count('\n')+1
    reads=list(dict.fromkeys(re.findall(r'lua_to\w+\(L,\s*(-?\d+)\)',body)))
    returns=list(dict.fromkeys(re.findall(r'return (\d+);',body)))
    lua_rows.append((reg[2],reg[1],', '.join(reads) or 'none',', '.join(returns),link('Talisman/Script.cpp',line)))
    lua_bodies.append(f'## {reg[2]}\n\nSource: {link("Talisman/Script.cpp",line)}.\n\n```cpp\n{body}\n```\n')
products['lua-api.md']=page('Lua API evidence',
    'All matched registered C functions are indexed with exact wrapper implementations below. '
    'Stack indices are raw C API positions (negative indices count from the top); return counts describe branches, not semantic types. '
    'Use the wrappers to establish argument order, return values, error sentinels and side effects before extending scripts. '
    'Wrapper excerpts retain the project source license.')+table(['Lua global','C wrapper','Read stack indices','Return counts','Source'],lua_rows)+'\n'+'\n'.join(lua_bodies)

try:
    revision=subprocess.check_output(['git','-c',f'safe.directory={root.as_posix()}','-C',str(root),'rev-parse','HEAD'],text=True).strip()
except subprocess.CalledProcessError:
    revision='unavailable'
manifest={'revision':revision,'scope':'Source and sample text files in named components, excluding bundled win32_deps and librethinkdbxx; docs and binaries excluded.',
          'sha256':{p.relative_to(root).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
          'counts':{'ini_reads':len(ini),'toml_statements':len(toml_rows),'menu_commands':len(commands),'login_commands':len(login),'lua_functions':len(regs)}}
products['source-manifest.json']=json.dumps(manifest,indent=2,sort_keys=True)+'\n'
failed=[]
for name,content in products.items():
    destination=out/name
    if check:
        if not destination.exists() or destination.read_text(encoding='utf-8')!=content:
            failed.append(name)
    else:
        out.mkdir(parents=True,exist_ok=True)
        destination.write_text(content,encoding='utf-8',newline='\n')
print(json.dumps({'mode':'check' if check else 'generate','counts':manifest['counts'],'differences':failed}))
sys.exit(bool(failed))
