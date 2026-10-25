"""Server-only isolated SDK -> native host -> HTTP -> PostgreSQL -> worker check.

Requires existing test/native images and built trainer_backend_play_probe.
Never mounts the production service, DB, OAuth secret or user vault.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import uuid

parser=argparse.ArgumentParser()
parser.add_argument('native_workspace',type=Path)
parser.add_argument('output',type=Path)
parser.add_argument('--backend-image',default='traineros-backend-test:play-20261025')
parser.add_argument('--native-image',default='traineros-account-ci:20261025-ui')
args=parser.parse_args()
root=args.native_workspace.resolve();output=args.output.resolve()
if not (root/'build-trixie/trainer_backend_play_probe').is_file():parser.error('Build the native probe first')
output.mkdir(parents=True,exist_ok=True)
live=Path(tempfile.mkdtemp(prefix='play-fixture-',dir=output))
name='traineros-play-fixture-'+uuid.uuid4().hex[:12]

def run(command,**kw):
    return subprocess.run(command,stdin=subprocess.DEVNULL,**kw)

def private_helper(program):
    run(['docker','run','--rm','--network','none','--user','0','-v',str(live)+':/fixture',args.backend_image,'python','-c',program],check=True)

runner='''import os,subprocess,time,tempfile
from pathlib import Path
with tempfile.TemporaryDirectory() as temporary:
 root=Path(temporary);processes=[];endpoints=[]
 os.environ['QT_QPA_PLATFORM']='offscreen';os.environ['QT_QUICK_BACKEND']='software';os.environ['XDG_DATA_HOME']=str(root/'data')
 try:
  for i in range(5):
   endpoint=root/f'peer-{i}'/'endpoint.json';endpoints.append(str(endpoint))
   processes.append(subprocess.Popen(['/work/build-trixie/trainer_backend_play_probe',f'/fixtures/player-{i}.json',str(endpoint)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL))
  deadline=time.monotonic()+25
  while not all(Path(p).exists() for p in endpoints) and time.monotonic()<deadline:time.sleep(0.1)
  if not all(Path(p).exists() for p in endpoints):raise RuntimeError('native_probe_start_failed:'+str([p.poll() for p in processes]))
  result=subprocess.run(['python3','/work/source/sdk/examples/scorecard_demo.py',*endpoints],capture_output=True,text=True)
  if result.returncode:raise RuntimeError(result.stderr)
  print(result.stdout)
  time.sleep(0.3)
 finally:
  for process in processes:process.terminate()
  for process in processes:process.wait(timeout=5)
'''
started=False
try:
    private_helper(f'import os;os.chown("/fixture",10001,{os.getgid()});os.chmod("/fixture",0o750)')
    run(['docker','run','-d','--rm','--name',name,'--network','none','--cpus','1','--memory','768m','--pids-limit','128',
         '-e','PLAY_FIXTURE_DIR=/fixture','-v',str(live)+':/fixture',args.backend_image,'python','tests/play_http_fixture.py'],check=True,stdout=subprocess.DEVNULL)
    started=True
    for _ in range(40):
        check=run(['docker','exec',name,'python','-c',"import urllib.request;urllib.request.urlopen('http://127.0.0.1:19091/health/ready',timeout=1)"],capture_output=True)
        if check.returncode==0:break
        time.sleep(0.2)
    else:raise RuntimeError('fixture_ready_timeout')
    result=run(['docker','run','--rm','--network','container:'+name,'--cpus','2','--memory','2g',
                '-v',str(root)+':/work:ro','-v',str(live)+':/fixtures:ro',args.native_image,'python3','-c',runner],capture_output=True,text=True)
    if result.returncode:raise RuntimeError(result.stderr[-6000:])
    report=json.loads(result.stdout)
    (output/'live-chain.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
finally:
    if started:run(['docker','stop','--time','15',name],stdout=subprocess.DEVNULL)
    private_helper('from pathlib import Path;import os\n'
        'for i in range(5): (Path("/fixture")/f"player-{i}.json").unlink(missing_ok=True)\n'
        f'os.chown("/fixture",{os.getuid()},{os.getgid()});os.chmod("/fixture",0o700)')
    live.rmdir()
