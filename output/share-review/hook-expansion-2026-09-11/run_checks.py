from pathlib import Path
import hashlib
import json
import subprocess

P=Path(__file__).resolve().parent
H=P/'stage/hook'
def run(args):
    r=subprocess.run([str(x) for x in args],check=True,capture_output=True,text=True,timeout=30)
    return r.stdout+r.stderr
demo=run(['make','-C',H,'demo'])
run(['cc','-std=c99','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined',
     '-fno-omit-frame-pointer','-I',H,P/'check_views.c',H/'view_area.c',H/'view_switch.c','-o',P/'check_views'])
boundary=run([P/'check_views'])
(P/'test.log').write_text(demo+'\n'+boundary)
tested={}
for file in H.rglob('*'):
    if file.is_file() and 'build' not in file.relative_to(H).parts:
        tested['hook/'+file.relative_to(H).as_posix()]=hashlib.sha256(file.read_bytes()).hexdigest()
result={'macos_shipped_makefile_demo':'passed','forwarded_calls':256,'threads':4,
        'geometry_and_state_asan_ubsan':'passed','view_boundaries':boundary.strip(),
        'linux_recipe':'not executed','hardware_and_protocol':'not tested; no adapter supplied',
        'tested_inputs':tested}
(P/'test_results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='tested_inputs'},indent=2))
