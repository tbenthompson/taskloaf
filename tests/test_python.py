import taskloaf

def test_launch():
    ctx = taskloaf.launch_local(1)
    def return1():
        print("INTASK")
        return 1
    print("READY")
    fut1 = taskloaf.ready(2)
    print(fut1.get())
    print("BEFORE")
    fut = taskloaf.task(return1)
    print("TASKED")
    result = fut.get()
    print("GETTED")
    print(result)
    assert(result == 1)

if __name__ == '__main__':
    test_launch()
