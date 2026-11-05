import importlib.util,os,sys,tempfile,threading,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
spec=importlib.util.spec_from_file_location('compiler',root/'tools/compilar.py');c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'clang').write_text('#!/usr/bin/env python3\nimport subprocess,time\np=subprocess.Popen(["sleep","120"])\nopen('+repr(str(p/'child'))+',"w").write(str(p.pid))\ntime.sleep(120)\n');(p/'clang').chmod(0o700)
 (p/'sample.c').write_text('int sample;')
 os.environ['PATH']=str(p)+':'+os.environ['PATH'];result=[]
 t=threading.Thread(target=lambda:result.append(c.compile_source(str(p/'sample.c'),str(p),'0',0)));t.start()
 deadline=time.monotonic()+5
 while not(p/'child').exists() and time.monotonic()<deadline:time.sleep(.02)
 assert (p/'child').exists(),'compiler did not start'
 pid=int((p/'child').read_text());c.cancel_compilation(15,None);t.join(5)
 assert not t.is_alive(),'cancellation did not return'
 assert result[0][1]=='Compilación cancelada',result
 assert not (p/'obj/sample.c.o').exists(),'cancelled object published'
 assert not list((p/'obj').glob('.compilar-*')),'temporary object leaked'
 status=Path(f'/proc/{pid}/stat')
 assert not status.exists() or status.read_text().split()[2]=='Z','compiler child is still running'
 print('PASS: cancellation stops compiler subtree, publishes no object, removes temporary output')
