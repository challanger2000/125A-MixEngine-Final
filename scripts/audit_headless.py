#!/usr/bin/env python3
"""Build repository DSP/processor diagnostics with the pinned SDK, without a GUI.

This does not build a native VST3 or establish Windows/DAW/editor readiness.
It reads the repository target list and builds the actual sources unchanged.
"""
import argparse
from pathlib import Path
import re
import subprocess

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk-root',type=Path,required=True)
p.add_argument('--hiir-root',type=Path,required=True)
p.add_argument('--build-root',type=Path,required=True)
p.add_argument('--cmake',default='cmake')
p.add_argument('--ninja',default='ninja')
p.add_argument('--parallel',type=int,default=4)
a=p.parse_args()
root=Path(__file__).resolve().parents[1]
sdk=a.sdk_root.resolve();hiir=a.hiir_root.resolve();build=a.build_root.resolve()
for path in (sdk/'CMakeLists.txt',hiir/'Upsampler2xF64Fpu.h'):
    if not path.is_file():raise SystemExit(f'Missing dependency: {path}')
def q(path):return '"'+str(path).replace('\\','/')+'"'
cmake=f'''cmake_minimum_required(VERSION 3.25)
project(MixEngineHeadlessAudit LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory({q(sdk)} sdk)
include_directories({q(root/'source')} {q(sdk)} {q(hiir.parent)})
enable_testing()
'''
common=[root/'source/processor.cpp',sdk/'public.sdk/source/vst/hosting/parameterchanges.cpp',sdk/'public.sdk/source/common/memorystream.cpp']
for name,channel in [('audit_processor',False),('audit_channel',True)]:
    cmake+=f'add_library({name} STATIC '+ ' '.join(map(q,common))+')\n'
    cmake+=f'target_link_libraries({name} PUBLIC sdk)\n'
    if channel:cmake+=f'target_compile_definitions({name} PUBLIC MIXENGINE_CHANNEL_BUILD=1)\n'
source=(root/'CMakeLists.txt').read_text()
targets=re.findall(r'add_executable\(([^ ]+) (tests/[^ ]+\.cpp)([^\n]*)',source)
for name,test,rest in targets:
    if name in ('controller-lifecycle-diagnostic','audit-linux-editor'):continue
    cmake+=f'add_executable({name} {q(root/test)})\n'
    if 'source/processor.cpp' in rest:
        library='audit_channel' if name in ('audit-channel-contract','automation-curve-channel-contract') else 'audit_processor'
        cmake+=f'target_link_libraries({name} PRIVATE {library})\n'
    cmake+=f'add_test(NAME {name} COMMAND {name})\n'
generated=build/'harness';generated.mkdir(parents=True,exist_ok=True)
(generated/'CMakeLists.txt').write_text(cmake)
subprocess.run([a.cmake,'-S',str(generated),'-B',str(build/'build'),'-G','Ninja',
               f'-DCMAKE_MAKE_PROGRAM={a.ninja}','-DCMAKE_BUILD_TYPE=Release'],check=True)
subprocess.run([a.cmake,'--build',str(build/'build'),'--parallel',str(a.parallel)],check=True)
count=sum(name not in ('controller-lifecycle-diagnostic','audit-linux-editor') for name,_,_ in targets)
print(f'Built {count} headless targets; controller/editor/native VST3 checks remain separate.')
