from timeit import default_timer as get_time

def ignore_args(*args, **kwargs):
    pass

class Timer(object):
    def __init__(self, output_fnc = print):
        if output_fnc is None:
            output_fnc = ignore_args
        self.output_fnc = output_fnc
        self.restart()

    def restart(self):
        self.start = get_time()

    def report(self, name, should_restart = True):
        text += name + " took " + str(get_time() - self.start)
        self.output_fnc(text)
        if should_restart:
            self.restart()
