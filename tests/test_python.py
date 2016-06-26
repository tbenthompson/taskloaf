import taskloaf
import random

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

def test_fib():
    def fib(i):
        if i < 3:
            return taskloaf.ready(1)
        else:
            return (fib(i - 1)
                .then(lambda a: fib(i - 2).then(lambda b: a + b))
                .unwrap())

    ctx = taskloaf.launch_local(1)
    assert(fib(20).get() == 6765)



def test_tree():
    ctx = taskloaf.launch_local(1)

    def empty_tree():
        return (None, None, 0)

    def insert(tree, val):
        def insert_helper(node):
            if val < node[2]:
                if node[0] is not None:
                    return (node[0].then(insert_helper), node[1], node[2])
                else:
                    return (taskloaf.ready((None,None,val)), node[1], node[2])
            else:
                if node[1] is not None:
                    return (node[0], node[1].then(insert_helper), node[2])
                else:
                    return (node[0], taskloaf.ready((None,None,val)), node[2])
        return tree.then(insert_helper)

    def concat(first_str, second_str):
        return (first_str
            .then(lambda s1: second_str.then(lambda s2: s1 + s2))
            .unwrap())

    def print_tree(tree):
        def print_helper(t, indent):
            out = taskloaf.ready('    ' * indent + str(t[2]) + '\n')
            if t[0] != None:
                out = concat(out, t[0].then(lambda t: print_helper(t, indent + 1)).unwrap())
            if t[1] != None:
                out = concat(out, t[1].then(lambda t: print_helper(t, indent + 1)).unwrap())
            return out
        return tree.then(lambda t: print_helper(t, 0)).unwrap()

    t = taskloaf.task(empty_tree)
    values = [random.randint(-10000, 10000) for i in range(10)]
    for v in values:
        t = insert(t, v)
    # print("")
    # print("")
    # print("")
    print(print_tree(t).get())
