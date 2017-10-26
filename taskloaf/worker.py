import time

run = None
tasks = []
services = dict()

def shutdown():
    global run
    run = False

def submit_task(t):
    tasks.append(t)

def task_loop(t):
    global tasks, run

    run = True
    submit_task(t)

    n_tasks = 0
    start = time.time()

    while run:
        if len(tasks) == 0:
            continue
        tasks.pop()()
        n_tasks += 1

    run_time = time.time() - start
    # print('ran ' + str(n_tasks) + ' tasks in ' + str(run_time) + ' secs.')

def comm_poll(c):
    t = c.recv()
    tasks.append(lambda: comm_poll(c))
    if t is not None:
        tasks.append(t)

def start_comm(c):
    services['comm'] = c
    comm_poll(c)
