from taskloaf.worker import submit_task, local_task, shutdown, get_service
from taskloaf.signals import signal, set_trigger
from taskloaf.launch import launch

x = 3

def task1(i):
    print('task1: ' + str(i))
    if i == 0:
        signal(1)
    else:
        local_task(lambda: task1(i - 1))

def task2(i):
    print('task2: ' + str(i))
    if i == x:
        signal(2)
    else:
        local_task(lambda: task2(i + 1))

def submit():
    submit_task(0, lambda: task1(x))
    submit_task(1, lambda: task2(0))

    def shutter():
        print('shutter running on ' + str(get_service('comm').addr))
        submit_task(0, shutdown)
        shutdown()

    def trigger2():
        print('trigger2 running on ' + str(get_service('comm').addr))
        submit_task(1, lambda: set_trigger(2, shutter))

    def trigger_setup():
        print("SET!")
        set_trigger(1, trigger2)
    submit_task(0, trigger_setup)


if __name__ == "__main__":
    launch(2, submit)
