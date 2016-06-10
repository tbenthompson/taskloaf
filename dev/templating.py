import mako.template

tmpl = mako.template.Template(filename = "src/taskloaf/closure.tcpp")
src = tmpl.render()
open('src/taskloaf/closure.cpp', 'w').write(src)
