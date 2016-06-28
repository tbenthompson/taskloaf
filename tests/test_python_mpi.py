import taskloaf

def test_fib():
    def fib(i):
        if i < 3:
            return taskloaf.ready(1)
        else:
            return (fib(i - 1)
                .then(lambda a: fib(i - 2).then(lambda b: a + b))
                .unwrap())

    ctx = taskloaf.launch_mpi()
    assert(fib(20).get() == 6765)

def fnc():
    return 10

def test_send_task():
    ctx = taskloaf.launch_mpi()
    assert(taskloaf.task(1, fnc).get() == 10)

if __name__ == '__main__':
    test_fib()
