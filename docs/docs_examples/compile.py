import subprocess
import sys
import os

examples = []
for fname in os.listdir('.'):
    base, ext = os.path.splitext(fname)
    if ext == '.cpp':
        examples.append(base)

open('.gitignore', 'w').write("".join([e + "\n" for e in examples]))

compile_me = examples
if len(sys.argv) > 1:
    compile_me = [sys.argv[1]]

ps = []
for e in compile_me:
    print("Compiling " + str(e))
    cmd = [
        'g++', '-o', e, e + '.cpp', '--std=c++1y', '-I../../src/',
        '-I../../lib', '-L../../build', '-Wl,-rpath,../../build', '-ltaskloaf'
    ];
    print(' '.join(cmd))
    ps.append((e, subprocess.Popen(cmd)))

for p in ps:
    p[1].wait()
    print("Done compiling " + p[0])
