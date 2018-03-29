import datetime

import sys

append = sys.argv[1]

d = datetime.datetime.now()
version_str = d.strftime('%y.%m.%d') + '.' + append
open('VERSION', 'w').write(version_str)

def run(cmd):
    import os
    os.system(' '.join(cmd))

run(['python', 'setup.py', 'sdist', 'upload', '-r', 'pypi'])
run(['git', 'commit', '-a', '-m', '"' + version_str + '"'])
run(['git', 'push'])
run(['git', 'tag', version_str])
run(['git', 'push', 'origin', version_str])
