from timeit import default_timer as get_time

def ignore_args(*args, **kwargs):
    pass

class Timer(object):
    def __init__(self, prefix = '', tabs = 0, output_fnc = print):
        self.prefix = prefix
        self.tabs = tabs
        if output_fnc is None:
            output_fnc = ignore_args
        self.output_fnc = output_fnc
        self.restart()

    def restart(self):
        self.start = get_time()

    def report(self, name, should_restart = True):
        text = '    ' * self.tabs + self.prefix + ' '
        text += name + " took " + str(get_time() - self.start)
        self.output_fnc(text)
        if should_restart:
            self.restart()
