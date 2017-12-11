profiler = None

def set_profiler(p):
    global profiler
    profiler = p
    import sys
    PY3 = sys.version_info[0] == 3
    if PY3:
        import builtins
    else:
        import __builtin__ as builtins
    builtins.__dict__['profile'] = p

def null_profiler():
    def wrapper(f):
        return f
    set_profiler(wrapper)

def enable_profiling():
    import line_profiler
    set_profiler(line_profiler.LineProfiler())

def profile_stats():
    try:
        profiler.print_stats()
    except:
        pass

null_profiler()
