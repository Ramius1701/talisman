"""Exercise actual BBS output and interactive editor on an inherited loopback socket."""
from pathlib import Path
import hashlib, json, os, shutil, socket, sqlite3, subprocess, sys, time
from datetime import datetime, timezone
here=Path(__file__).resolve().parent
inputs=json.loads((here/'build-inputs.json').read_text())
source=Path(inputs['source']); deps=Path(inputs['dependencies'])
dest=here/(sys.argv[1] if len(sys.argv)>1 else 'runtime-01')
plain='--plain' in sys.argv
if dest.exists(): raise SystemExit('Use a fresh runtime directory.')
dest.mkdir()
for folder in ['data','menus','gfiles']: shutil.copytree(source/'Talisman'/folder,dest/folder)
for folder in ['scripts','msgs','logs','temp','dloads/general/uploads','dloads/general/misc']: (dest/folder).mkdir(parents=True,exist_ok=True)
shutil.copy2(here/'build/Release/talisman.exe',dest/'talisman.exe')
for dll in (deps/'bin').glob('*.dll'): shutil.copy2(dll,dest/dll.name)
for file in (source/'Talisman/gfiles').iterdir(): shutil.copy2(file,dest/'gfiles'/file.name)
shutil.copy2(source/'Talisman/scripts/color-demo.lua',dest/'scripts/color-demo.lua')
(dest/'talisman.ini').write_text('''[main]
root menu = main
system name = Color Verification
sysop name = Fixture
windows local echo = false
new user sec level = 10
max nodes = 4
ssh port = -1
gopher port = -1
input foreground = #010203
input background = 21
[paths]
gfile path = gfiles
data path = data
menu path = menus
message path = msgs
log path = logs
temp path = temp
script path = scripts
''')
(dest/'data/loginitems.toml').write_text('loginitem = []\n')
(dest/'menus/main.toml').write_text('''[menu]
description = "Color Verification"
prompt = "VERIFY>"
[[menuitem]]
command = "RUNSCRIPT"
hotkey = "C"
data = "test-colors"
[[menuitem]]
command = "RUNSCRIPT"
hotkey = "E"
data = "test-editor"
[[menuitem]]
command = "RUNSCRIPT"
hotkey = "D"
data = "color-demo"
[[menuitem]]
command = "GOODBYE"
hotkey = "G"
''')
(dest/'scripts/test-colors.lua').write_text('''bbs_write_string("|01CLASSIC|FG:196|INDEX|BG:21|BLUE|FG:#010203||BG:#040506|RGB|07\\r\\n")
bbs_display_gfile("color-demo-256")
bbs_display_gfile("color-demo-truecolor")
bbs_write_string("\\27[0mCOLOR_DONE\\r\\n")
''')
(dest/'scripts/test-editor.lua').write_text('''bbs_edit_ansi(80, 25, "editor-test.ans")
bbs_write_string("\\27[0mEDITOR_DONE\\r\\n")
''')
with sqlite3.connect(dest/'data/users.sqlite3') as db:
 db.execute('CREATE TABLE users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT)')
 salt='fixture-only-salt'; password=hashlib.sha256(('TestPass123'+salt).encode()).hexdigest().upper()
 db.execute('INSERT INTO users VALUES(1,?,?,?)',('ColorTester',password,salt))
listener=socket.socket(); listener.bind(('127.0.0.1',0)); listener.listen(1)
client=socket.create_connection(listener.getsockname()); server,_=listener.accept(); listener.close()
server.set_inheritable(True); client.settimeout(.25)
buffer=b''; transcript=bytearray()
def send(text): client.sendall(text.encode())
def expect(text,seconds=25):
 global buffer
 until=time.monotonic()+seconds; target=text.encode()
 while target not in buffer:
  if time.monotonic()>until: raise AssertionError(f'Missing {text!r}; recent bytes {buffer[-500:]!r}')
  try: data=client.recv(65536)
  except socket.timeout: continue
  if not data: raise AssertionError(f'EOF before {text!r}: {buffer[-500:]!r}')
  buffer+=data; transcript.extend(data)
 buffer=buffer[buffer.index(target)+len(target):]
results={'utc':datetime.now(timezone.utc).isoformat(),'method':'Modified x86 Talisman; loopback inherited socket, bypasses Servo; byte capture, not visual terminal emulation.','checks':[]}
with (dest/'session-process.log').open('w') as log:
 proc=subprocess.Popen([str(dest/'talisman.exe'),'-N','1','-S',str(server.fileno()),'-T'],cwd=dest,close_fds=False,creationflags=subprocess.CREATE_NO_WINDOW,stdout=log,stderr=subprocess.STDOUT,env={k.upper():v for k,v in os.environ.items()})
 server.close()
 try:
  expect('use it anyway?'); send('n' if plain else 'y')
  expect('LOGIN:'); send('ColorTester\r')
  expect('PASSW:'); send('TestPass123\r')
  expect('VERIFY>'); send('c'); expect('COLOR_DONE'); expect('VERIFY>')
  if plain:
   assert b'CLASSICINDEXBLUERGB' in transcript
   for token in [b'|FG:',b'|BG:',b'\x1b[38;5;',b'\x1b[38;2;',b'\x1b[48;5;',b'\x1b[48;2;']:
    assert token not in transcript,repr(token)
   assert b'ANSI color demo requires an ANSI terminal.' in transcript
   results['checks'].append('Non-ANSI callers: color tokens stripped and ASCII asset fallback selected')
  else:
   for value in ['\x1b[0;34mCLASSIC','\x1b[38;5;196mINDEX','\x1b[48;5;21mBLUE','\x1b[38;2;1;2;3m\x1b[48;2;4;5;6mRGB','\x1b[22;38;2;1;2;3m','\x1b[48;5;21m','TALISMAN / 256 COLOR ARTWORK','TALISMAN / TRUECOLOR COLOR ARTWORK']:
    assert value.encode() in transcript,repr(value)
    results['checks'].append(value)
   for choice,label in [('1','16'),('2','256'),('3','TRUECOLOR')]:
    send('d'); expect('Truecolor RGB:'); send(choice); expect(f'TALISMAN / {label} COLOR ARTWORK'); expect('Press any key to return.'); send('x'); expect('VERIFY>')
   results['checks'].append('Shipped showcase script selects all three color-depth assets')
   send('e'); expect('Ctrl-Z For Options')
   send('\x1ac'); expect('Foreground or background'); send('f'); expect('blank cancels):'); send('#123456\r'); expect('Ctrl-Z For Options')
   send('\x1ac'); expect('Foreground or background'); send('b'); expect('blank cancels):'); send('196\r'); expect('Ctrl-Z For Options')
   send('Z'); expect('\x1b[22;38;2;18;52;86;48;5;196mZ')
   send('\x1as'); expect('Ctrl-Z For Options')
   send('\x1aq'); expect('EDITOR_DONE'); expect('VERIFY>')
   saved=(dest/'editor-test.ans').read_bytes()
   assert b'\x1b[22;38;2;18;52;86;48;5;196mZ' in saved
   results['checks'].append('Interactive RGB foreground / indexed background selection, typed output and saved file')
  send('g'); proc.wait(timeout=10)
  results['session_exit_code']=proc.returncode; results['pass']=True
 finally:
  client.close()
  if proc.poll() is None: proc.terminate(); proc.wait(timeout=5)
  (dest/'transcript.bin').write_bytes(transcript)
  (here/('session-plain-results.json' if plain else 'session-results.json')).write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
