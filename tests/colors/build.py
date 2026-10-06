from pathlib import Path
import os, subprocess
here=Path(__file__).resolve().parent
env={k.upper():v for k,v in os.environ.items()}
commands=[['cmake','-S',str(here),'-B',str(here/'build'),'-A','Win32'],
          ['cmake','--build',str(here/'build'),'--config','Release','--parallel','4']]
with (here/'build.log').open('w') as log:
    for command in commands:
        r=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
        if r.returncode: log.flush(); raise SystemExit((here/'build.log').read_text()[-6000:])
print('Build passed')
