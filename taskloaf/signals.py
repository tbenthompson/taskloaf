import taskloaf.worker

def signal(x):
    signals_registry = taskloaf.worker.services['signals_registry']
    if x in signals_registry:
        for t in signals_registry[x]:
            print('signalled ' + str(x) + ' running ' + str(t))
            taskloaf.worker.submit_task(t)
    signals_registry[x] = 0

def set_trigger(x, t):
    signals_registry = taskloaf.worker.services['signals_registry']
    if x in signals_registry:
        if signals_registry[x] == 0:
            print('already signalled ' + str(x) + ' running ' + str(t))
            taskloaf.worker.submit_task(t)
        else:
            signals_registry.append(t)
    signals_registry[x] = [t]
