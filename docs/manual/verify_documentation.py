"""Static manual QA, without executing the BBS or changing its data.

Usage: python verify_documentation.py SOURCE_ROOT MANUAL_DIRECTORY
Supports staging the manual outside the repository by mapping its relative links
to their final docs/manual location. Requires Python 3.11+ for tomllib.
"""
import json
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
import posixpath
import re
import subprocess
import sys
import tomllib
from urllib.parse import unquote

root=Path(sys.argv[1]).resolve()
manual=Path(sys.argv[2]).resolve()
broken=[]
links=0
for page in sorted(manual.rglob('*.md')):
    text=page.read_text(encoding='utf-8')
    logical=PurePosixPath('docs/manual') / page.relative_to(manual).as_posix()
    # Check ordinary Markdown links including line-fragment source citations.
    for target in re.findall(r'\]\(([^)]+)\)', text):
        if '://' in target or target.startswith('#'):
            continue
        target=unquote(target.split('#')[0])
        resolved=posixpath.normpath(str(logical.parent / target))
        if resolved.startswith('docs/manual/'):
            actual=manual/resolved[len('docs/manual/'):]
        else:
            actual=root/resolved
        links+=1
        if not actual.exists() and actual != manual/'verification-results.json':
            broken.append({'page':page.relative_to(manual).as_posix(),'target':target})

toml_errors=[]
samples=list((root/'Talisman/data').glob('*.toml'))+list((root/'Talisman/menus').glob('*.toml'))
for sample in sorted(samples):
    try:
        tomllib.loads(sample.read_text(encoding='utf-8'))
    except (tomllib.TOMLDecodeError,UnicodeError) as exc:
        toml_errors.append({'path':sample.relative_to(root).as_posix(),'error':str(exc)})

commands=re.findall(r'strcasecmp\(items\[i\]\.command\.c_str\(\),\s*"([^"]+)"\)',
                    (root/'Talisman/Menu.cpp').read_text(encoding='utf-8'))
guide=(manual/'menus-and-display.md').read_text(encoding='utf-8')
missing=[cmd for cmd in commands if '`'+cmd+'`' not in guide]
fresh=subprocess.run([sys.executable,str(manual/'generate_reference.py'),str(root),str(manual/'reference'),'--check'],
                     text=True,capture_output=True)
results={'checked_at_utc':datetime.now(timezone.utc).isoformat(),'scope':'Static documentation checks only; this checker does not execute the BBS or a build.',
         'markdown_pages':len(list(manual.rglob('*.md'))),'local_links_checked':links,'broken_links':broken,
         'core_sample_toml_files':len(samples),'sample_syntax_errors':toml_errors,
         'menu_commands':len(commands),'menu_commands_missing_from_guide':missing,
         'reference_freshness_exit_code':fresh.returncode,'reference_freshness_output':fresh.stdout.strip()}
(manual/'verification-results.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
print(json.dumps(results,indent=2))
sys.exit(bool(broken or toml_errors or missing or fresh.returncode))
