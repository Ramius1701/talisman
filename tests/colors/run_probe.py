from pathlib import Path
import os, json, subprocess
here=Path(__file__).resolve().parent
inputs=json.loads((here/'build-inputs.json').read_text())
env={k.upper():v for k,v in os.environ.items()}
env['PATH']=str(Path(inputs['dependencies'])/'bin')+';'+env['PATH']
result=subprocess.run([str(here/'build/Release/color-probe.exe')],cwd=here,env=env,capture_output=True,text=True)
(here/'probe-results.txt').write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr,end='')
raise SystemExit(result.returncode)
