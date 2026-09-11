"""LOCAL ONLY: host checks and build commands are not part of the export."""
from pathlib import Path
import json
import os
import subprocess

ROOT = Path(__file__).resolve().parents[3]
P = Path(__file__).resolve().parent
SHARE = ROOT/'output/share/altscreen-hook-excerpts'
H = SHARE/'hook'
flags=['cc','-std=c99','-Wall','-Wextra','-Werror','-pedantic','-I',str(H)]

def run(args, **kwargs):
    result=subprocess.run([str(x) for x in args],check=True,text=True,capture_output=True,**kwargs)
    return result.stdout.strip()

run(flags+['-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',SHARE/'altscreen',
           P/'check_excerpts.c',SHARE/'altscreen/avcc.c',SHARE/'altscreen/frame_io.c',
           H/'process_scope.c','-o',P/'check_video_hook_helpers'])
run([P/'check_video_hook_helpers'])
run(flags+['-dynamiclib',P/'demo_provider.c','-o',P/'demo_provider.dylib'])
# The host harness uses an explicit provider dependency and flat namespace.
# This makes the fictional provider visible after the wrapper for RTLD_NEXT.
run(flags+['-dynamiclib',H/'interpose_example.c','-Wl,-needed_library,'+str(P/'demo_provider.dylib'),
           '-pthread','-o',P/'demo_hook.dylib'])
run(flags+['-dynamiclib',H/'interpose_example.c','-pthread','-o',P/'demo_hook_no_provider.dylib'])
run(flags+['-Wl,-flat_namespace',P/'check_interpose.c',P/'demo_provider.dylib','-pthread','-o',P/'check_interpose'])
run(flags+[P/'check_missing_provider.c','-o',P/'check_missing_provider'])
env=dict(os.environ)
env['DYLD_INSERT_LIBRARIES']=str(P/'demo_hook.dylib')
env['DYLD_FORCE_FLAT_NAMESPACE']='1'
output=[run([P/'check_interpose'],env=env),run([P/'check_missing_provider',P/'demo_hook_no_provider.dylib'])]
result={'helper_c99_asan_ubsan':'passed','dynamic_interposition':'passed','concurrent_calls':256,
        'original_arguments_status_errno':'preserved','missing_provider':'passed',
        'host':'macOS, fictional provider with flat namespace/explicit dependency', 'output':output}
(P/'test_results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
