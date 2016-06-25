import taskloaf

def test_ready():
    ctx = taskloaf.launch_local(1)
    fut1 = taskloaf.ready(2)
    assert(fut1.get() == 2)

def test_task():
    ctx = taskloaf.launch_local(1)
    def return1():
        return 1
    fut = taskloaf.task(return1)
    assert(fut.get() == 1)

def test_then():
    ctx = taskloaf.launch_local(1)
    assert(taskloaf.ready(2).then(lambda x: x + 1).get() == 3)

def test_unwrap():
    ctx = taskloaf.launch_local(1)
    def func():
        return taskloaf.task(lambda: 10)
    assert(taskloaf.task(func).unwrap().get() == 10)
