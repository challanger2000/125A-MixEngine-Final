#!/usr/bin/env python3
"""Reproduce the FULL/audit program and SDK drum-loop bit comparisons.
No worktree, release build, package, dependency edit or fixture redistribution.
Run from this checkout with the setup toolchain activated. Outputs are outside it.
"""
from pathlib import Path
import argparse,hashlib,io,json,math,struct,subprocess,tarfile,wave
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk-root',type=Path,default=Path('/workspace/dependencies/vst3sdk'))
p.add_argument('--hiir-root',type=Path,default=Path('/workspace/dependencies/hiir'))
p.add_argument('--output',type=Path,default=Path('/workspace/build/program-reference'))
a=p.parse_args();root=Path(__file__).resolve().parents[2];out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
base='23b4ddb669f02dbeb780a87a997522d98c7f426c'
archive=subprocess.check_output(['git','-C',str(root),'archive',base,'source','tests/program_material.cpp'])
baseline=out/'baseline';baseline.mkdir(exist_ok=True)
with tarfile.open(fileobj=io.BytesIO(archive)) as t:t.extractall(baseline,filter='data')
def quote(path):return '"'+str(path).replace('\\','/')+'"'
cmake=f'''cmake_minimum_required(VERSION 3.25)
project(ProgramReference LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory({quote(a.sdk_root)} sdk)
include_directories({quote(a.sdk_root)} {quote(a.hiir_root.parent)})
'''
body=r'''
#include <fstream>
int main(int argc,char**argv){
 if(argc!=4)return 2;std::vector<double> input;
 if(std::string(argv[1])=="synthetic")input=makeProgram();
 else{std::ifstream f(argv[1],std::ios::binary|std::ios::ate);auto n=f.tellg();if(n<=0||n%8)return 3;input.resize(size_t(n)/8);f.seekg(0);f.read(reinterpret_cast<char*>(input.data()),n);if(!f)return 4;}
 double scale=std::stod(argv[3]);for(auto& x:input)x*=scale;
 const auto output=render(false,input);std::ofstream f(argv[2],std::ios::binary);f.write(reinterpret_cast<const char*>(output.data()),output.size()*sizeof(double));return f?0:1;
}
'''
for name,checkout in [('baseline',baseline),('current',root)]:
 renderer=out/(name+'.cpp');renderer.write_text('#define main original_program_main\n#include '+quote(checkout/'tests/program_material.cpp')+'\n#undef main\n'+body)
 cmake+=f'add_executable({name}-render {quote(renderer)} {quote(checkout/"source/processor.cpp")} {quote(a.sdk_root/"public.sdk/source/vst/hosting/parameterchanges.cpp")})\ntarget_link_libraries({name}-render PRIVATE sdk)\n'
(out/'CMakeLists.txt').write_text(cmake)
subprocess.run(['cmake','-S',str(out),'-B',str(out/'build'),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release'],check=True)
subprocess.run(['cmake','--build',str(out/'build'),'--target','baseline-render','current-render','--parallel','4'],check=True)
fixture=a.sdk_root/'public.sdk/samples/vst/again_auv3/Shared/drumLoop.wav'
expected='7e3fd3a093fcc4b94418664adac487781e402ed26e75fef9a5046476a0f38522'
if hashlib.sha256(fixture.read_bytes()).hexdigest()!=expected:raise SystemExit('SDK fixture hash mismatch')
with wave.open(str(fixture)) as w:
 if (w.getnchannels(),w.getsampwidth(),w.getframerate())!=(2,2,44100):raise SystemExit('Unexpected fixture format')
 frames=w.getnframes();v=struct.unpack('<'+'h'*(frames*2),w.readframes(frames));x=[(v[2*i]+v[2*i+1])/65536 for i in range(frames)]
y=[]
for i in range(int(len(x)*48000/44100)):
 pos=i*44100/48000;j=int(pos);weight=pos-j;y.append(x[j]*(1-weight)+x[min(j+1,len(x)-1)]*weight)
converted=out/'drum-mono-48k.bin';converted.write_bytes(struct.pack('<'+'d'*len(y),*y))
rows=[];ok=True
for label,input_path,scale in [('synthetic','synthetic',1.),*[("sdk-drum",str(converted),s) for s in (.25,.5,1.,2.)]]:
 blobs=[]
 for name in ['baseline','current']:
  path=out/f'{label}-{name}-{scale}.bin';subprocess.run([str(out/'build'/(name+'-render')),input_path,str(path),str(scale)],check=True);blobs.append(path.read_bytes())
 values=struct.unpack('<'+'d'*(len(blobs[1])//8),blobs[1]);finite=all(math.isfinite(x) for x in values);equal=blobs[0]==blobs[1];ok=ok and finite and equal
 rows.append(dict(fixture=label,scale=scale,frames=len(values),bit_exact=equal,finite=finite,peak=max(map(abs,values)),rms=math.sqrt(sum(x*x for x in values)/len(values)),sha256=hashlib.sha256(blobs[1]).hexdigest()))
report=dict(baseline=base,audit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),sdk_fixture_sha256=expected,results=rows)
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));raise SystemExit(0 if ok else 1)
