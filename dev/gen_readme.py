import mako

from mako.template import Template
d = dict()
d['cpp_readme'] = open('examples/readme.cpp', 'r').read()
d['python_readme'] = open('taskloaf/readme.py', 'r').read()
for f in os.listdir('docs/docs_examples'):
    print(f)

open('README.md', 'w').write(Template(filename = 'README.md.tmpl').render(**d))
