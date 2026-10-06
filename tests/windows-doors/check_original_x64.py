from pathlib import Path
import os
import json
import subprocess
import sys

here=Path(__file__).resolve().parent
output=here/'original-x64'
output.mkdir(exist_ok=True)
msbuild='C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe'
source=Path(json.loads((here/'build-inputs.json').read_text())['source'])
args=[msbuild,str(source/'Talisman/Talisman.vcxproj'),'/t:Build','/p:Configuration=Release',
      '/p:Platform=x64','/p:VcpkgEnabled=false',f'/p:IntDir={output}\\obj\\',f'/p:OutDir={output}\\bin\\']
retarget='--retarget' in sys.argv
if retarget: args.append('/p:PlatformToolset=v145')
logfile=here/('original-x64-retargeted.log' if retarget else 'original-x64.log')
with logfile.open('w') as log:
    result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,env={k.upper():v for k,v in os.environ.items()})
print('Original x64 project exit code:',result.returncode)
lines=logfile.read_text().splitlines()
errors=[line for line in lines if ': error ' in line or ': fatal error ' in line]
print('\n'.join(errors[:8]))
