import os
import subprocess

def benchmark(cmd, indicator_string, max_ms, env_mod = None):
    env = os.environ.copy()
    if env_mod is not None:
        for k, v in env_mod.iteritems():
            env[k] = v
    p = subprocess.Popen(cmd, stdout = subprocess.PIPE, env = env)
    p.wait()
    lines = p.stdout.readlines()
    l = filter(lambda x: indicator_string in x, lines)[0]
    time_ms = int(l[len(indicator_string):(l.find('ms'))])
    assert(time_ms < max_ms)
    print(cmd[0] + ' - ms: ' + str(time_ms))

benchmark(['./build/fib', '45', '30', '6'], 'run took', 300)

benchmark(
    ['./build/cholesky', '4096', '32', '6', 'false'],
    'Taskloaf took',
    300,
    env_mod = dict(OMP_NUM_THREADS = '1')
)


