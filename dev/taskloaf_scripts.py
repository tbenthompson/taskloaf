import os

def get_n_cores():
    try:
        import multiprocessing
        return multiprocessing.cpu_count()
    except (ImportError, NotImplementedError):
        return 1

def build_dir(name):
    return os.path.join('build', name)
