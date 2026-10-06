from pathlib import Path
import os
import subprocess
import sys

here=Path(__file__).resolve().parent
platform=sys.argv[1]
folder=here/('build-'+platform+'-clean')
env={key.upper():value for key,value in os.environ.items()}
commands=[['cmake','-S',str(here),'-B',str(folder),'-A','Win32' if platform=='x86' else 'x64'],
          ['cmake','--build',str(folder),'--config','Release','--target',*sys.argv[2:],'--parallel','4']]
with (here/('build-'+platform+'.log')).open('w') as log:
    for cmd in commands:
        print('Running:', ' '.join(cmd),flush=True)
        result=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT,env=env)
        log.flush()
        if result.returncode:
            print((here/('build-'+platform+'.log')).read_text()[-5000:])
            sys.exit(result.returncode)
print('Build passed')
