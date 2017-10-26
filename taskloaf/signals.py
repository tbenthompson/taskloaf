from taskloaf.worker import get_service, submit_task

def signal(x):
    reg = get_service('signals_registry')
    if x in reg:
        for t in reg[x]:
            print('signalled ' + str(x) + ' running ' + str(t))
            submit_task(t)
    reg[x] = 0

def set_trigger(x, t):
    reg = get_service('signals_registry')
    if x in reg:
        if reg[x] == 0:
            print('already signalled ' + str(x) + ' running ' + str(t))
            submit_task(t)
        else:
            reg.append(t)
    else:
        reg[x] = [t]
