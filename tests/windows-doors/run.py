"""Run original x86 Talisman -> x86/x64 native doors on inherited loopback sockets."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import socket
import sqlite3
import struct
import subprocess
import sys
import time

here=Path(__file__).resolve().parent
inputs=json.loads((here/'build-inputs.json').read_text())
source=Path(inputs['source'])
deps=Path(inputs['dependencies'])
dest=here/(sys.argv[1] if len(sys.argv)>1 else 'runtime-01')
if dest.exists():
    raise SystemExit('Choose a new runtime destination; never overwrite a prior fixture.')
dest.mkdir()
for folder in ['data','menus','gfiles']:
    shutil.copytree(source/'Talisman'/folder,dest/folder)
for folder in ['scripts','msgs','logs','temp','dloads/general/uploads','dloads/general/misc']:
    (dest/folder).mkdir(parents=True,exist_ok=True)
shutil.copy2(here/'build-x86-clean/bin/Release/talisman.exe',dest/'talisman.exe')
for architecture in ['x86','x64']:
    shutil.copy2(here/f'build-{architecture}-clean/bin/Release/door-probe.exe',dest/f'door-{architecture}.exe')
for dll in (deps/'bin').glob('*.dll'):
    shutil.copy2(dll,dest/dll.name)
(dest/'talisman.ini').write_text('''[main]
root menu = main
system name = Door Verification
sysop name = Fixture
windows local echo = false
new user sec level = 10
max nodes = 4
ssh port = -1
gopher port = -1
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
description = "Door Verification"
prompt = "VERIFY>"
[[menuitem]]
command = "RUNDOOR"
hotkey = "A"
data = "./door-x86.exe"
[[menuitem]]
command = "RUNDOOR"
hotkey = "B"
data = "./door-x64.exe"
[[menuitem]]
command = "GOODBYE"
hotkey = "G"
''')
with sqlite3.connect(dest/'data/users.sqlite3') as db:
    db.execute('CREATE TABLE users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT)')
    salt='fixture-only-salt'
    password=hashlib.sha256(('TestPass123'+salt).encode()).hexdigest().upper()
    db.execute('INSERT INTO users VALUES(1,?,?,?)',('DoorTester',password,salt))

def pe_machine(path):
    with path.open('rb') as f:
        f.seek(0x3c); offset=struct.unpack('<I',f.read(4))[0]
        f.seek(offset)
        assert f.read(4)==b'PE\0\0'
        return hex(struct.unpack('<H',f.read(2))[0])

listener=socket.socket()
listener.bind(('127.0.0.1',0)); listener.listen(1)
client=socket.create_connection(listener.getsockname()); server,_=listener.accept()
listener.close()
server.set_inheritable(True)
client.settimeout(.25)
buffer=b''
transcript=bytearray()
def send(text): client.sendall(text.encode())
def expect(text,seconds=25):
    global buffer
    until=time.monotonic()+seconds
    target=text.encode()
    while target not in buffer:
        if time.monotonic()>until:
            raise AssertionError(f'Missing {text!r}; recent bytes {buffer[-1000:]!r}')
        try: data=client.recv(8192)
        except socket.timeout: continue
        if not data: raise AssertionError(f'EOF before {text!r}: {buffer[-1000:]!r}')
        buffer+=data; transcript.extend(data)
    buffer=buffer[buffer.index(target)+len(target):]

revision=subprocess.check_output(['git','-c',f'safe.directory={source.as_posix()}','-C',str(source),'rev-parse','HEAD'],text=True).strip()
results={'source_revision':revision,'date':'2026-10-07',
         'method':'Unmodified preserved Talisman source built x86 with local dependency libraries; direct inherited loopback socket bypasses Servo.',
         'pe_machines':{name:pe_machine(dest/name) for name in ['talisman.exe','door-x86.exe','door-x64.exe']},
         'tests':[]}
with (dest/'session-process.log').open('w') as log:
    proc=subprocess.Popen([str(dest/'talisman.exe'),'-N','1','-S',str(server.fileno()),'-T'],
                          cwd=dest,close_fds=False,creationflags=subprocess.CREATE_NO_WINDOW,
                          stdout=log,stderr=subprocess.STDOUT,env={k.upper():v for k,v in os.environ.items()})
    server.close()
    try:
        expect('use it anyway?'); send('n')
        expect('LOGIN:'); send('DoorTester\r')
        expect('PASSW:'); send('TestPass123\r')
        expect('VERIFY>')
        for key,bits in [('a',32),('b',64)]:
            send(key)
            expect(f'DOOR_BITS:{bits}:DoorTester')
            send('CHALLENGE_64BIT\r\n')
            expect(f'DOOR_REPLY_OK:{bits}')
            expect('VERIFY>')
            report=(dest/f'door-result-{bits}.txt').read_text()
            assert f'pointer_bits={bits}' in report and 'duplex=pass' in report
            results['tests'].append({'door_bits':bits,'dropfile':'pass','bidirectional_socket':'pass','return_to_menu':'pass','report':report})
        send('g')
        proc.wait(timeout=10)
        results['session_exit_code']=proc.returncode
        results['pass']=True
    except Exception as exc:
        results['pass']=False; results['error']=str(exc)
        raise
    finally:
        client.close()
        if proc.poll() is None:
            proc.terminate(); proc.wait(timeout=5)
        (dest/'transcript.bin').write_bytes(transcript)
        (here/'results.json').write_text(json.dumps(results,indent=2)+'\n')
        print(json.dumps(results,indent=2))
